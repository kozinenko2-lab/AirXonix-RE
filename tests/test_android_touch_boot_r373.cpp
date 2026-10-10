#include "platform/android_virtual_input.hpp"
#include "platform/input.hpp"
#include "game/game.hpp"
#include "game/legacy_menu.hpp"
#include <cassert>
#include <cmath>
#include <iostream>

struct GameTestProbe { static void enter(Game& g) {g.enterMainMenu();} };

// Only AndroidVirtualInput is permitted to construct the Android input frame.
// Synthetic/raw SDL joystick state is deliberately excluded by platform code.
int main() {
    AndroidVirtualInput bridge;
    Game g;
    GameTestProbe::enter(g);
    std::uint32_t ticks=0;
    auto step=[&] {
        const auto phase=g.phase();
        const bool menu=phase==GamePhase::MainMenu || phase==GamePhase::ModeSelect;
        bool ready=true;
        if(phase==GamePhase::MainMenu) {
            const auto& m=g.mainMenu();
            ready=std::fabs(m.selectorOffset-kLegacyMainMenuSelectorSlideTrace.targetFor(m.selected))<1.e-7f && !m.inputLatched;
        } else if (phase==GamePhase::ModeSelect) {
            const auto& m=g.modeSelect();
            ready=m.readyForInput && !m.inputLatched && !m.exitPending;
        }
        const auto f=bridge.poll(menu,ready,static_cast<unsigned>(phase),ticks);
        InputState s{};
        s.up=(f.bits&AndroidVirtualInput::UP)!=0;
        s.down=(f.bits&AndroidVirtualInput::DOWN)!=0;
        s.left=(f.bits&AndroidVirtualInput::LEFT)!=0;
        s.right=(f.bits&AndroidVirtualInput::RIGHT)!=0;
        s.action=(f.bits&AndroidVirtualInput::ENTER)!=0;
        s.back=(f.bits&AndroidVirtualInput::ESCAPE)!=0;
        s.legacyPressedCode=f.virtualKey;
        g.update(s,16);
        ticks+=16;
    };
    // 5 seconds without touching: never auto-launch mode selector.
    for(int i=0;i<315;++i){step(); assert(g.phase()==GamePhase::MainMenu);}
    auto tap=[&](unsigned key){bridge.event(key,true);bridge.event(key,false);};
    tap(AndroidVirtualInput::ENTER);
    for(int i=0;i<85;++i)step();
    assert(g.phase()==GamePhase::ModeSelect);
    assert(g.modeSelect().readyForInput);
    // Holding no controls after Enter must not select a difficulty or start gameplay.
    for(int i=0;i<130;++i){step();assert(g.phase()==GamePhase::ModeSelect);}
    const int mode=g.modeSelect().selected;
    tap(AndroidVirtualInput::DOWN);
    for(int i=0;i<10;++i)step();
    assert(g.modeSelect().selected==mode+1);
    tap(AndroidVirtualInput::UP);
    for(int i=0;i<10;++i)step();
    assert(g.modeSelect().selected==mode);
    tap(AndroidVirtualInput::ESCAPE);
    for(int i=0;i<70;++i)step();
    assert(g.phase()==GamePhase::MainMenu);
    for(int i=0;i<130;++i){step(); assert(g.phase()==GamePhase::MainMenu);}
    std::cout << "r373: idle startup stays main menu; touch ENTER/DOWN/UP/ESCAPE transitions PASS\n";
}
