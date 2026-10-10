#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include "coop_rules.hpp"

namespace airxonix::lan {
using Byte=std::uint8_t;
constexpr std::uint32_t Magic=0x41584D50u; // AXMP
constexpr std::size_t MaxPayload=32*1024;
enum class Kind : Byte { Hello=1, Input=2, Snapshot=3 };

struct InputPacket {std::uint32_t sequence=0; Byte buttons=0;std::uint32_t ackEpoch=0,ackDeathSerial=0;};
// Every death must be individually acknowledged, even if world snapshots are coalesced.
struct DeathView {std::uint32_t serial=0;Byte mask=0;float primaryX=0.f,primaryZ=0.f,otherX=0.f,otherZ=0.f;};
constexpr std::size_t MaxPendingDeaths=64;
struct PlayerView {
    std::int16_t x=0,y=0;
    float worldX=0.f,worldZ=0.f;
};
struct AirView {float x=0.f,y=0.f,worldX=0.f,worldZ=0.f;std::int16_t subtype=0;std::int16_t heading=0;Byte active=0;};
struct GroundView {float x=0.f,y=0.f,worldX=0.f,worldZ=0.f,delay=0.f;Byte active=0;};
struct PickupView {
    std::int32_t timerMs=0;
    std::int16_t gridX=0,gridY=0;
    float worldX=0.f,height=0.f,worldZ=0.f;
};
struct HomingView { Byte active=0;float x=.5f,y=.25f,z=.5f,spin=0.f; };
struct EraserView { Byte active=0;float x=.5f,z=.5f,spin=0.f;std::int32_t angle=0; };
struct Snapshot {
    std::uint32_t sequence=0;
    std::uint32_t epoch=1; // increases on host-authorized restart
    std::uint16_t mode=0,level=0;
    std::int32_t lives=3,score=0,timeMs=0,percent=0;
    Byte lastVictim=0;
    Byte deathMask=0; // bit 0 BLUE; bit 1 RED; simultaneous deaths share one serial
    Byte phase=0;
    Byte paused=0; // pause state is decided by the host; no client-side toggling
    std::uint32_t deathSerial=0;
    std::vector<DeathView> deaths; // all unacknowledged events, in serial order
    // Authoritative freeze after the host has loaded the next co-op level.
    std::uint16_t transitionMs=0;
    float deathWorldX=0.f,deathWorldZ=0.f; // position of lastVictim
    float otherDeathWorldX=0.f,otherDeathWorldZ=0.f; // BLUE when both fall
    std::uint16_t finaleElapsedMs=0;
    float finaleY=0.008f;
    float finaleX=0.5f,finaleZ=0.5f;
    float finaleCinematicPhase=0.f,finaleRotorPhase=0.f;
    std::array<PlayerView,2> players{};
    std::array<Byte,coop::CellCount> field{};
    std::array<Byte,coop::CellCount> trailOwners{};
    std::vector<AirView> air;
    std::vector<GroundView> ground;
    std::array<PickupView,6> pickups{};
    std::int32_t pickupAngle=0;
    HomingView homing{};
    EraserView eraser{};
};

class Writer {
public:
    void u8(Byte v){data.push_back(v);}
    void u16(std::uint16_t v){u8(Byte(v>>8));u8(Byte(v));}
    void u32(std::uint32_t v){u16(std::uint16_t(v>>16));u16(std::uint16_t(v));}
    void i32(std::int32_t v){u32(static_cast<std::uint32_t>(v));}
    void f32(float f){std::uint32_t n;static_assert(sizeof(n)==sizeof(f));std::memcpy(&n,&f,4);u32(n);}
    void bytes(const Byte* ptr,std::size_t count){data.insert(data.end(),ptr,ptr+count);}
    std::vector<Byte> data;
};
class Reader {
public:
    Reader(const Byte* p,std::size_t n):ptr(p),remaining(n){}
    bool u8(Byte& n){if(remaining<1)return false;n=*ptr++;--remaining;return true;}
    bool u16(std::uint16_t& n){Byte a,b;if(!u8(a)||!u8(b))return false;n=std::uint16_t(a<<8|b);return true;}
    bool u32(std::uint32_t& n){std::uint16_t a,b;if(!u16(a)||!u16(b))return false;n=std::uint32_t(a)<<16|b;return true;}
    bool i32(std::int32_t& n){std::uint32_t v;if(!u32(v))return false;n=std::int32_t(v);return true;}
    bool f32(float& f){std::uint32_t v;if(!u32(v))return false;std::memcpy(&f,&v,4);return true;}
    bool bytes(Byte* out,std::size_t len){if(len>remaining)return false;std::memcpy(out,ptr,len);ptr+=len;remaining-=len;return true;}
    bool done() const {return remaining==0;}
private:
    const Byte* ptr;std::size_t remaining;
};

inline std::vector<Byte> encodeInput(const InputPacket& input){Writer w;w.u32(input.sequence);w.u8(input.buttons);w.u32(input.ackEpoch);w.u32(input.ackDeathSerial);return std::move(w.data);}
inline bool decodeInput(const std::vector<Byte>& b,InputPacket& out){Reader r(b.data(),b.size());return r.u32(out.sequence)&&r.u8(out.buttons)&&r.u32(out.ackEpoch)&&r.u32(out.ackDeathSerial)&&r.done();}
inline std::vector<Byte> encodeSnapshot(const Snapshot& s){
    Writer w;w.u32(s.sequence);w.u32(s.epoch);w.u16(s.mode);w.u16(s.level);
    w.i32(s.lives);w.i32(s.score);w.i32(s.timeMs);w.i32(s.percent);
    w.u8(s.lastVictim);w.u8(s.deathMask);w.u8(s.phase);w.u8(s.paused);w.u32(s.deathSerial);w.u16(s.transitionMs);
    w.u16(std::uint16_t(s.deaths.size()));
    for(const auto& d:s.deaths){w.u32(d.serial);w.u8(d.mask);w.f32(d.primaryX);w.f32(d.primaryZ);w.f32(d.otherX);w.f32(d.otherZ);}
    w.f32(s.deathWorldX);w.f32(s.deathWorldZ);w.f32(s.otherDeathWorldX);w.f32(s.otherDeathWorldZ);
    w.u16(s.finaleElapsedMs);w.f32(s.finaleY);w.f32(s.finaleX);w.f32(s.finaleZ);w.f32(s.finaleCinematicPhase);w.f32(s.finaleRotorPhase);
    for(const auto& p:s.players){w.u16(std::uint16_t(p.x));w.u16(std::uint16_t(p.y));w.f32(p.worldX);w.f32(p.worldZ);}
    w.bytes(s.field.data(),s.field.size());w.bytes(s.trailOwners.data(),s.trailOwners.size());
    w.u16(std::uint16_t(s.air.size()));
    for(const auto& a:s.air){w.f32(a.x);w.f32(a.y);w.f32(a.worldX);w.f32(a.worldZ);w.u16(std::uint16_t(a.subtype));w.u16(std::uint16_t(a.heading));w.u8(a.active);}
    w.u16(std::uint16_t(s.ground.size()));
    for(const auto& g:s.ground){w.f32(g.x);w.f32(g.y);w.f32(g.worldX);w.f32(g.worldZ);w.f32(g.delay);w.u8(g.active);}
    for(const auto& p:s.pickups){w.i32(p.timerMs);w.u16(std::uint16_t(p.gridX));w.u16(std::uint16_t(p.gridY));w.f32(p.worldX);w.f32(p.height);w.f32(p.worldZ);}
    w.i32(s.pickupAngle);
    w.u8(s.homing.active);w.f32(s.homing.x);w.f32(s.homing.y);w.f32(s.homing.z);w.f32(s.homing.spin);
    w.u8(s.eraser.active);w.f32(s.eraser.x);w.f32(s.eraser.z);w.f32(s.eraser.spin);w.i32(s.eraser.angle);
    return std::move(w.data);
}
inline bool decodeSnapshot(const std::vector<Byte>& b,Snapshot& s){
    if(b.size()>MaxPayload)return false;
    Reader r(b.data(),b.size());
    if(!r.u32(s.sequence)||!r.u32(s.epoch)||!r.u16(s.mode)||!r.u16(s.level)||!r.i32(s.lives)||!r.i32(s.score)||!r.i32(s.timeMs)||!r.i32(s.percent)||!r.u8(s.lastVictim)||!r.u8(s.deathMask)||!r.u8(s.phase)||!r.u8(s.paused)||!r.u32(s.deathSerial)||!r.u16(s.transitionMs))return false;
    std::uint16_t deathCount=0;
    if(!r.u16(deathCount)||deathCount>MaxPendingDeaths)return false;
    s.deaths.clear();s.deaths.reserve(deathCount);
    for(unsigned i=0;i<deathCount;++i){
        DeathView d{};
        if(!r.u32(d.serial)||!r.u8(d.mask)||!r.f32(d.primaryX)||!r.f32(d.primaryZ)||
           !r.f32(d.otherX)||!r.f32(d.otherZ)||d.mask==0||d.mask>3||d.serial==0||
           (i>0&&d.serial!=s.deaths.back().serial+1)||d.serial>s.deathSerial||
           !std::isfinite(d.primaryX)||!std::isfinite(d.primaryZ)||
           !std::isfinite(d.otherX)||!std::isfinite(d.otherZ)||
           d.primaryX<0.f||d.primaryX>1.f||d.primaryZ<0.f||d.primaryZ>1.f||
           d.otherX<0.f||d.otherX>1.f||d.otherZ<0.f||d.otherZ>1.f)return false;
        s.deaths.push_back(d);
    }
    if(!r.f32(s.deathWorldX)||!r.f32(s.deathWorldZ)||!r.f32(s.otherDeathWorldX)||!r.f32(s.otherDeathWorldZ)||
       !r.u16(s.finaleElapsedMs)||!r.f32(s.finaleY)||!r.f32(s.finaleX)||!r.f32(s.finaleZ)||
       !r.f32(s.finaleCinematicPhase)||!r.f32(s.finaleRotorPhase))return false;
    if(!std::isfinite(s.deathWorldX)||!std::isfinite(s.deathWorldZ)||s.deathWorldX<0.f||s.deathWorldX>1.f||s.deathWorldZ<0.f||s.deathWorldZ>1.f||
       !std::isfinite(s.otherDeathWorldX)||!std::isfinite(s.otherDeathWorldZ)||
       s.otherDeathWorldX<0.f||s.otherDeathWorldX>1.f||s.otherDeathWorldZ<0.f||s.otherDeathWorldZ>1.f||
       !std::isfinite(s.finaleY)||s.finaleY<0.f||s.finaleY>0.11f||s.finaleElapsedMs>8000||
       !std::isfinite(s.finaleX)||!std::isfinite(s.finaleZ)||
       s.finaleX<0.f||s.finaleX>1.f||s.finaleZ<0.f||s.finaleZ>1.f||
       !std::isfinite(s.finaleCinematicPhase)||!std::isfinite(s.finaleRotorPhase)||
       s.finaleCinematicPhase<0.f||s.finaleCinematicPhase>8.f||
       s.finaleRotorPhase<0.f||s.finaleRotorPhase>7.f)return false;
    if(s.epoch==0||s.mode>15||s.level>255||s.lives<0||s.lives>100||s.percent<0||s.percent>200||s.lastVictim>2||s.deathMask>3||
       ((s.deathMask==1&&s.lastVictim!=1)||(s.deathMask==2&&s.lastVictim!=2)||(s.deathMask==3&&s.lastVictim!=2))||
       s.phase>20||s.paused>1||s.transitionMs>1500)return false;
    for(auto& p:s.players){std::uint16_t x,y;if(!r.u16(x)||!r.u16(y)||!r.f32(p.worldX)||!r.f32(p.worldZ))return false;p.x=std::int16_t(x);p.y=std::int16_t(y);if(p.x< -4||p.x>67||p.y< -4||p.y>67||!std::isfinite(p.worldX)||!std::isfinite(p.worldZ)||p.worldX<0.f||p.worldX>1.f||p.worldZ<0.f||p.worldZ>1.f)return false;}
    if(!r.bytes(s.field.data(),s.field.size())||!r.bytes(s.trailOwners.data(),s.trailOwners.size()))return false;
    std::uint16_t n=0;if(!r.u16(n)||n>128)return false;s.air.resize(n);
    for(auto& a:s.air){std::uint16_t subtype,heading;if(!r.f32(a.x)||!r.f32(a.y)||!r.f32(a.worldX)||!r.f32(a.worldZ)||!r.u16(subtype)||!r.u16(heading)||!r.u8(a.active))return false;a.subtype=std::int16_t(subtype);a.heading=std::int16_t(heading);if(!std::isfinite(a.x)||!std::isfinite(a.y)||!std::isfinite(a.worldX)||!std::isfinite(a.worldZ))return false;}
    if(!r.u16(n)||n>128)return false;
    s.ground.resize(n);
    for(auto& g:s.ground)if(!r.f32(g.x)||!r.f32(g.y)||!r.f32(g.worldX)||!r.f32(g.worldZ)||!r.f32(g.delay)||!r.u8(g.active)||!std::isfinite(g.x)||!std::isfinite(g.y)||!std::isfinite(g.worldX)||!std::isfinite(g.worldZ)||!std::isfinite(g.delay))return false;
    for(auto& p:s.pickups){
        std::uint16_t gx=0,gy=0;
        if(!r.i32(p.timerMs)||!r.u16(gx)||!r.u16(gy)||!r.f32(p.worldX)||!r.f32(p.height)||!r.f32(p.worldZ))return false;
        p.gridX=std::int16_t(gx);p.gridY=std::int16_t(gy);
        if(p.timerMs< -1000||p.timerMs>3600000||p.gridX< -4||p.gridX>67||p.gridY< -4||p.gridY>67||
           !std::isfinite(p.worldX)||!std::isfinite(p.height)||!std::isfinite(p.worldZ)||
           p.worldX<0.f||p.worldX>1.f||p.worldZ<0.f||p.worldZ>1.f||p.height< -.5f||p.height>1.f)return false;
    }
    if(!r.i32(s.pickupAngle)||!r.u8(s.homing.active)||!r.f32(s.homing.x)||!r.f32(s.homing.y)||!r.f32(s.homing.z)||!r.f32(s.homing.spin)||
       !r.u8(s.eraser.active)||!r.f32(s.eraser.x)||!r.f32(s.eraser.z)||!r.f32(s.eraser.spin)||!r.i32(s.eraser.angle))return false;
    if(s.homing.active>1||s.eraser.active>1||
       !std::isfinite(s.homing.x)||!std::isfinite(s.homing.y)||!std::isfinite(s.homing.z)||!std::isfinite(s.homing.spin)||
       !std::isfinite(s.eraser.x)||!std::isfinite(s.eraser.z)||!std::isfinite(s.eraser.spin)||
       s.homing.x<0.f||s.homing.x>1.f||s.homing.z<0.f||s.homing.z>1.f||
       s.eraser.x<0.f||s.eraser.x>1.f||s.eraser.z<0.f||s.eraser.z>1.f||
       s.homing.y< -1.f||s.homing.y>1.f||s.eraser.angle<0||s.eraser.angle>2047)return false;
    return r.done();
}
inline std::vector<Byte> frame(Kind kind,const std::vector<Byte>& body){
    if(body.size()>MaxPayload)return {};
    Writer w;w.u32(Magic);w.u16(coop::ProtocolVersion);w.u8(Byte(kind));w.u8(0);w.u32(std::uint32_t(body.size()));
    w.bytes(body.data(),body.size());return std::move(w.data);
}
} // namespace airxonix::lan
