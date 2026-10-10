// Two actual TCP peers on localhost, using the same protocol and game core as
// two handhelds, including START from red, snapshot propagation and reconnect.
#include "net/lan_transport.hpp"
#include "game/game.hpp"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <thread>
#include <unistd.h>
using namespace airxonix;
using Clock=std::chrono::steady_clock;
static void pump(lan::TcpSession& host,lan::TcpSession& guest){
    host.poll();guest.poll();std::this_thread::sleep_for(std::chrono::milliseconds(2));
}
static bool handshake(lan::TcpSession& a,lan::TcpSession& b){
    const auto until=Clock::now()+std::chrono::seconds(2);
    while(Clock::now()<until){pump(a,b);if(a.connected()&&b.connected())return true;}
    return false;
}
static bool exchange(lan::TcpSession& s,lan::TcpSession& c,Game& server,Game& client,
                     std::uint32_t& seq,bool redPause=false){
    lan::InputPacket input{++seq,redPause?std::uint8_t(64):std::uint8_t(0),client.coopSessionEpoch(),client.coopDeathSerial()};
    if(!c.sendInput(input))return false;
    lan::InputPacket received{};
    const auto until=Clock::now()+std::chrono::seconds(2);
    while(Clock::now()<until){pump(s,c);if(s.takeInput(received))break;}
    if(received.sequence!=input.sequence)return false;
    InputState blue{},red{};red.pause=(received.buttons&64)!=0;
    server.acknowledgeCoopDeaths(received.ackEpoch,received.ackDeathSerial);
    server.updateCoop(blue,red,16);
    if(!s.sendSnapshot(server.makeCoopSnapshot(seq)))return false;
    lan::Snapshot snap{};
    while(Clock::now()<until){pump(s,c);if(c.takeSnapshot(snap))return client.applyCoopSnapshot(snap);}
    return false;
}
int main(){
    Game host,guest;host.startCoopSession();guest.startCoopSession();
    lan::TcpSession server,client;std::string error;
    const std::uint16_t port=std::uint16_t(29000+(::getpid()%15000));
    assert(server.open(lan::Role::Host,"",port,error));
    assert(client.open(lan::Role::Client,"127.0.0.1",port,error));
    assert(handshake(server,client));
    std::uint32_t sequence=0;
    assert(exchange(server,client,host,guest,sequence));
    assert(host.lives()==guest.lives());
    // The initial 1.5 sec transition is host-driven and fully frozen.
    for(int i=0;i<20;++i)host.updateCoop(InputState{},InputState{},100);
    assert(exchange(server,client,host,guest,sequence,true));
    assert(host.paused()&&guest.paused());
    assert(exchange(server,client,host,guest,sequence,true));
    assert(host.paused()&&guest.paused());
    assert(exchange(server,client,host,guest,sequence,false));
    assert(exchange(server,client,host,guest,sequence,true));
    assert(!host.paused()&&!guest.paused());
    // Real TCP drop/rejoin: host listener survives and same game epoch remains.
    client.close();
    for(int i=0;i<30&&server.connected();++i){server.poll();std::this_thread::sleep_for(std::chrono::milliseconds(2));}
    assert(!server.connected()&&server.listenReady());
    lan::TcpSession another;
    assert(another.open(lan::Role::Client,"127.0.0.1",port,error));
    assert(handshake(server,another));
    sequence=0; // local sequence number resets after reconnect
    assert(exchange(server,another,host,guest,sequence));
    assert(host.coopSessionEpoch()==guest.coopSessionEpoch());
    assert(host.lives()==guest.lives()&&host.score()==guest.score());
    assert(host.paused()==guest.paused());
    std::puts("PASS: two real TCP peers, guest START, held pause, drop, rejoin, live authoritative state");
}
