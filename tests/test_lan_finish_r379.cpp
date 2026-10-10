// r379: paired host deaths, single wire event, terminal state and finale.
#include "game/game.hpp"
#include "net/lan_protocol.hpp"
#include "render/legacy_hud.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>

struct GameTestProbe {
    static void lives(Game& g,int n){g.lives_=n;}
    static void both(Game& g){g.coopKillMask(3u,"paired collision test");}
    static void markBothDead(Game& g){g.player_.kill();g.secondPlayer_.kill();}
    static void finale(Game& g){g.beginFinalSequence();}
};

static int countDeaths(Game& g){
    int n=0;
    for(const auto& e:g.takeDeathAudioEvents())if(e.logicalId==0x05u)++n;
    return n;
}

int main(){
    using namespace airxonix;
    static_assert(coop::ProtocolVersion>=7,"paired death requires v5");
    InputState idle{};
    for(int lives : {3,2,1}){
        Game host,guest;host.startCoopSession();guest.startCoopSession();
        auto initial=host.makeCoopSnapshot(1);
        assert(guest.applyCoopSnapshot(initial));
        GameTestProbe::lives(host,lives);
        const int startingTime=host.timeRemaining();
        GameTestProbe::both(host);
        assert(host.lives()==std::max(0,lives-2));
        assert(host.coopDeathSerial()==1 && host.coopDeathMask()==3);
        assert(host.lastCoopVictim()==coop::Owner::Red);
        assert(countDeaths(host)==2);
        if(lives<=2)assert(host.phase()==GamePhase::GameOver);
        else assert(host.phase()==GamePhase::Gameplay);
        auto snap=host.makeCoopSnapshot(2);
        assert(snap.deathMask==3 && snap.deathSerial==1);
        auto bytes=lan::encodeSnapshot(snap);
        lan::Snapshot decoded{};
        assert(lan::decodeSnapshot(bytes,decoded));
        assert(guest.applyCoopSnapshot(decoded));
        assert(countDeaths(guest)==2); // client gets both death sounds, once
        assert(guest.applyCoopSnapshot(decoded));
        assert(countDeaths(guest)==0);
        assert(guest.lives()==host.lives() && guest.phase()==host.phase());
        assert(guest.coopDeathMask()==3);
        assert(guest.coopDeathNoticeRemainingMs()==2200);
        LegacyHudState hud{};
        hud.coopEnabled=true;hud.coopVictim=2;hud.coopDeathMask=3;
        hud.coopDeathNoticeMs=2200;
        hud.screen=LegacyHudScreen::GameOver;
        const auto bothOverlay=LegacyHud::compose(hud);
        hud.coopDeathMask=2;
        const auto redOverlay=LegacyHud::compose(hud);
        assert(bothOverlay.size()>redOverlay.size()); // BOTH vs RED label
        hud.coopEnabled=false;
        assert(LegacyHud::compose(hud).empty()); // singleplayer unchanged
        if(host.phase()==GamePhase::GameOver){
            for(int i=0;i<60;++i)host.updateCoop(idle,idle,100);
            assert(host.phase()==GamePhase::GameOver);
            assert(host.timeRemaining()==startingTime);
        }
        // Reject corrupt simultaneous death coordinates and mask.
        snap.otherDeathWorldX=std::numeric_limits<float>::quiet_NaN();
        assert(!lan::decodeSnapshot(lan::encodeSnapshot(snap),decoded));
        snap.otherDeathWorldX=0.5f;
        snap.deathMask=4;
        assert(!lan::decodeSnapshot(lan::encodeSnapshot(snap),decoded));
    }

    // Check the production frame path, not only the isolated death helper.
    Game tickHost;tickHost.startCoopSession();
    GameTestProbe::lives(tickHost,1);
    GameTestProbe::markBothDead(tickHost);
    tickHost.updateCoop(idle,idle,16);
    assert(tickHost.lives()==0 && tickHost.coopDeathMask()==3 && tickHost.coopDeathSerial()==1);
    assert(tickHost.phase()==GamePhase::GameOver);

    // Campaign finale progresses on the host and replicates fixed shared score,
    // phase and host-controlled avatar presentation to the guest.
    Game winner,spectator;winner.startCoopSession();spectator.startCoopSession();
    assert(spectator.applyCoopSnapshot(winner.makeCoopSnapshot(1)));
    GameTestProbe::finale(winner);
    assert(winner.phase()==GamePhase::FinalSequence);
    const int finalScore=winner.score();
    for(int i=0;i<40;++i)winner.updateCoop(idle,idle,100);
    auto middle=winner.makeCoopSnapshot(2);
    assert(middle.finaleElapsedMs==4000 && middle.phase==static_cast<unsigned>(GamePhase::FinalSequence));
    lan::Snapshot received{};
    assert(lan::decodeSnapshot(lan::encodeSnapshot(middle),received));
    assert(spectator.applyCoopSnapshot(received));
    assert(spectator.phase()==GamePhase::FinalSequence);
    assert(spectator.score()==finalScore);
    assert(std::fabs(spectator.finalePresentationY()-middle.finaleY)<1e-5f);
    assert(std::fabs(spectator.finalScene().xonixWorldX-middle.finaleX)<1e-5f);
    assert(std::fabs(spectator.finalScene().cinematicPhase-middle.finaleCinematicPhase)<1e-5f);
    assert(std::fabs(spectator.finalScene().rotorPhase-middle.finaleRotorPhase)<1e-5f);
    for(int i=0;i<40;++i)winner.updateCoop(idle,idle,100);
    assert(winner.phase()==GamePhase::Complete);
    auto end=winner.makeCoopSnapshot(3);
    assert(spectator.applyCoopSnapshot(end));
    assert(spectator.phase()==GamePhase::Complete);
    assert(spectator.score()==winner.score());
    for(int i=0;i<20;++i)winner.updateCoop(idle,idle,100);
    assert(winner.phase()==GamePhase::Complete && winner.score()==finalScore);
    LegacyHudState result{};result.coopEnabled=true;result.screen=LegacyHudScreen::Complete;
    result.score=finalScore;
    assert(!LegacyHud::compose(result).empty());
    result.coopEnabled=false;
    assert(LegacyHud::compose(result).empty());
    std::puts("PASS r379: simultaneous death, terminal GameOver, finale score/phase, malformed snapshots");
}
