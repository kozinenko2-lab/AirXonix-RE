#include "game/bonus_extensions.hpp"
#include "game/game.hpp"
#include "net/lan_protocol.hpp"
#include "render/bonus_mesh_pack.hpp"
#include "core/cleanroom_archive.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

struct GameTestProbe {static void reset(Game& g){g.startNewSession(0);}};

int main(int argc,char**argv){
    using namespace airxonix;
    static_assert(coop::ProtocolVersion==8);
    bonus::Runtime b;
    assert(!b.enabled());
    b.reset(55);assert(b.state().kind==bonus::Kind::None);
    b.enable(true);b.reset(55);
    assert(b.state().kind!=bonus::Kind::None);
    const auto kind=b.state().kind;
    for(int n=0;n<300;n++)b.advance(100,{0.f,.008f,0.f});
    const auto& pos=b.state();
    assert(pos.waitMs==0&&pos.height<=.012f);
    auto result=b.advance(100,{pos.x,.008f,pos.z});
    assert(result==kind);
    assert(b.state().waitMs==10000);
    b.grant(bonus::Kind::Shield);assert(b.state().shieldMs==12000);
    assert(b.absorbHit());assert(!b.absorbHit());
    b.grant(bonus::Kind::Snowflake);assert(b.frozen());
    b.grant(bonus::Kind::ScoreX2);assert(b.doubleScore());
    b.grant(bonus::Kind::SafeTail);assert(b.safeTail());
    for(int i=0;i<150;i++)b.tickTimers(100);
    assert(!b.frozen()&&!b.doubleScore()&&!b.safeTail());
    b.enable(false);assert(!b.enabled());

    Game off,on;
    assert(!off.bonusExtensions().enabled());
    on.setNewBonusesEnabled(true);
    assert(on.bonusExtensions().enabled());
    GameTestProbe::reset(off);GameTestProbe::reset(on);
    for(std::size_t i=0;i<Pickups::Count;i++){
        const auto& p=off.pickups().items()[i];
        const auto& q=on.pickups().items()[i];
        assert(p.gridX==q.gridX&&p.gridY==q.gridY&&p.timerMs==q.timerMs);
    }
    const int life=on.lives();on.applyExtendedBonus(PickupEffect::TwoLives);
    assert(on.lives()==life+2);
    on.applyExtendedBonus(PickupEffect::Shield);
    assert(on.bonusExtensions().state().shieldMs==12000);
    off.applyExtendedBonus(PickupEffect::Shield);
    assert(!off.bonusExtensions().enabled()&&off.bonusExtensions().state().shieldMs==0);

    const auto settingsPath=std::filesystem::temp_directory_path()/"airxonix-r388-settings-test.bin";
    Game saving;saving.initializeLegacySettings(settingsPath);
    saving.setNewBonusesEnabled(true);assert(saving.saveLegacySettingsNow());
    assert(std::filesystem::file_size(settingsPath)==36);
    Game loading;loading.initializeLegacySettings(settingsPath);
    assert(loading.settings().newBonuses);
    loading.setNewBonusesEnabled(false);assert(loading.saveLegacySettingsNow());
    Game again;again.initializeLegacySettings(settingsPath);
    assert(!again.settings().newBonuses);
    std::filesystem::remove(settingsPath);
    std::filesystem::remove(settingsPath.string()+".bonuses.ini");

    lan::Snapshot snap{};snap.bonusEnabled=1;snap.bonusKind=2;
    snap.bonusX=.55f;snap.bonusZ=.46f;snap.bonusHeight=.009f;
    snap.bonusWait=0;snap.shieldMs=12000;snap.freezeMs=5000;
    snap.scoreMs=3000;snap.tailMs=2000;
    auto encoded=lan::encodeSnapshot(snap);
    lan::Snapshot decoded{};
    assert(lan::decodeSnapshot(encoded,decoded));
    assert(decoded.bonusKind==2&&decoded.shieldMs==12000);
    encoded[encoded.size()-1]=0xFF; encoded[encoded.size()-2]=0xFF; // invalid eraser angle
    assert(!lan::decodeSnapshot(encoded,decoded));

    if(argc>1){
        std::string err;assert(CleanroomArchive::instance().open(argv[1],&err));
        std::array<LegacyMesh,5> models{};
        assert(bonus::loadModels(models));
        for(const auto& mesh:models)assert(!mesh.vertices.empty()&&!mesh.faces.empty());
        std::vector<std::uint8_t> tex;
        assert(CleanroomArchive::instance().read("textures/BONUSX.tga",tex));
        assert(tex.size()>200000u);
    }
    std::cout<<"r388 bonus extensions OK\n";
}
