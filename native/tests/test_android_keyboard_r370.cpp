#include <cassert>
#include "platform/input.hpp"

int main() {
    struct Test { unsigned bit;int vk; bool InputState::*field; } cases[] = {
        {1,0x26,&InputState::up}, {2,0x28,&InputState::down},
        {4,0x25,&InputState::left}, {8,0x27,&InputState::right},
        {16,0x0D,&InputState::action}, {32,0x1B,&InputState::back},
        {64,0x50,&InputState::pause}
    };
    for (auto c: cases) {
        InputState in{};
        AndroidTouchKeyboard::apply(in,c.bit,c.bit,false);
        assert(in.*(c.field));
        assert(in.legacyDown(c.vk));
        assert(in.legacyPressedCode==c.vk);
        assert(!in.legacyPressedFromController);
        assert(!in.legacyDown(0x104) && !in.legacyDown(0x105));
    }
    // Tap entirely between SDL frames: key edge survives release.
    {InputState in{};AndroidTouchKeyboard::apply(in,0,16,false);
        assert(in.legacyPressedCode==0x0D);assert(in.action);}
    {InputState in{};AndroidTouchKeyboard::apply(in,0,32,false);
        assert(in.legacyPressedCode==0x1B);assert(in.back);}
    // Game ModeSelect: sustained keys stay down and must not emit
    // repeated SDL_KEYDOWN edges (unlike joystick menu autorepeat).
    {InputState in{};AndroidTouchKeyboard::apply(in,2,0,false);
        assert(in.down && in.legacyDown(0x28));assert(in.legacyPressedCode==-1);}
    {InputState in{};AndroidTouchKeyboard::apply(in,8,0,true);
        assert(in.right && in.legacyPressedCode==0x27);}
    // A/B win over the directional edge for the one-shot key code, but
    // arrow held-state is still visible (simultaneous multitouch).
    {InputState in{};AndroidTouchKeyboard::apply(in,2|16,2|16,true);
        assert(in.legacyPressedCode==0x0D && in.down && in.action);}
    return 0;
}
