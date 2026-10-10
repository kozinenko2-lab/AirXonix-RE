// Independent POSIX/TCP test: only lan_transport.cpp is needed, no SDL/GLES.
#include "net/lan_transport.hpp"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <thread>
#include <unistd.h>
using namespace airxonix;
using Clock=std::chrono::steady_clock;
static void tick(lan::TcpSession& a,lan::TcpSession& b){
    a.poll();b.poll();
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
}
int main(){
    lan::TcpSession server,guest;
    std::string error;
    const auto port=std::uint16_t(36000+(::getpid()%20000));
    assert(server.open(lan::Role::Host,"",port,error));
    assert(guest.open(lan::Role::Client,"127.0.0.1",port,error));
    auto deadline=Clock::now()+std::chrono::seconds(2);
    while(Clock::now()<deadline && (!server.connected()||!guest.connected()))tick(server,guest);
    assert(server.connected() && guest.connected());
    assert(guest.sendInput({1,8}));
    lan::InputPacket received{};
    deadline=Clock::now()+std::chrono::seconds(2);
    bool gotInput=false;
    while(Clock::now()<deadline&&!gotInput){
        tick(server,guest);
        gotInput=server.takeInput(received);
    }
    assert(gotInput && received.buttons==8);

    // Server still sends snapshots; the guest continues receiving, but the
    // guest radio is simulated as silent (no more input frames to the host).
    // A TCP socket would otherwise remain open indefinitely in this case.
    lan::Snapshot snap{};
    const auto started=Clock::now();
    deadline=started+std::chrono::seconds(6);
    std::uint32_t seq=0;
    while(Clock::now()<deadline && server.connected()){
        snap.sequence=++seq;
        assert(server.sendSnapshot(snap));
        tick(server,guest);
        lan::Snapshot ignored;
        guest.takeSnapshot(ignored);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    assert(!server.connected());
    assert(server.listenReady()); // Host still waits for another console.
    assert(server.error().find("receive timeout")!=std::string::npos);
    assert(Clock::now()-started<std::chrono::seconds(6));
    lan::InputPacket stale{};
    assert(!server.takeInput(stale)); // Held key from previous link is gone.

    guest.close();
    lan::TcpSession rejoined;
    assert(rejoined.open(lan::Role::Client,"127.0.0.1",port,error));
    deadline=Clock::now()+std::chrono::seconds(2);
    while(Clock::now()<deadline && (!server.connected()||!rejoined.connected()))tick(server,rejoined);
    assert(server.connected() && rejoined.connected());
    assert(rejoined.sendInput({1,2})); // Sequence resets for a fresh peer.
    bool gotNew=false;
    deadline=Clock::now()+std::chrono::seconds(2);
    while(Clock::now()<deadline&&!gotNew){
        tick(server,rejoined);
        gotNew=server.takeInput(received);
    }
    assert(gotNew && received.buttons==2 && received.sequence==1);
    std::puts("PASS r376: TCP receive timeout, listener survives, stale input cleared, rejoin");
}
