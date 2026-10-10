#pragma once
#include <array>
#include <cstddef>
#include <cstdint>


// Keep the gameplay-facing input state independent from SDL. The native
// platform implementation includes SDL only in input.cpp, which lets the
// reconstructed game core be compiled/tested without any platform SDK.
struct _SDL_GameController;
struct _SDL_Joystick;

struct InputState {
    static constexpr std::size_t LegacyCodeCount=0x11C;
    bool quit = false;
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;
    bool action = false;
    bool back = false;
    bool pause = false;
    bool select = false; // PortMaster Back/Select; reserved for native diagnostics chords
    // Original AirXonix key namespace. 0x00..0xFF are Win32 VK-like codes;
    // 0x100..0x11B are J1/J2 directions/buttons from table 0x00440A40.
    std::array<bool,LegacyCodeCount> legacyHeld{};
    int legacyPressedCode = -1; // one edge event for modal remapping
    // r248: source tag prevents controller J1/D-Pad edge codes from being
    // mistaken for PC keyboard bytes by the post-game name editor.
    bool legacyPressedFromController = false;
    // r243: actual typed text byte converted from SDL UTF-8 to Windows-1251.
    // This is separate from legacyPressedCode because SDLK_a is a VK-like 'A'
    // regardless of Shift/case/layout, while 0x40F160 consumes typed bytes.
    int legacyTextByte = -1;
    bool legacyDown(int code) const {
        return code>=0 && static_cast<std::size_t>(code)<legacyHeld.size() && legacyHeld[static_cast<std::size_t>(code)];
    }
};

// Android touchscreen sends the *same Win32-compatible virtual-key codes*
// as the SDL keyboard path, not the J1 gamepad namespace (0x100..0x11B).
// The game core uses both continuous held state and one-shot key edges.
// Keep this SDK-independent to exercise the whole translation in tests.
struct AndroidTouchKeyboard {
    static constexpr unsigned Up=1u, Down=2u, Left=4u, Right=8u;
    static constexpr unsigned Enter=16u, Escape=32u, Pause=64u;
    static constexpr unsigned Directions=Up|Down|Left|Right;

    static void apply(InputState& s, unsigned active, unsigned downEdges, bool repeatDirections) {
        auto held=[&](unsigned bit,int vk) {
            if(active&bit) s.legacyHeld[static_cast<std::size_t>(vk)]=true;
        };
        // A quick DOWN+UP can arrive entirely between SDL polls. That event
        // must be visible to Game::update via its ordinary input booleans,
        // not ONLY legacyPressedCode (which mode selection ignores).
        const unsigned pressed = active | downEdges;
        s.up    |= (pressed&Up)!=0;   held(Up,0x26);
        s.down  |= (pressed&Down)!=0; held(Down,0x28);
        s.left  |= (pressed&Left)!=0; held(Left,0x25);
        s.right |= (pressed&Right)!=0;held(Right,0x27);
        s.action|= (pressed&Enter)!=0;held(Enter,0x0D);
        s.back  |= (pressed&Escape)!=0;held(Escape,0x1B);
        s.pause |= (pressed&Pause)!=0;held(Pause,0x50);

        // SDL_KEYDOWN legacyPressedCode is an edge; menu stick auto-repeat
        // supplies additional separate virtual arrow key-downs.
        unsigned event=downEdges;
        if(repeatDirections) event |= active&Directions;
        const int key = (event&Enter)?0x0D : (event&Escape)?0x1B :
                        (event&Up)?0x26 : (event&Down)?0x28 :
                        (event&Left)?0x25 : (event&Right)?0x27 :
                        (event&Pause)?0x50 : -1;
        if(key>=0) {
            s.legacyPressedCode=key;
            s.legacyPressedFromController=false;
        }
    }
};

class InputSystem {
public:
    InputSystem();
    ~InputSystem();
    void poll(InputState& state);
    void setMenuTouchNavigation(bool enabled, bool ready=true, unsigned screen=0) {
        menuTouchNavigation_=enabled;
        menuTouchReady_=ready;
        touchScreen_=screen;
    }
    void setRecordNameTextInput(bool enabled); // Android: show IME ONLY in high-score name editor
    bool rumble(float strength01,std::uint32_t durationMs);
private:
    _SDL_GameController* controller_ = nullptr;
    _SDL_Joystick* joystick_ = nullptr;
    bool ownsJoystick_ = false;
    int lastLoggedHat_ = -1;
    int lastLoggedAxisX_ = 0x7fffffff;
    int lastLoggedAxisY_ = 0x7fffffff;
    unsigned loggedButtonMask_ = 0;
    int lastCardinalDirection_ = 0; // 1 up, 2 down, 3 left, 4 right
    unsigned lastRawHatState_ = 0;
    bool preferRawDirections_ = false;
    bool menuTouchNavigation_ = false;
    bool menuTouchReady_ = true;
    unsigned touchScreen_ = 0;

    void closeDevice();
    void openFirstInputDevice();
};
