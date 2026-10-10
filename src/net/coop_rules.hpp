#pragma once
#include <array>
#include <cstdint>
#include <algorithm>
#include <vector>

namespace airxonix::coop {

// Shared cooperative campaign: resources are owned by the team, traces by players.
constexpr int Width=64, Height=64;
constexpr std::size_t CellCount=Width*Height;
enum class Owner : std::uint8_t { None=0, Blue=1, Red=2 };
struct Spawn { int x,y; };
constexpr Spawn BlueSpawn{20,0};
constexpr Spawn RedSpawn{44,0};
constexpr std::uint16_t Port=27840;
constexpr std::uint16_t ProtocolVersion=9;

struct Rgb { float r,g,b; };
constexpr Rgb color(Owner o){
    return o==Owner::Blue ? Rgb{0.26f,0.62f,1.f} :
           o==Owner::Red  ? Rgb{1.f,0.23f,0.32f} : Rgb{1.f,1.f,1.f};
}

class TrailOwners {
public:
    void clear(){ owners_.fill(Owner::None); }
    void assign(int x,int y,Owner player){
        if(inBounds(x,y))owners_[std::size_t(y)*Width+x]=player;
    }
    Owner at(int x,int y) const {
        return inBounds(x,y)?owners_[std::size_t(y)*Width+x]:Owner::None;
    }
    bool belongsTo(int x,int y,Owner player) const {return at(x,y)==player;}
    Owner hitByEnemy(int x,int y) const {return at(x,y);}
    const std::array<Owner,CellCount>& cells() const {return owners_;}
    void fromBytes(const std::array<std::uint8_t,CellCount>& bytes){
        for(std::size_t i=0;i<CellCount;++i)
            owners_[i]=(bytes[i]>=1 && bytes[i]<=2)?static_cast<Owner>(bytes[i]):Owner::None;
    }
    static bool inBounds(int x,int y){return x>=0&&x<Width&&y>=0&&y<Height;}
private:
    std::array<Owner,CellCount> owners_{};
};

struct SharedResources {
    int lives=3;
    int score=0;
    int timeMs=120000;
    Owner mostRecentVictim=Owner::None;
    std::uint32_t deathSerial=0;
    void die(Owner owner){
        if(owner==Owner::None||lives<=0)return;
        --lives;
        mostRecentVictim=owner;
        ++deathSerial;
    }
    void tick(int dt){timeMs=std::max(0,timeMs-std::max(0,dt));}
};

} // namespace airxonix::coop
