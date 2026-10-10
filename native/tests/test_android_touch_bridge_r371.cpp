#include "game/game.hpp"
#include "platform/input.hpp"
#include "platform/android_touch_menu.hpp"
#include "game/legacy_menu.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
struct GameTestProbe {static void enter(Game& g){g.enterMainMenu();}};

static void tick(Game& g, AndroidTouchMenu& adapter, unsigned mask, unsigned edges, unsigned now) {
    const auto phase=g.phase();
    bool menu = phase==GamePhase::MainMenu||phase==GamePhase::ModeSelect;
    bool ready=true;
    if(phase==GamePhase::MainMenu){
        const auto& m=g.mainMenu();
        ready=std::fabs(m.selectorOffset-kLegacyMainMenuSelectorSlideTrace.targetFor(m.selected))<1e-7f && !m.inputLatched;
    }else if(phase==GamePhase::ModeSelect){
        const auto& m=g.modeSelect();
        ready=m.readyForInput && !m.inputLatched && !m.exitPending;
    }
    const auto converted=adapter.frame(mask,edges,menu,now,ready,static_cast<unsigned>(phase));
    InputState in{};AndroidTouchKeyboard::apply(in,converted,0,false);
    g.update(in,16);
}
int main(){
    // Regression: r370 lost fast touch taps. Their VK edge was set, but the
    // game actually checks action/back and never sees legacyPressedCode here.
    for(unsigned b : {16u,32u,1u,2u,4u,8u}){
        InputState i{};AndroidTouchKeyboard::apply(i,0,b,false);
        if(b==16)assert(i.action);
        if(b==32)assert(i.back);
        if(b==1) assert(i.up);
        if(b==2) assert(i.down);
        if(b==4) assert(i.left);
        if(b==8) assert(i.right);
    }
    // Scene 1: downward menu selection, rapid upward swipe DURING animation
    // must queue, and both A & B need to work even if DOWN+UP lasted < 1 frame.
    Game game;GameTestProbe::enter(game);AndroidTouchMenu adapter;
    unsigned now=0;
    auto frame=[&](unsigned mask,unsigned edge){tick(game,adapter,mask,edge,now);now+=16;};
    frame(0,0);frame(0,2);assert(game.mainMenu().selected==1);
    frame(0,1); // UP tap arrives while the selector is still moving
    for(int i=0;i<35;++i)frame(0,0);
    assert(game.mainMenu().selected==0 && "UP must not be lost during DOWN animation");
    frame(0,16); // A fast tap enters difficulty screen
    for(int i=0;i<25 && game.phase()!=GamePhase::ModeSelect;++i)frame(0,0);
    assert(game.phase()==GamePhase::ModeSelect && "A tap must open difficulty");
    for(int i=0;i<55;++i)frame(0,0);
    assert(game.modeSelect().readyForInput);
    const int oldMode=game.modeSelect().selected;
    frame(0,2); // DOWN: difficulty +1
    for(int i=0;i<4;++i)frame(0,0);
    assert(game.modeSelect().selected==oldMode+1);
    frame(0,1); // UP: difficulty -1
    for(int i=0;i<4;++i)frame(0,0);
    assert(game.modeSelect().selected==oldMode);
    frame(0,32); // B fast tap returns to main menu
    for(int i=0;i<60 && game.phase()==GamePhase::ModeSelect;++i)frame(0,0);
    assert(game.phase()==GamePhase::MainMenu);
    std::cout<<"Android touchscreen bridge r371 full MainMenu/ModeSelect A/B/UP/DOWN PASS\n";
}
