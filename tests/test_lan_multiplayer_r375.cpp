#include "net/coop_rules.hpp"
#include "net/lan_protocol.hpp"
#include "net/lan_transport.hpp"
#include "game/game.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
#include <thread>
#include <chrono>
#include <unistd.h>

using namespace airxonix;
int main(){
    static_assert(coop::ProtocolVersion>=3, "r377 sends extra authoritative world state");
    Game host;host.startCoopSession();
    auto s=host.makeCoopSnapshot(1);
    s.players[0].worldX=.471f;s.players[0].worldZ=.449f;
    s.players[1].worldX=.518f;s.players[1].worldZ=.472f;
    Game guest;guest.startCoopSession();
    assert(guest.applyCoopSnapshot(s));
    assert(std::fabs(guest.player().worldX()-.471f)<.000001f);
    assert(std::fabs(guest.player().worldZ()-.449f)<.000001f);
    assert(std::fabs(guest.secondPlayer().worldX()-.518f)<.000001f);
    assert(std::fabs(guest.secondPlayer().worldZ()-.472f)<.000001f);
    const auto encoded=lan::encodeSnapshot(s);
    lan::Snapshot read;assert(lan::decodeSnapshot(encoded,read));
    s.players[1].worldX=std::numeric_limits<float>::quiet_NaN();
    assert(!lan::decodeSnapshot(lan::encodeSnapshot(s),read));
    s.players[1].worldX=.518f;

    lan::TcpSession server,client;
    std::string err;
    const auto port=std::uint16_t(34000+(::getpid()%15000));
    assert(server.open(lan::Role::Host,"",port,err));
    assert(client.open(lan::Role::Client,"127.0.0.1",port,err));
    // Before both HELLO frames are processed, the game must stay frozen.
    assert(!server.connected()); assert(!client.connected());
    for(int i=0;i<2000&&!server.connected();++i){server.poll();client.poll();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    assert(server.connected()&&client.connected());

    // A stagnant Wi-Fi receiver must NOT cause 200 copies of the field to
    // accumulate in our userspace socket queue. Keep only in-flight + latest.
    for(std::uint32_t sequence=1;sequence<=200;++sequence){
        s.sequence=sequence;
        assert(server.sendSnapshot(s));
    }
    assert(server.queuedBytes()<=2*(lan::MaxPayload+12));
    assert(server.queuedBytes()>=encoded.size());
    // Drive TCP until latest snapshot wins. The peer should get at most a
    // couple of complete snapshots rather than an ancient backlog.
    bool gotLatest=false;
    for(int i=0;i<4000;++i){
        server.poll();client.poll();
        lan::Snapshot readBack;
        if(client.takeSnapshot(readBack)&&readBack.sequence==200)gotLatest=true;
        if(gotLatest)break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    assert(gotLatest);
    // Drop/disconnect must not expose the last session's held inputs.
    client.close();server.poll();
    assert(!client.connected());
    lan::InputPacket packet;
    assert(!client.takeInput(packet));
    server.close();
    std::puts("PASS r375: hello handshake, bounded snapshot queue, latest snapshot, sub-cell rendering, reject NaN");
}
