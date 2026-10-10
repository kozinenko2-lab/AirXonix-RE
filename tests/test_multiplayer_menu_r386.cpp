#include "game/game.hpp"
#include "render/legacy_hud.hpp"
#include <cassert>
#include <cstdio>
struct GameTestProbe {static void enterMainMenu(Game& g){g.enterMainMenu();}};
static void press(Game& game,bool down=false,bool action=false,bool back=false){
    InputState i{};i.down=down;i.action=action;i.back=back;
    game.update(i,16);
    for(int k=0;k<12;++k)game.update(InputState{},16);
}
int main(){
    Game game;GameTestProbe::enterMainMenu(game);
    assert(game.phase()==GamePhase::MainMenu);
    for(int i=0;i<4;++i)press(game,true);
    assert(game.mainMenu().selected==4);
    press(game,false,true);
    assert(game.phase()==GamePhase::MultiplayerMenu);
    assert(game.multiplayerMenu().selected==0);
    press(game,false,true);
    assert(game.takeMultiplayerMenuAction()==MultiplayerMenuAction::Host);
    assert(game.multiplayerMenu().busy);
    press(game,false,false,true);
    assert(game.takeMultiplayerMenuAction()==MultiplayerMenuAction::Cancel);
    assert(!game.multiplayerMenu().busy);
    press(game,true);
    assert(game.multiplayerMenu().selected==1);
    press(game,false,true);
    assert(game.takeMultiplayerMenuAction()==MultiplayerMenuAction::Join);
    assert(game.multiplayerMenu().busy);
    press(game,false,false,true);
    assert(game.takeMultiplayerMenuAction()==MultiplayerMenuAction::Cancel);
    press(game,true);
    press(game,false,true);
    assert(game.phase()==GamePhase::MainMenu);
    LegacyHudState hud{};hud.screen=LegacyHudScreen::Multiplayer;
    hud.multiplayerSelected=1;
    hud.multiplayerLine1="SEARCHING FOR HOST";
    const auto sprites=LegacyHud::compose(hud);
    int textured=0;
    for(const auto& s:sprites)if(s.atlas==LegacyHudAtlas::MenuMP)++textured;
    assert(textured==4);
    for(const auto& sp:sprites)if(sp.atlas==LegacyHudAtlas::MenuMP){
        assert(sp.v0>=0.f && sp.v1>sp.v0 && sp.v1<=1.f);
    }
    LegacyHudState options{};
    options.screen=LegacyHudScreen::Settings;
    options.settingsSelected=6;
    const auto settingsSprites=LegacyHud::compose(options);
    int retroSettingPlates=0;
    for(const auto& sp:settingsSprites)if(sp.atlas==LegacyHudAtlas::MenuMP){
        assert(sp.v0>=.5f && sp.v1<=1.f);++retroSettingPlates;
    }
    // r389 adds a third graphics plate.
    assert(retroSettingPlates==3);
    game.startCoopSession(0);
    assert(game.coopEnabled());
    game.endCoopSessionToMenu();
    assert(!game.coopEnabled() && game.phase()==GamePhase::MainMenu);
    std::puts("PASS: six-row menu, multiplayer Host/Join/Cancel/Back, return to menu, 4 clean-room graphics strips");
}
