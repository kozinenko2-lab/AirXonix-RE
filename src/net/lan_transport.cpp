#include "lan_transport.hpp"
#if !defined(_WIN32)
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#else
#error "LAN multiplayer requires POSIX sockets on the target H700 Linux systems"
#endif
namespace airxonix::lan {
namespace {
void nonblocking(int fd){int f=fcntl(fd,F_GETFL,0);if(f>=0)fcntl(fd,F_SETFL,f|O_NONBLOCK);}
std::uint32_t get32(const Byte* b){return (std::uint32_t(b[0])<<24)|(std::uint32_t(b[1])<<16)|(std::uint32_t(b[2])<<8)|b[3];}
}
TcpSession::~TcpSession(){close();}
void TcpSession::close(){if(fd_>=0)::close(fd_);if(listener_>=0)::close(listener_);fd_=listener_=-1;connecting_=connected_=ready_=helloSent_=helloReceived_=false;incoming_.clear();outgoing_.clear();pendingInput_.clear();pendingSnapshot_.clear();inputReady_=snapshotReady_=false;seenInputSequence_=seenSnapshotSequence_=false;lastRxAt_={};}
void TcpSession::drop(const std::string& why){error_=why;if(fd_>=0)::close(fd_);fd_=-1;connected_=connecting_=ready_=helloSent_=helloReceived_=false;incoming_.clear();outgoing_.clear();pendingInput_.clear();pendingSnapshot_.clear();inputReady_=snapshotReady_=false;seenInputSequence_=seenSnapshotSequence_=false;lastRxAt_={};}
bool TcpSession::open(Role role,const std::string& address,std::uint16_t port,std::string& error){
    close();role_=role;error_.clear();error.clear();
    int s=::socket(AF_INET,SOCK_STREAM,0);if(s<0){error="socket failed";return false;}
    int yes=1;setsockopt(s,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));
    sockaddr_in to{};to.sin_family=AF_INET;to.sin_port=htons(port);
    if(role==Role::Host){to.sin_addr.s_addr=htonl(INADDR_ANY);
        if(::bind(s,reinterpret_cast<sockaddr*>(&to),sizeof(to))!=0||::listen(s,1)!=0){error=std::string("bind/listen failed: ")+std::strerror(errno);::close(s);return false;}
        nonblocking(s);listener_=s;return true;
    }
    if(inet_pton(AF_INET,address.c_str(),&to.sin_addr)!=1){error="Invalid IPv4 host address";::close(s);return false;}
    nonblocking(s);const int res=::connect(s,reinterpret_cast<sockaddr*>(&to),sizeof(to));
    if(res<0&&errno!=EINPROGRESS){error=std::string("connect failed: ")+std::strerror(errno);::close(s);return false;}
    fd_=s;connecting_=res<0;connected_=res==0;
    lastRxAt_=std::chrono::steady_clock::now();
    return true;
}
bool TcpSession::enqueue(Kind kind,const std::vector<Byte>& body){
    if(!connected() || body.size()>MaxPayload)return false;
    auto f=frame(kind,body);if(f.empty())return false;
    // An in-flight TCP frame cannot be truncated/replaced. A frame waiting
    // *behind* it can be replaced safely: only the latest input or snapshot
    // matters, since all complete snapshots contain the entire world state.
    if(outgoing_.empty())outgoing_=std::move(f);
    else if(kind==Kind::Snapshot)pendingSnapshot_=std::move(f);
    else if(kind==Kind::Input)pendingInput_=std::move(f);
    else return false;
    return true;
}
bool TcpSession::sendInput(const InputPacket& x){return enqueue(Kind::Input,encodeInput(x));}
bool TcpSession::sendSnapshot(const Snapshot& x){return enqueue(Kind::Snapshot,encodeSnapshot(x));}
bool TcpSession::takeInput(InputPacket& x){if(!inputReady_)return false;x=lastInput_;inputReady_=false;return true;}
bool TcpSession::takeSnapshot(Snapshot& x){if(!snapshotReady_)return false;x=std::move(lastSnapshot_);snapshotReady_=false;return true;}
void TcpSession::poll(){
    if(fd_<0&&listener_>=0){
        sockaddr_in addr{};socklen_t n=sizeof(addr);
        const int s=::accept(listener_,reinterpret_cast<sockaddr*>(&addr),&n);
        if(s>=0){error_.clear();fd_=s;nonblocking(fd_);int one=1;setsockopt(fd_,IPPROTO_TCP,TCP_NODELAY,&one,sizeof(one));connected_=true;ready_=helloSent_=helloReceived_=false;incoming_.clear();outgoing_.clear();pendingInput_.clear();pendingSnapshot_.clear();inputReady_=snapshotReady_=false;seenInputSequence_=seenSnapshotSequence_=false;lastRxAt_=std::chrono::steady_clock::now();}
    }
    if(fd_<0)return;
    // Do not depend on TCP FIN/RST: an unplugged radio can leave the socket
    // logically connected indefinitely and consume the shared game timer.
    if(std::chrono::steady_clock::now()-lastRxAt_>ReceiveTimeout){
        drop("receive timeout (Wi-Fi peer silent)");return;
    }
    if(connecting_){int err=0;socklen_t n=sizeof(err);
        if(getsockopt(fd_,SOL_SOCKET,SO_ERROR,&err,&n)!=0){drop("connect status error");return;}
        if(err==EINPROGRESS||err==EALREADY)return;
        if(err!=0){drop(std::string("connect rejected: ")+std::strerror(err));return;}
        connecting_=false;connected_=true;
    }
    if(!connected_)return;
    if(!helloSent_){
        // Hello is sent before any game data and must be verified in both
        // directions before the application can report a connected session.
        Writer w;w.u16(coop::ProtocolVersion);w.u32(Magic);
        outgoing_=frame(Kind::Hello,w.data);helloSent_=true;
    }
    if(outgoing_.empty()){
        if(!pendingInput_.empty())outgoing_.swap(pendingInput_);
        else if(!pendingSnapshot_.empty())outgoing_.swap(pendingSnapshot_);
    }
    if(!outgoing_.empty()){
        const ssize_t n=::send(fd_,outgoing_.data(),outgoing_.size(),MSG_NOSIGNAL);
        if(n>0){
            outgoing_.erase(outgoing_.begin(),outgoing_.begin()+n);
            if(outgoing_.empty()){
                if(!pendingInput_.empty())outgoing_.swap(pendingInput_);
                else if(!pendingSnapshot_.empty())outgoing_.swap(pendingSnapshot_);
            }
        }
        else if(n<0&&errno!=EAGAIN&&errno!=EWOULDBLOCK&&errno!=EINTR){drop("send disconnected");return;}
    }
    Byte scratch[16384];
    for(int tries=0;tries<4;++tries){const ssize_t n=::recv(fd_,scratch,sizeof(scratch),0);
        if(n>0){lastRxAt_=std::chrono::steady_clock::now();incoming_.insert(incoming_.end(),scratch,scratch+n);if(incoming_.size()>MaxPayload*4){drop("buffer limit exceeded");return;}}
        else if(n==0){drop("peer disconnected");return;}
        else if(errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR)break;
        else {drop("recv disconnected");return;}
    }
    while(incoming_.size()>=12){
        if(get32(incoming_.data())!=Magic){drop("wrong magic");return;}
        const auto version=std::uint16_t(incoming_[4]<<8|incoming_[5]);
        const Byte kind=incoming_[6];const std::uint32_t length=get32(incoming_.data()+8);
        if(version!=coop::ProtocolVersion||length>MaxPayload||incoming_[7]){drop("incompatible protocol");return;}
        if(incoming_.size()<12+length)break;
        std::vector<Byte> body(incoming_.begin()+12,incoming_.begin()+12+length);
        incoming_.erase(incoming_.begin(),incoming_.begin()+12+length);
        if(kind==Byte(Kind::Hello)){Reader r(body.data(),body.size());std::uint16_t v=0;std::uint32_t magic=0;if(!r.u16(v)||!r.u32(magic)||!r.done()||v!=coop::ProtocolVersion||magic!=Magic||helloReceived_){drop("invalid hello");return;}
            helloReceived_=true;ready_=helloSent_&&helloReceived_;}
        else if(kind==Byte(Kind::Input)){
            InputPacket x;if(!ready_||!decodeInput(body,x)){drop("unexpected/invalid input");return;}
            if(!seenInputSequence_||std::int32_t(x.sequence-lastInputSequence_)>0){
                lastInput_=x;inputReady_=true;lastInputSequence_=x.sequence;seenInputSequence_=true;
            }
            ++messages_;
        }
        else if(kind==Byte(Kind::Snapshot)){
            Snapshot x;if(!ready_||!decodeSnapshot(body,x)){drop("unexpected/invalid snapshot");return;}
            if(!seenSnapshotSequence_||std::int32_t(x.sequence-lastSnapshotSequence_)>0){
                lastSnapshotSequence_=x.sequence;seenSnapshotSequence_=true;
                lastSnapshot_=std::move(x);snapshotReady_=true;
            }
            ++messages_;
        }
        else {drop("unknown message type");return;}
    }
}
} // namespace airxonix::lan
