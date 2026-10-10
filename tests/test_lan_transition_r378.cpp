// r378: host-controlled level change, 1500ms freeze, reconnect and shared HUD.
#include "game/game.hpp"
#include "net/lan_protocol.hpp"
#include "render/legacy_hud.hpp"
#include <cassert>
#include <cstdio>
#include <vector>

struct GameTestProbe {
    static void beginNextLevel(Game& g){g.coopBeginNextLevel();}
    static void kill(Game& g,airxonix::coop::Owner o){g.coopKill(o,"r378 test");}
};

int main(){
    using namespace airxonix;
    static_assert(coop::ProtocolVersion>=4,"level freeze requires new wire format");
    Game host;host.startCoopSession();
    Game guest;guest.startCoopSession();
    auto snapshot=host.makeCoopSnapshot(1);
    assert(guest.applyCoopSnapshot(snapshot));
    assert(host.levelIndex()==0 && guest.levelIndex()==0);
    const int beforeLives=host.lives();
    GameTestProbe::beginNextLevel(host);
    assert(host.levelIndex()==1 && host.coopTransitionRemainingMs()==1500);
    snapshot=host.makeCoopSnapshot(2);
    assert(snapshot.transitionMs==1500 && snapshot.level==1);
    auto encoded=lan::encodeSnapshot(snapshot);
    lan::Snapshot decoded{};
    assert(lan::decodeSnapshot(encoded,decoded));
    assert(guest.applyCoopSnapshot(decoded));
    assert(guest.levelIndex()==1 && guest.coopTransitionRemainingMs()==1500);
    assert(guest.lives()==beforeLives);
    assert(guest.timeRemaining()==host.timeRemaining());

    // HUD transition is visible on both machines and not on single-player.
    LegacyHudState hud{};hud.coopEnabled=true;hud.coopTransitionMs=host.coopTransitionRemainingMs();hud.levelNumber=2;
    const auto overlayHud=LegacyHud::compose(hud);
    hud.coopEnabled=false;
    assert(overlayHud.size()==LegacyHud::compose(hud).size()+7); // LEVEL 2

    InputState idle{};
    const int frozenTimer=host.timeRemaining();
    const auto frozenPlayer=host.player().x();
    for(int i=0;i<14;++i){
        host.updateCoop(idle,idle,100);
        assert(host.timeRemaining()==frozenTimer);
        assert(host.player().x()==frozenPlayer);
    }
    assert(host.coopTransitionRemainingMs()==100);
    snapshot=host.makeCoopSnapshot(3);
    assert(guest.applyCoopSnapshot(snapshot));
    assert(guest.coopTransitionRemainingMs()==100);
    host.updateCoop(idle,idle,100);
    assert(host.coopTransitionRemainingMs()==0 && host.timeRemaining()==frozenTimer);
    host.updateCoop(idle,idle,16);
    assert(host.timeRemaining()==frozenTimer-16);

    // Joining a running level mid-transition must not restart the countdown.
    Game rejoined;rejoined.startCoopSession();
    assert(rejoined.applyCoopSnapshot(snapshot));
    assert(rejoined.levelIndex()==1 && rejoined.coopTransitionRemainingMs()==100);

    // Death overlay is shown for RED on host and client once per death serial.
    snapshot=host.makeCoopSnapshot(4);
    assert(guest.applyCoopSnapshot(snapshot));
    GameTestProbe::kill(host,coop::Owner::Red);
    assert(host.coopDeathNoticeRemainingMs()==2200);
    snapshot=host.makeCoopSnapshot(5);
    assert(guest.applyCoopSnapshot(snapshot));
    assert(guest.lastCoopVictim()==coop::Owner::Red);
    assert(guest.coopDeathNoticeRemainingMs()==2200);
    guest.updateCoopPresentation(100);
    assert(guest.coopDeathNoticeRemainingMs()==2100);
    assert(guest.applyCoopSnapshot(snapshot));
    assert(guest.coopDeathNoticeRemainingMs()==2100);
    hud={};hud.coopEnabled=true;hud.coopVictim=2;hud.coopDeathNoticeMs=2100;
    const auto redHud=LegacyHud::compose(hud);
    hud.coopEnabled=false;
    assert(redHud.size()==LegacyHud::compose(hud).size()+14); // RED XONIX DOWN
    // Death notice must also survive a shared last-life GameOver screen.
    hud.screen=LegacyHudScreen::GameOver;
    hud.coopEnabled=true;
    assert(LegacyHud::compose(hud).size()>14); // r379 adds co-op result text
    // Presentation ticks cap at 100ms for lag spikes; advance in steps.
    for(int i=0;i<30;++i)guest.updateCoopPresentation(100);
    assert(guest.coopDeathNoticeRemainingMs()==0);
    hud.coopDeathNoticeMs=0;hud.coopEnabled=true;
    assert(!LegacyHud::compose(hud).empty()); // r379: result panel stays visible after notice fades
    hud.coopEnabled=false;
    assert(LegacyHud::compose(hud).empty());

    // Packet validator refuses forged oversized transition lengths.
    snapshot.transitionMs=1501;
    assert(!lan::decodeSnapshot(lan::encodeSnapshot(snapshot),decoded));
    std::puts("PASS r378: level serial, frozen countdown, reconnect, HUD, death, malformed transition");
}
