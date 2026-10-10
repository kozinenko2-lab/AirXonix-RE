#include "lan_discovery.hpp"
#include "lan_protocol.hpp"
#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

namespace airxonix::lan {
namespace {
constexpr std::uint16_t DiscoveryPort=static_cast<std::uint16_t>(coop::Port+1);
constexpr std::uint32_t DiscoveryMagic=0x41584d44u; // AXMD
constexpr std::uint32_t DiscoveryTag=0x72333836u;   // r386, separate LAN-generation fingerprint
constexpr auto SendInterval=std::chrono::milliseconds(650);

void nonblocking(int fd){const int f=fcntl(fd,F_GETFL,0);if(f>=0)(void)fcntl(fd,F_SETFL,f|O_NONBLOCK);}
void put16(std::uint8_t* b,std::uint16_t n){b[0]=std::uint8_t(n>>8);b[1]=std::uint8_t(n);}
void put32(std::uint8_t* b,std::uint32_t n){for(int i=0;i<4;++i)b[i]=std::uint8_t(n>>(24-i*8));}
std::uint16_t get16(const std::uint8_t* b){return std::uint16_t((std::uint16_t(b[0])<<8)|b[1]);}
std::uint32_t get32(const std::uint8_t* b){return (std::uint32_t(b[0])<<24)|(std::uint32_t(b[1])<<16)|(std::uint32_t(b[2])<<8)|b[3];}
std::vector<in_addr> broadcastAddresses(){
    std::vector<in_addr> targets;
    // Limited broadcast is useful on small hotspot networks, but some wifi
    // interfaces only route their subnet-directed broadcast correctly.
    targets.push_back({htonl(INADDR_BROADCAST)});
    ifaddrs* list=nullptr;
    if(getifaddrs(&list)==0){
        for(ifaddrs* p=list;p;p=p->ifa_next){
            if(!p->ifa_addr || p->ifa_addr->sa_family!=AF_INET ||
               !(p->ifa_flags&IFF_UP) || !(p->ifa_flags&IFF_BROADCAST) || !p->ifa_broadaddr)continue;
            const in_addr a=reinterpret_cast<sockaddr_in*>(p->ifa_broadaddr)->sin_addr;
            bool seen=false;for(const auto& t:targets)if(t.s_addr==a.s_addr)seen=true;
            if(!seen)targets.push_back(a);
        }
        freeifaddrs(list);
    }
    return targets;
}
}

LanDiscovery::~LanDiscovery(){close();}
void LanDiscovery::close(){
    if(fd_>=0)::close(fd_);
    fd_=-1;role_=DiscoveryRole::None;tcpPort_=0;lastBroadcastAt_={};
    discoveredReady_=false;discovered_={};error_.clear();
}
bool LanDiscovery::startHost(std::uint16_t port,std::string& error){
    close();error.clear();
    if(port<1024){error="invalid TCP port";return false;}
    const int fd=::socket(AF_INET,SOCK_DGRAM,0);
    if(fd<0){error=std::string("UDP socket: ")+std::strerror(errno);return false;}
    int yes=1;(void)setsockopt(fd,SOL_SOCKET,SO_BROADCAST,&yes,sizeof(yes));
    nonblocking(fd);fd_=fd;role_=DiscoveryRole::Host;tcpPort_=port;
    return true;
}
bool LanDiscovery::startClient(std::string& error){
    close();error.clear();
    const int fd=::socket(AF_INET,SOCK_DGRAM,0);
    if(fd<0){error=std::string("UDP socket: ")+std::strerror(errno);return false;}
    int yes=1;(void)setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));
    sockaddr_in local{};local.sin_family=AF_INET;local.sin_port=htons(DiscoveryPort);
    local.sin_addr.s_addr=htonl(INADDR_ANY);
    if(::bind(fd,reinterpret_cast<sockaddr*>(&local),sizeof(local))!=0){
        error=std::string("UDP discovery bind: ")+std::strerror(errno);::close(fd);return false;
    }
    nonblocking(fd);fd_=fd;role_=DiscoveryRole::Client;
    return true;
}
void LanDiscovery::poll(){
    if(fd_<0)return;
    const auto now=std::chrono::steady_clock::now();
    if(role_==DiscoveryRole::Host){
        if(lastBroadcastAt_!=std::chrono::steady_clock::time_point{} && now-lastBroadcastAt_<SendInterval)return;
        lastBroadcastAt_=now;
        std::uint8_t beacon[12]{};
        put32(beacon,DiscoveryMagic);put16(beacon+4,coop::ProtocolVersion);
        put16(beacon+6,tcpPort_);put32(beacon+8,DiscoveryTag);
        for(const auto& addr:broadcastAddresses()){
            sockaddr_in target{};target.sin_family=AF_INET;target.sin_addr=addr;target.sin_port=htons(DiscoveryPort);
            (void)::sendto(fd_,beacon,sizeof(beacon),MSG_NOSIGNAL,reinterpret_cast<sockaddr*>(&target),sizeof(target));
        }
        return;
    }
    for(int i=0;i<16;++i){
        std::uint8_t bytes[64]{};sockaddr_in peer{};socklen_t plen=sizeof(peer);
        const ssize_t n=::recvfrom(fd_,bytes,sizeof(bytes),0,reinterpret_cast<sockaddr*>(&peer),&plen);
        if(n<0){
            if(errno!=EINTR && errno!=EAGAIN && errno!=EWOULDBLOCK)error_=std::strerror(errno);
            break;
        }
        if(n!=12 || get32(bytes)!=DiscoveryMagic || get16(bytes+4)!=coop::ProtocolVersion ||
           get32(bytes+8)!=DiscoveryTag || get16(bytes+6)<1024)continue;
        char address[INET_ADDRSTRLEN]{};
        if(!inet_ntop(AF_INET,&peer.sin_addr,address,sizeof(address)))continue;
        discovered_={address,get16(bytes+6)};
        discoveredReady_=true;
        break;
    }
}
bool LanDiscovery::takeDiscoveredHost(DiscoveredHost& out){
    if(!discoveredReady_)return false;
    out=discovered_;discoveredReady_=false;return true;
}
} // namespace airxonix::lan
