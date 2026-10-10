#include "platform/android_virtual_input.hpp"
#include "platform/input.hpp"
#include "game/game.hpp"
#include "game/legacy_menu.hpp"
#include <cassert>
#include <cmath>
#include <iostream>

struct GameTestProbe {static void enter(Game& g){g.enterMainMenu();}};
using V=AndroidVirtualInput;
static InputState get(V& v,unsigned phase,bool menu,bool ready,unsigned& clock){
    auto t=v.poll(menu,ready,phase,clock);clock+=16;
    InputState s{};
    auto b=t.bits;
    s.up=b&V::UP; s.down=b&V::DOWN;s.left=b&V::LEFT;s.right=b&V::RIGHT;
    s.action=b&V::ENTER;s.back=b&V::ESCAPE;s.pause=b&V::PAUSE;
    s.legacyPressedCode=t.virtualKey;
    if(t.virtualKey>=0)s.legacyHeld[t.virtualKey]=true;
    return s;
}
int main(){
    V v;unsigned ms=0;
    get(v,1,true,true,ms);  // initialize scene
    for(auto [key,vk] : {std::pair{V::UP,0x26}, {V::DOWN,0x28},
                          {V::LEFT,0x25}, {V::RIGHT,0x27},
                          {V::ENTER,0x0D}, {V::ESCAPE,0x1B}}){
        v.event(key,true);v.event(key,false); // <1 frame
        auto a=get(v,1,true,true,ms);assert(a.legacyPressedCode==vk);
        auto released=get(v,1,true,true,ms);assert(released.legacyPressedCode==-1);
    }
    // A and B are distinct events even if fired in the same frame.
    v.event(V::ENTER,true);v.event(V::ENTER,false);
    v.event(V::ESCAPE,true);v.event(V::ESCAPE,false);
    assert(get(v,1,true,true,ms).action);
    assert(!get(v,1,true,true,ms).action);
    assert(get(v,1,true,true,ms).back);
    get(v,1,true,true,ms);
    // Held upward direction repeats; releasing ends repeat.
    v.event(V::UP,true);
    assert(get(v,1,true,true,ms).up);get(v,1,true,true,ms);
    bool repeated=false;
    for(int i=0;i<35;++i)repeated |= get(v,1,true,true,ms).up;
    assert(repeated);
    v.event(V::UP,false);get(v,1,true,true,ms);
    for(int i=0;i<12;++i)assert(!get(v,1,true,true,ms).up);
    // Fade-in: queue the event without losing it.
    v.event(V::ENTER,true);v.event(V::ENTER,false);
    for(int i=0;i<10;++i)assert(!get(v,1,true,false,ms).action);
    assert(get(v,1,true,true,ms).action);get(v,1,true,true,ms);
    // Switching screens must suppress a held old confirm button.
    v.event(V::ENTER,true);
    assert(get(v,1,true,true,ms).action);
    assert(!get(v,2,true,true,ms).action);
    v.event(V::ENTER,false);
    get(v,2,true,true,ms);
    v.event(V::ENTER,true);v.event(V::ENTER,false);
    assert(get(v,2,true,true,ms).action);get(v,2,true,true,ms);
    // Gameplay uses continuous state, unlike menus.
    get(v,3,false,true,ms); // scene switch with neutral input
    v.event(V::LEFT,true);
    assert(get(v,3,false,true,ms).left);
    assert(get(v,3,false,true,ms).left);
    v.event(V::LEFT,false);
    assert(!get(v,3,false,true,ms).left);
    // Complete real game input: menu and difficult selector read booleans.
    V bridge;Game game;GameTestProbe::enter(game);ms=0;
    auto frame=[&](){
        const auto phase=game.phase();bool menu=phase==GamePhase::MainMenu||phase==GamePhase::ModeSelect;
        bool ready=true;
        if(phase==GamePhase::MainMenu){
            const auto&m=game.mainMenu();
            ready=std::fabs(m.selectorOffset-kLegacyMainMenuSelectorSlideTrace.targetFor(m.selected))<1e-7f&&!m.inputLatched;
        }else if(phase==GamePhase::ModeSelect){
            const auto&m=game.modeSelect();ready=m.readyForInput&&!m.inputLatched&&!m.exitPending;
        }
        auto s=get(bridge,static_cast<unsigned>(phase),menu,ready,ms);
        game.update(s,16);
    };
    frame();
    auto tap=[&](unsigned bit){bridge.event(bit,true);bridge.event(bit,false);};
    tap(V::DOWN);for(int i=0;i<45;++i)frame();assert(game.mainMenu().selected==1);
    tap(V::UP);for(int i=0;i<45;++i)frame();assert(game.mainMenu().selected==0);
    tap(V::ENTER);for(int i=0;i<45;++i)frame();assert(game.phase()==GamePhase::ModeSelect);
    for(int i=0;i<70;++i)frame();assert(game.modeSelect().readyForInput);
    int previous=game.modeSelect().selected;
    tap(V::DOWN);for(int i=0;i<5;++i)frame();assert(game.modeSelect().selected==previous+1);
    tap(V::UP);for(int i=0;i<5;++i)frame();assert(game.modeSelect().selected==previous);
    tap(V::ESCAPE);for(int i=0;i<55;++i)frame();assert(game.phase()==GamePhase::MainMenu);
    std::cout<<"Android r372 full virtual keyboard bridge and real game transitions PASS\n";
}
