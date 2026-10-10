#include "net/coop_rules.hpp"
#include "net/lan_protocol.hpp"
#include "net/lan_transport.hpp"
#include "game/game.hpp"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <thread>
#include <unistd.h>
using namespace airxonix;
struct GameTestProbe {
    static void coopKill(Game& game,coop::Owner which){game.coopKill(which,"unit-test");}
};
int main(){
    static_assert(coop::BlueSpawn.x!=coop::RedSpawn.x,"spawns must be separate");
    assert(coop::color(coop::Owner::Blue).b>coop::color(coop::Owner::Blue).r);
    assert(coop::color(coop::Owner::Red).r>coop::color(coop::Owner::Red).b);
    coop::TrailOwners own;own.assign(10,10,coop::Owner::Blue);own.assign(42,21,coop::Owner::Red);
    assert(own.hitByEnemy(10,10)==coop::Owner::Blue);
    assert(own.hitByEnemy(42,21)==coop::Owner::Red);
    own.clear();assert(own.at(10,10)==coop::Owner::None);
    coop::SharedResources shared;shared.score=800;shared.timeMs=120000;
    shared.die(coop::Owner::Blue);shared.die(coop::Owner::Red);
    assert(shared.lives==1&&shared.score==800&&shared.deathSerial==2&&shared.mostRecentVictim==coop::Owner::Red);

    Game host;host.startCoopSession();
    assert(host.coopEnabled());
    assert(host.player().x()==coop::BlueSpawn.x && host.secondPlayer().x()==coop::RedSpawn.x);
    assert(host.lives()==3);
    GameTestProbe::coopKill(host,coop::Owner::Red);
    assert(host.lives()==2 && host.lastCoopVictim()==coop::Owner::Red);
    assert(host.secondPlayer().x()==coop::RedSpawn.x);
    GameTestProbe::coopKill(host,coop::Owner::Blue);
    assert(host.lives()==1 && host.lastCoopVictim()==coop::Owner::Blue);
    assert(host.player().x()==coop::BlueSpawn.x);
    assert(host.coopDeathSerial()==2);

    InputState neutral{};host.updateCoop(neutral,neutral,16);
    lan::Snapshot s=host.makeCoopSnapshot(3);
    auto encoded=lan::encodeSnapshot(s);assert(encoded.size()<lan::MaxPayload);
    lan::Snapshot decoded;assert(lan::decodeSnapshot(encoded,decoded));
    assert(decoded.sequence==3&&decoded.field==s.field&&decoded.trailOwners==s.trailOwners);
    assert(decoded.players[0].x!=decoded.players[1].x);
    assert(decoded.lives==s.lives && decoded.timeMs==s.timeMs && decoded.air.size()==s.air.size());
    Game replica;replica.startCoopSession();assert(replica.applyCoopSnapshot(decoded));
    assert(replica.player().x()==host.player().x());
    assert(replica.secondPlayer().x()==host.secondPlayer().x());
    assert(replica.field().cells()==host.field().cells());

    auto inputData=lan::encodeInput({255u,0x0f});
    lan::InputPacket in;assert(lan::decodeInput(inputData,in)&&in.sequence==255&&in.buttons==0x0f);
    inputData.push_back(0xff);assert(!lan::decodeInput(inputData,in));
    auto truncated=encoded;truncated.resize(35);assert(!lan::decodeSnapshot(truncated,decoded));
    auto packet=lan::frame(lan::Kind::Snapshot,encoded);
    assert(packet.size()==encoded.size()+12);
    assert(packet[0]=='A'&&packet[1]=='X'&&packet[2]=='M'&&packet[3]=='P');

    // Real localhost TCP integration, not simulated network memory queues.
    lan::TcpSession server,client;
    std::string error;
    const auto port=std::uint16_t(32000+(::getpid()%20000));
    assert(server.open(lan::Role::Host,"",port,error));
    assert(client.open(lan::Role::Client,"127.0.0.1",port,error));
    bool receivedInput=false,receivedSnapshot=false;
    for(int cycle=0;cycle<4500;++cycle){
        server.poll();client.poll();
        if(server.connected())client.sendInput({42u,3u});
        if(client.connected())server.sendSnapshot(s);
        lan::InputPacket inp;
        if(server.takeInput(inp)){
            assert(inp.sequence==42&&inp.buttons==3);receivedInput=true;
        }
        lan::Snapshot snap;
        if(client.takeSnapshot(snap)){
            assert(snap.sequence==3 && snap.field==s.field && snap.score==s.score);receivedSnapshot=true;
        }
        if(receivedInput&&receivedSnapshot)break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    assert(receivedInput&&receivedSnapshot);
    client.close();server.poll();server.close();
    std::printf("PASS: two spawns, trail owners, shared resources, snapshot, malformed packet, localhost TCP both directions\n");
    return 0;
}
