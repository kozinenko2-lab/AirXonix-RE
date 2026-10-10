#pragma once
// Android-only virtual keyboard bridge. It is deliberately independent of SDL,
// Java and the H700 GameController/joystick implementation.
#include <algorithm>
#include <cstdint>
#include <deque>
#include <mutex>

class AndroidVirtualInput {
public:
    static constexpr unsigned UP=1u, DOWN=2u, LEFT=4u, RIGHT=8u;
    static constexpr unsigned ENTER=16u, ESCAPE=32u, PAUSE=64u;
    static constexpr unsigned DIRECTION=UP|DOWN|LEFT|RIGHT;
    static constexpr unsigned ALL=DIRECTION|ENTER|ESCAPE|PAUSE;

    struct Frame {
        unsigned bits=0;
        unsigned edge=0;
        int virtualKey=-1;
    };

    // JNI invokes this on Android's UI thread. Every short DOWN/UP survives
    // even if BOTH events arrive before the game consumes the next frame.
    void event(unsigned key, bool down) {
        if(!key || (key & (key-1u)) || !(key&ALL)) return;
        std::lock_guard<std::mutex> lock(mutex_);
        const bool wasDown = (physical_ & key)!=0;
        if(wasDown==down) return;
        if(down) {
            physical_ |= key;
            if(queue_.size()<64) queue_.push_back(key);
        } else {
            physical_ &= ~key;
            suppressed_ &= ~key;
        }
    }

    // Clears controls when Activity loses focus, preventing stuck keys.
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        physical_=suppressed_=0;
        queue_.clear();
        neutralFrame_=false;
        lastDirection_=0;
        nextRepeat_=0;
    }

    Frame poll(bool isMenu, bool ready, unsigned phase, std::uint32_t now) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(!initialized_ || phase!=phase_) {
            initialized_=true;phase_=phase;
            queue_.clear();
            suppressed_=physical_; // prior screen's held A must not confirm next screen
            neutralFrame_=false;
            lastDirection_=0;
            nextRepeat_=0;
        }
        suppressed_ &= physical_;
        const unsigned held=physical_ & ~suppressed_;
        if(!isMenu) {
            // Gameplay is continuous. Also preserve a very fast tap as one
            // input frame without waiting for a specific event polling rate.
            unsigned newest=0;
            while(!queue_.empty()) {
                const unsigned key=queue_.front();queue_.pop_front();
                if(!(suppressed_&key)) newest |= key;
            }
            const unsigned bits=held|newest;
            return {bits, newest, keycode(newest)};
        }
        // A real keyboard's key UP re-arms original AirXonix menus. After every
        // virtual key pulse we deliver a neutral frame, regardless of touch
        // duration. Never leak held keys into the original menu latch.
        if(neutralFrame_) {neutralFrame_=false;return {};}

        const unsigned direction=pickDirection(held&DIRECTION);
        if(direction!=lastDirection_) {
            lastDirection_=direction;
            nextRepeat_=now+350;
        }
        if(!ready) return {}; // keep queued taps until animation/fade accepts them

        while(!queue_.empty()) {
            const unsigned key=queue_.front();queue_.pop_front();
            if(suppressed_&key) continue;
            // Obsolete direction edges (left->right before next poll) are not
            // allowed to replay as movement in the opposite direction.
            if((key&DIRECTION) && (held&DIRECTION) && key!=direction)continue;
            neutralFrame_=true;
            if(key&DIRECTION) nextRepeat_=now+350;
            return {key,key,keycode(key)};
        }
        if(direction && static_cast<std::int32_t>(now-nextRepeat_)>=0){
            nextRepeat_=now+140;
            neutralFrame_=true;
            return {direction,direction,keycode(direction)};
        }
        return {};
    }

    static int keycode(unsigned bit) {
        if(bit&ENTER)return 0x0D; if(bit&ESCAPE)return 0x1B;
        if(bit&UP)return 0x26; if(bit&DOWN)return 0x28;
        if(bit&LEFT)return 0x25; if(bit&RIGHT)return 0x27;
        if(bit&PAUSE)return 0x50;
        return -1;
    }
private:
    static unsigned pickDirection(unsigned bits) {
        if(bits&UP)return UP; if(bits&DOWN)return DOWN;
        if(bits&LEFT)return LEFT; if(bits&RIGHT)return RIGHT;
        return 0;
    }
    std::mutex mutex_;
    unsigned physical_=0, suppressed_=0;
    std::deque<unsigned> queue_;
    bool neutralFrame_=false, initialized_=false;
    unsigned phase_=0, lastDirection_=0;
    std::uint32_t nextRepeat_=0;
};
