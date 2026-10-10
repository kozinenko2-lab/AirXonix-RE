#pragma once
#include <cstdint>

// r371: edge-preserving Android-to-game input adapter. A/B and direction
// changes are deferred while the original game's animation/fade latches
// disallow navigation. A quick tap must never be lost between SDL frames.
// Gameplay remains continuous and does not acquire menu-specific repeats.
class AndroidTouchMenu {
public:
    static constexpr unsigned kDirections=0x0fu;
    static constexpr unsigned kButtons=0x70u;
    static constexpr std::uint32_t kInitialRepeatMs=340;
    static constexpr std::uint32_t kRepeatMs=145;

    unsigned frame(unsigned held, unsigned edges, bool menu, std::uint32_t now,
                   bool ready=true, unsigned screen=0) {
        held&=0x7fu; edges&=0x7fu;
        // Scene change: never reinterpret the confirming A of MainMenu
        // as confirmation of ModeSelect. Wait for its physical release.
        if(!sceneInitialized_ || screen!=lastScreen_) {
            sceneInitialized_=true;lastScreen_=screen;
            pendingButtons_=pendingDirection_=lastDirection_=0;
            repeatAt_=0;
            releaseFrame_=false;
            suppressed_=held;
            edges=0;
        }
        suppressed_ &= held;
        held &= ~suppressed_;
        edges &= ~suppressed_;
        if(!menu){
            pendingButtons_=pendingDirection_=lastDirection_=0;
            repeatAt_=0;
            return held | edges;
        }
        pendingButtons_ |= edges & kButtons;
        unsigned direction=held&kDirections;
        if(direction==0 && (edges&kDirections)!=0)direction=edges&kDirections;
        if(direction!=lastDirection_){
            if(direction){pendingDirection_=direction;repeatAt_=now+kInitialRepeatMs;}
            lastDirection_=direction;
        }else if(direction && static_cast<std::int32_t>(now-repeatAt_)>=0){
            pendingDirection_=direction;
            repeatAt_=now+kRepeatMs;
        }
        if(!ready)return 0; // preserves unconsumed pulses across animation
        // Keypresses are serialized: menu's release-to-rearm latch needs
        // at least one empty input frame after each accepted pulse.
        if(releaseFrame_){releaseFrame_=false;return 0;}
        if(pendingButtons_){
            const unsigned button=(pendingButtons_&0x10u)?0x10u:
                (pendingButtons_&0x20u)?0x20u:0x40u;
            pendingButtons_ &= ~button;
            releaseFrame_=true;
            return button;
        }
        if(pendingDirection_){
            const unsigned dir=pendingDirection_;
            pendingDirection_=0;
            releaseFrame_=true;
            return dir;
        }
        return 0;
    }
private:
    unsigned lastScreen_=~0u;
    bool sceneInitialized_=false;
    unsigned suppressed_=0;
    unsigned pendingDirection_=0;
    unsigned pendingButtons_=0;
    unsigned lastDirection_=0;
    bool releaseFrame_=false;
    std::uint32_t repeatAt_=0;
};
