#pragma once
#include "lan_protocol.hpp"
#include <cstddef>
#include <chrono>
#include <string>
#include <vector>

namespace airxonix::lan {
enum class Role {Host,Client};
class TcpSession {
public:
    TcpSession()=default;
    ~TcpSession();
    TcpSession(const TcpSession&)=delete;
    TcpSession& operator=(const TcpSession&)=delete;
    bool open(Role role,const std::string& address,std::uint16_t port,std::string& error);
    void poll();
    // True only after mutually verified HELLO; raw TCP availability is private.
    bool connected() const{return connected_&&ready_;}
    bool listenReady() const{return listener_>=0;}
    bool sendInput(const InputPacket& in);
    bool sendSnapshot(const Snapshot& in);
    bool takeInput(InputPacket& out);
    bool takeSnapshot(Snapshot& out);
    void close();
    std::string error() const{return error_;}
    std::uint64_t receivedCount() const{return messages_;}
    std::size_t queuedBytes() const{return outgoing_.size()+pendingInput_.size()+pendingSnapshot_.size();}
private:
    bool enqueue(Kind kind,const std::vector<Byte>& body);
    void drop(const std::string& reason);
    int listener_=-1,fd_=-1;
    bool connecting_=false,connected_=false,ready_=false,helloSent_=false,helloReceived_=false;
    Role role_=Role::Host;
    std::string error_;
    // Only one wire frame in flight. Snapshots/inputs awaiting send are
    // replaced by fresher data, preventing Wi-Fi lag from growing unbounded.
    std::vector<Byte> incoming_, outgoing_,pendingInput_,pendingSnapshot_;
    InputPacket lastInput_{};
    Snapshot lastSnapshot_{};
    bool inputReady_=false,snapshotReady_=false;
    std::uint64_t messages_=0;
    std::uint32_t lastInputSequence_=0,lastSnapshotSequence_=0;
    bool seenInputSequence_=false,seenSnapshotSequence_=false;
    // TCP can stay ESTABLISHED when Wi-Fi disappears. Detect silence using a
    // monotonic clock rather than SO_KEEPALIVE (whose defaults take hours).
    std::chrono::steady_clock::time_point lastRxAt_{};
    static constexpr auto ReceiveTimeout=std::chrono::milliseconds(3500);
};
} // namespace airxonix::lan
