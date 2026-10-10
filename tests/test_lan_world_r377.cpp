// r377: one authority, shared pickups, special presentation and death attribution.
#include "game/game.hpp"
#include "game/pickups.hpp"
#include "net/lan_protocol.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>

struct PickupsTestProbe {
    static Pickup& item(Pickups& p,std::size_t i){return p.items_[i];}
};
struct GameTestProbe {
    static void kill(Game& g,airxonix::coop::Owner o){g.coopKill(o,"r377 test");}
};

int main(){
    using namespace airxonix;
    static_assert(coop::ProtocolVersion>=3,"new snapshot requires protocol revision");
    Game host;host.startCoopSession();
    const int beforeTime=host.timeRemaining();
    const int beforeAngle=host.pickups().presentationAngle2048();
    InputState noInput{};
    host.updateCoop(noInput,noInput,16);
    assert(host.timeRemaining()==beforeTime-16);
    assert(host.pickups().presentationAngle2048()==((beforeAngle+32)&2047));

    // A pickup under RED's coordinates must be applied exactly once despite
    // having two collectors in a single frame. BLUE is deliberately distant.
    Pickups pickups;
    Field field;
    Entities entities;
    LegacyRandom rng;
    PickupsTestProbe::item(pickups,0)={0,32,32,.512f,.008f,.510f};
    const PickupCollectorState both[2]={{.43f,.008f,.43f},{.512f,.008f,.510f}};
    auto effects=pickups.updateWithCollectors(16,field,both,2,entities,rng,90000,3);
    assert(effects.size()==1 && effects.front()==PickupEffect::Score1000);
    assert(pickups.presentationAngle2048()==32);
    assert(PickupsTestProbe::item(pickups,0).timerMs>0);

    // Guest sees the host's world view, not its own preseeded random pickups.
    auto snap=host.makeCoopSnapshot(17);
    snap.pickups[0]={750,23,45,.46f,.009f,.55f};
    snap.pickupAngle=922;
    snap.homing={1,.42f,.021f,.53f,1.3f};
    snap.eraser={1,.56f,.48f,2.7f,731};
    auto wire=lan::encodeSnapshot(snap);
    assert(wire.size()<lan::MaxPayload);
    lan::Snapshot decoded{};
    assert(lan::decodeSnapshot(wire,decoded));
    Game guest;guest.startCoopSession();
    assert(guest.applyCoopSnapshot(decoded));
    assert(guest.pickups().items()[0].timerMs==750);
    assert(guest.pickups().items()[0].gridX==23);
    assert(std::fabs(guest.pickups().items()[0].worldZ-.55f)<1.e-6f);
    assert(guest.pickups().presentationAngle2048()==922);
    assert(std::fabs(guest.specialObjects().homing().worldX-.42f)<1.e-6f);
    assert(std::fabs(guest.specialObjects().eraser().worldZ-.48f)<1.e-6f);
    assert(guest.specialObjects().eraser().rotationAngle2048==731);

    // Network input validation must reject NaN and impossible coordinates.
    snap.pickups[0].height=std::numeric_limits<float>::quiet_NaN();
    assert(!lan::decodeSnapshot(lan::encodeSnapshot(snap),decoded));
    snap.pickups[0].height=.009f;
    snap.homing.x=std::numeric_limits<float>::infinity();
    assert(!lan::decodeSnapshot(lan::encodeSnapshot(snap),decoded));
    snap.homing.x=.42f;
    snap.eraser.angle=99999;
    assert(!lan::decodeSnapshot(lan::encodeSnapshot(snap),decoded));
    snap.eraser.angle=731;
    assert(lan::decodeSnapshot(lan::encodeSnapshot(snap),decoded));

    // Death event carries actual impact position, NOT respawn location.
    GameTestProbe::kill(host,coop::Owner::Red);
    snap=host.makeCoopSnapshot(18);
    assert(snap.deathSerial==1 && snap.lastVictim==2);
    assert(lan::decodeSnapshot(lan::encodeSnapshot(snap),decoded));
    assert(guest.applyCoopSnapshot(decoded));
    assert(guest.lastCoopVictim()==coop::Owner::Red);
    assert(guest.coopDeathSerial()==1);
    auto sounds=guest.takeDeathAudioEvents();
    int deaths=0;
    for(const auto& event:sounds){
        if(event.logicalId==0x05u){
            ++deaths;
            assert(std::fabs(event.x-snap.deathWorldX)<1.e-6f);
            assert(std::fabs(event.z-snap.deathWorldZ)<1.e-6f);
        }
    }
    assert(deaths==1);
    assert(guest.applyCoopSnapshot(decoded));
    sounds=guest.takeDeathAudioEvents();
    for(const auto& event:sounds)assert(event.logicalId!=0x05u);
    std::puts("PASS r377: shared pickup once; live bonuses and specials on guest; malformed packets; death sound exactly once");
}
