#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include "pickups.hpp"

// Optional native extension. Never modifies the reconstructed original six
// pickups or consumes the MSVC-compatible LegacyRandom stream.
namespace airxonix::bonus {
enum class Kind : std::uint8_t { Shield=0, Snowflake=1, LivesX2=2, ScoreX2=3, SafeTail=4, None=255 };
constexpr int Count=5;
struct State {
    Kind kind=Kind::None;
    float x=.5f,z=.5f,height=.08f;
    int waitMs=9000;  // delay before a physical bonus falls
    int shieldMs=0,freezeMs=0,scoreMs=0,tailMs=0;
    std::uint32_t random=0x932ace5du;
};
class Runtime {
public:
    bool enabled() const{return enabled_;}
    void enable(bool e){enabled_=e;if(!e)state_={};}
    const State& state() const{return state_;}
    void networkState(bool on,const State& s){enabled_=on;state_=s;}
    void reset(std::uint32_t levelSeed){
        if(!enabled_){state_={};return;}
        state_={};state_.random=levelSeed^0xb27d48a5u;
        if(state_.random==0)state_.random=1;
        spawn(7000);
    }
    static std::uint32_t next(std::uint32_t& s){
        if(!s)s=0xa931fe51u;
        s^=s<<13;s^=s>>17;s^=s<<5;
        return s;
    }
    void tickTimers(int dt){
        if(!enabled_)return;
        dt=std::clamp(dt,0,100);
        state_.shieldMs=std::max(0,state_.shieldMs-dt);
        state_.freezeMs=std::max(0,state_.freezeMs-dt);
        state_.scoreMs=std::max(0,state_.scoreMs-dt);
        state_.tailMs=std::max(0,state_.tailMs-dt);
    }
    // Single extension pickup; two player collectors in cooperative sessions.
    Kind advance(int dt,const PickupCollectorState& first,const PickupCollectorState* second=nullptr){
        if(!enabled_||dt<=0)return Kind::None;
        dt=std::min(100,dt);
        if(state_.waitMs>0){state_.waitMs=std::max(0,state_.waitMs-dt);return Kind::None;}
        state_.height=std::max(.008f,state_.height-float(dt)*.00008f);
        const auto close=[&](const PickupCollectorState& p){
            const float dx=state_.x-p.worldX,dz=state_.z-p.worldZ;
            return p.worldY<.03f && dx*dx+dz*dz<4.9e-5f;
        };
        if(state_.height<=.012f && (close(first)||(second && close(*second)))){
            auto kind=state_.kind;
            spawn(10000);
            return kind;
        }
        return Kind::None;
    }
    void grant(Kind k){
        if(!enabled_)return;
        switch(k){
        case Kind::Shield:state_.shieldMs=12000;break;
        case Kind::Snowflake:state_.freezeMs=6500;break;
        case Kind::ScoreX2:state_.scoreMs=15000;break;
        case Kind::SafeTail:state_.tailMs=8000;break;
        default:break;
        }
    }
    bool absorbHit(){if(!enabled_||state_.shieldMs<=0)return false;state_.shieldMs=0;return true;}
    bool frozen() const{return enabled_&&state_.freezeMs>0;}
    bool doubleScore() const{return enabled_&&state_.scoreMs>0;}
    bool safeTail() const{return enabled_&&state_.tailMs>0;}
private:
    void spawn(int delay){
        state_.kind=static_cast<Kind>(next(state_.random)%Count);
        state_.x=.4015384614f+float(5+(next(state_.random)%54u))*.003125f;
        state_.z=.4015384614f+float(5+(next(state_.random)%54u))*.003125f;
        state_.height=.08f;
        state_.waitMs=delay;
    }
    bool enabled_=false;
    State state_{};
};
}
