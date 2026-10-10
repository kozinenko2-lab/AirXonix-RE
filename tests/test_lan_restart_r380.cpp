// r380: authoritative rematch and durable death-event delivery over coalesced snapshots.
#include "game/game.hpp"
#include "net/lan_protocol.hpp"
#include "render/legacy_hud.hpp"
#include <cassert>
#include <cstdio>
#include <limits>

struct GameTestProbe {
    static void kill(Game& g,std::uint8_t mask){g.coopKillMask(mask,"r380 regression");}
    static void lives(Game& g,int lives){g.lives_=lives;}
    static void completed(Game& g){g.phase_=GamePhase::Complete;}
};
static int deaths(Game& g){
    int count=0;
    for(const auto& e:g.takeDeathAudioEvents())if(e.logicalId==0x05u)++count;
    return count;
}
int main(){
    using namespace airxonix;
    static_assert(coop::ProtocolVersion==7);
    Game host,guest;host.startCoopSession();guest.startCoopSession();
    assert(guest.applyCoopSnapshot(host.makeCoopSnapshot(1)));
    assert(guest.coopSessionEpoch()==1 && guest.coopDeathSerial()==0);

    // Three separate authoritative frames happen while receiver is slow.
    GameTestProbe::kill(host,1);
    GameTestProbe::kill(host,2);
    GameTestProbe::kill(host,3);
    assert(host.lives()==0 && host.phase()==GamePhase::GameOver);
    assert(host.coopDeathSerial()==3);
    auto batched=host.makeCoopSnapshot(2);
    assert(batched.deaths.size()==3);
    assert(batched.deaths[0].mask==1 && batched.deaths[1].mask==2 && batched.deaths[2].mask==3);
    lan::Snapshot decoded{};
    assert(lan::decodeSnapshot(lan::encodeSnapshot(batched),decoded));
    assert(guest.applyCoopSnapshot(decoded));
    assert(guest.coopDeathSerial()==3);
    assert(guest.coopDeathMask()==3);
    assert(guest.lives()==0 && guest.phase()==GamePhase::GameOver);
    assert(deaths(guest)==4); // one BLUE, one RED, then two simultaneous deaths
    assert(guest.applyCoopSnapshot(decoded));
    assert(deaths(guest)==0); // duplicated snapshots do not replay audio

    // A guest input acknowledges exactly the contiguous events it applied.
    lan::InputPacket ack{19,0,guest.coopSessionEpoch(),guest.coopDeathSerial()};
    lan::InputPacket ackDecoded{};
    assert(lan::decodeInput(lan::encodeInput(ack),ackDecoded));
    assert(ackDecoded.ackEpoch==1 && ackDecoded.ackDeathSerial==3);
    host.acknowledgeCoopDeaths(ackDecoded.ackEpoch,ackDecoded.ackDeathSerial);
    assert(host.makeCoopSnapshot(3).deaths.empty());
    host.acknowledgeCoopDeaths(1,999); // invalid future acknowledgement ignored
    host.acknowledgeCoopDeaths(2,3); // wrong epoch ignored

    // Game Over is sticky without a new host A press. Guest buttons never restart.
    InputState idle{},a{},redA{};a.action=true;redA.action=true;
    for(int n=0;n<30;++n)host.updateCoop(idle,redA,100);
    assert(host.phase()==GamePhase::GameOver && host.coopSessionEpoch()==1);
    host.updateCoop(a,idle,16);
    assert(host.phase()==GamePhase::Gameplay && host.coopSessionEpoch()==2);
    assert(host.score()==0 && host.coopDeathSerial()==0 && host.coopDeathMask()==0);
    assert(host.coopTransitionRemainingMs()==1500);
    assert(host.makeCoopSnapshot(4).deaths.empty());
    const int timer=host.timeRemaining();
    host.updateCoop(a,idle,100); // held A must not repeat restart
    assert(host.coopSessionEpoch()==2 && host.timeRemaining()==timer);
    auto restarted=host.makeCoopSnapshot(5);
    assert(restarted.epoch==2 && restarted.deathSerial==0);
    assert(guest.applyCoopSnapshot(restarted));
    assert(guest.coopSessionEpoch()==2 && guest.phase()==GamePhase::Gameplay);
    assert(guest.coopDeathSerial()==0 && guest.lives()==host.lives());
    assert(deaths(guest)==0);
    assert(!guest.applyCoopSnapshot(batched)); // delayed old-round packet rejected

    // More pending events than fit in one snapshot are NOT dropped: the host
    // sends the oldest 64, receives cumulative ACK, then sends the rest.
    Game backlogHost,backlogGuest;
    backlogHost.startCoopSession();backlogGuest.startCoopSession();
    assert(backlogGuest.applyCoopSnapshot(backlogHost.makeCoopSnapshot(1)));
    GameTestProbe::lives(backlogHost,100);
    for(int n=0;n<70;++n)GameTestProbe::kill(backlogHost,1);
    assert(backlogHost.coopDeathSerial()==70);
    auto firstBatch=backlogHost.makeCoopSnapshot(2);
    assert(firstBatch.deaths.size()==lan::MaxPendingDeaths && firstBatch.deaths.back().serial==64);
    assert(backlogGuest.applyCoopSnapshot(firstBatch));
    assert(backlogGuest.coopDeathSerial()==64);
    backlogHost.acknowledgeCoopDeaths(backlogGuest.coopSessionEpoch(),backlogGuest.coopDeathSerial());
    auto secondBatch=backlogHost.makeCoopSnapshot(3);
    assert(secondBatch.deaths.size()==6 && secondBatch.deaths.front().serial==65);
    assert(backlogGuest.applyCoopSnapshot(secondBatch));
    assert(backlogGuest.coopDeathSerial()==70);
    assert(deaths(backlogGuest)==70);
    backlogHost.acknowledgeCoopDeaths(backlogGuest.coopSessionEpoch(),backlogGuest.coopDeathSerial());
    assert(backlogHost.makeCoopSnapshot(4).deaths.empty());

    // The host can also choose to replay after the completed finale.
    GameTestProbe::completed(host);
    host.updateCoop(idle,idle,16); // release A
    host.updateCoop(a,idle,16);
    assert(host.coopSessionEpoch()==3 && host.phase()==GamePhase::Gameplay);

    // Malformed / out of order event journals are rejected BEFORE affecting gameplay.
    batched.epoch=3;
    batched.deaths[1].serial=batched.deaths[0].serial;
    assert(!lan::decodeSnapshot(lan::encodeSnapshot(batched),decoded));
    batched.deaths[1].serial=2;
    batched.deaths[0].primaryX=std::numeric_limits<float>::infinity();
    assert(!lan::decodeSnapshot(lan::encodeSnapshot(batched),decoded));
    batched.deaths[0].primaryX=0.5f;
    batched.deaths[2].mask=7;
    assert(!lan::decodeSnapshot(lan::encodeSnapshot(batched),decoded));
    auto badInput=lan::encodeInput(ack);badInput.pop_back();
    assert(!lan::decodeInput(badInput,ackDecoded));
    LegacyHudState hud{};hud.coopEnabled=true;hud.screen=LegacyHudScreen::GameOver;
    assert(LegacyHud::compose(hud).size()>0);
    std::puts("PASS r380: event journal ACK, snapshot coalescing, restart generations, malformed packets");
}
