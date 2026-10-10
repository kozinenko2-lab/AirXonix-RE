#include "game/bonus_extensions.hpp"
#include "game/game.hpp"
#include "net/lan_protocol.hpp"
#include "render/bonus_mesh_pack.hpp"
#include "core/cleanroom_archive.hpp"
#include <cassert>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <fstream>

int main(int argc,char**argv){
    using namespace airxonix;
    static_assert(coop::ProtocolVersion==9);
    bonus::Runtime b;
    assert(!b.magnet());
    b.enable(true);b.grant(bonus::Kind::Magnet);
    assert(b.magnet());assert(b.state().magnetMs==12000);
    b.tickTimers(100);assert(b.state().magnetMs==11900);
    for(int i=0;i<119;i++)b.tickTimers(100);
    assert(!b.magnet());

    Field field;Entities entities;LegacyRandom rng;rng.seed(31);
    std::array<Pickup,Pickups::Count> records{};
    for(auto& item:records)item.timerMs=40000;
    records[0].timerMs=0;
    records[0].worldX=.53f;records[0].worldZ=.5f;records[0].height=.0005f;
    records[0].gridX=41;records[0].gridY=31;
    const PickupCollectorState first{.55f,.008f,.5f};
    Pickups baseline,attract;
    baseline.networkView(records,0);attract.networkView(records,0);
    baseline.updateWithCollectors(100,field,&first,1,entities,rng,60000,3,true,false);
    attract.updateWithCollectors(100,field,&first,1,entities,rng,60000,3,true,true);
    assert(std::fabs(baseline.items()[0].worldX-.53f)<1e-6f);
    assert(attract.items()[0].worldX>.537f && attract.items()[0].worldX<.54f);
    assert(attract.items()[0].height>.004f && attract.items()[0].height<.0101f);
    bool collected=false;
    for(int i=0;i<9;i++){
        auto effects=attract.updateWithCollectors(100,field,&first,1,entities,rng,60000,3,true,true);
        if(!effects.empty()){assert(effects[0]==PickupEffect::Score1000);collected=true;break;}
    }
    assert(collected);

    // Starting high above the board (still falling), a pickup must fly down
    // to the raised capture surface instead of clipping under it.
    records[0].worldX=.54f;records[0].height=.105f;
    Pickups falling;falling.networkView(records,0);
    bool highCollected=false;
    for(int i=0;i<30;i++){
        auto effects=falling.updateWithCollectors(100,field,&first,1,entities,rng,60000,3,true,true);
        if(!effects.empty()){highCollected=true;break;}
        assert(falling.items()[0].height>=.0099f);
    }
    assert(highCollected);
    // Choose the nearer collector, independently of host blue/red slot order.
    records[0].worldX=.52f;records[0].height=.005f;
    const PickupCollectorState both[2]={{.60f,.008f,.5f},{.50f,.008f,.5f}};
    Pickups coop;coop.networkView(records,0);
    coop.updateWithCollectors(100,field,both,2,entities,rng,60000,3,true,true);
    assert(coop.items()[0].worldX<.52f);

    Game g;assert(g.settings().graphicsMode==0);
    g.setGraphicsMode(3);assert(g.settings().graphicsMode==3);
    g.setGraphicsMode(99);assert(g.settings().graphicsMode==3);
    g.setGraphicsMode(-22);assert(g.settings().graphicsMode==0);
    auto path=std::filesystem::temp_directory_path()/"airxonix-r389-gfx.bin";
    g.initializeLegacySettings(path);g.setGraphicsMode(2);g.setNewBonusesEnabled(true);
    assert(g.saveLegacySettingsNow());
    assert(std::filesystem::file_size(path)==36);
    Game reload;reload.initializeLegacySettings(path);
    assert(reload.settings().graphicsMode==2 && reload.settings().newBonuses);
    for(const auto& ext:{"",".bonuses.ini",".graphics.ini"})std::filesystem::remove(path.string()+ext);

    lan::Snapshot snap{};snap.bonusEnabled=1;snap.bonusKind=5;snap.magnetMs=11600;
    lan::Snapshot back{};assert(lan::decodeSnapshot(lan::encodeSnapshot(snap),back));
    assert(back.bonusKind==5 && back.magnetMs==11600);
    snap.bonusKind=6;assert(!lan::decodeSnapshot(lan::encodeSnapshot(snap),back));

    if(argc>1){
        std::string error;assert(CleanroomArchive::instance().open(argv[1],&error));
        std::array<LegacyMesh,6> models{};assert(bonus::loadModels(models));
        for(const auto& m:models)assert(!m.vertices.empty() && !m.faces.empty());
        std::vector<std::uint8_t> raw;
        assert(CleanroomArchive::instance().read("textures/BONUSX.tga",raw));
        assert(raw.size()>256u*384u*4u);
        assert(CleanroomArchive::instance().read("textures/MPMENU.tga",raw));
        assert(raw.size()>256u*512u*4u);
    }
    std::cout<<"r389 magnet / graphics / networking PASS\n";
}
