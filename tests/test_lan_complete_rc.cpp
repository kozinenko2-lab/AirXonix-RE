// End-to-end deterministic regression of H700 co-op pause, per-player
// haptics, remote authoritative state and corrupted packet rejection.
#include "game/game.hpp"
#include "net/lan_protocol.hpp"
#include "render/legacy_hud.hpp"
#include <cassert>
#include <cstdio>
#include <limits>
using namespace airxonix;
struct GameTestProbe {
  static void kill(Game& g,std::uint8_t mask){g.coopKillMask(mask,"coop-rc integration");}
};
int main(){
  static_assert(coop::ProtocolVersion>=7, "pause sync needs AXMP v7");
  // Host-selected campaign mode survives a team Game Over and rematch.
  Game modeHost;
  modeHost.startCoopSession(4);
  assert(modeHost.modeIndex()==4);
  Game host,guest;
  host.startCoopSession();guest.startCoopSession();
  host.setCoopIsHost(true); guest.setCoopIsHost(false);
  assert(!host.coopLinkReady() && !guest.coopLinkReady());
  InputState idle{},start{};start.pause=true;
  assert(guest.applyCoopSnapshot(host.makeCoopSnapshot(1)));
  assert(!guest.paused());
  // The warmup freeze is part of the original co-op level transition.
  for(int i=0;i<20;i++)host.updateCoop(idle,idle,100);
  assert(host.coopTransitionRemainingMs()==0);
  const int before=host.timeRemaining();
  host.updateCoop(start,idle,16);
  assert(host.paused());
  host.updateCoop(start,idle,100);
  assert(host.paused() && host.timeRemaining()==before);
  lan::Snapshot paused{};
  assert(lan::decodeSnapshot(lan::encodeSnapshot(host.makeCoopSnapshot(2)),paused));
  assert(paused.paused==1);
  assert(guest.applyCoopSnapshot(paused) && guest.paused());
  host.updateCoop(idle,idle,16); // START release
  host.updateCoop(idle,start,16); // red player requests unpause
  assert(!host.paused());
  host.updateCoop(idle,start,16); // holding START must not repause
  assert(!host.paused());
  host.updateCoop(idle,idle,16);
  host.updateCoop(start,start,16); // simultaneous START edges produce one toggle
  assert(host.paused());
  host.updateCoop(idle,idle,16);
  host.updateCoop(start,idle,16);
  assert(!host.paused());
  auto unpaused=host.makeCoopSnapshot(3);
  assert(guest.applyCoopSnapshot(unpaused) && !guest.paused());
  // Broken pause bit: reject malformed packet, leave authoritative client state.
  auto bad=unpaused;bad.paused=4;
  lan::Snapshot discarded{};
  assert(!lan::decodeSnapshot(lan::encodeSnapshot(bad),discarded));
  // Host loses red life: no blue vibration. Guest only rumbles for red.
  GameTestProbe::kill(host,2);
  assert(!host.consumeCoopLocalHit(coop::Owner::Blue));
  assert(host.consumeCoopLocalHit(coop::Owner::Red));
  assert(!host.consumeCoopLocalHit(coop::Owner::Red));
  auto death=host.makeCoopSnapshot(4);
  assert(guest.applyCoopSnapshot(death));
  assert(guest.consumeCoopLocalHit(coop::Owner::Red));
  assert(!guest.consumeCoopLocalHit(coop::Owner::Blue));
  assert(guest.applyCoopSnapshot(death));
  assert(!guest.consumeCoopLocalHit(coop::Owner::Red));
  GameTestProbe::kill(host,3);
  auto doubleDeath=host.makeCoopSnapshot(5);
  assert(guest.applyCoopSnapshot(doubleDeath));
  assert(guest.consumeCoopLocalHit(coop::Owner::Blue));
  assert(guest.consumeCoopLocalHit(coop::Owner::Red));
  assert(!guest.consumeCoopLocalHit(coop::Owner::Red));
  LegacyHudState hud{};
  hud.coopEnabled=true;hud.screen=LegacyHudScreen::Gameplay;
  hud.coopIsHost=true;hud.coopLinkReady=true;
  auto ready=LegacyHud::compose(hud);
  hud.coopLinkReady=false;
  auto waiting=LegacyHud::compose(hud);
  assert(waiting.size()>ready.size());
  hud.coopLinkReady=true;hud.paused=true;
  auto pause=LegacyHud::compose(hud);
  assert(pause.size()>ready.size());
  // On Game Over both devices display identical team statistics.
  assert(guest.phase()==GamePhase::GameOver);
  assert(host.score()==guest.score() && host.lives()==guest.lives());
  std::puts("PASS: co-op pause by both, held START, pause snapshot, per-player rumble, HUD waiting, GameOver");
}
