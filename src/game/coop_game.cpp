#include "game.hpp"
#include <algorithm>
#include <cstdio>
#include <cmath>

using airxonix::coop::Owner;

void Game::startCoopSession(std::size_t selectedMode){
    coopInitialMode_=database_.modes().empty()?0:std::min(selectedMode,database_.modes().size()-1);
    coopEnabled_=true;
    lastCoopVictim_=Owner::None;
    coopSessionEpoch_=1;
    coopDeathSerial_=0;
    coopPendingDeaths_.clear();
    coopRestartActionHeld_=false;
    coopPauseHeld_=false;
    coopBlueHitPending_=coopRedHitPending_=0;
    coopDeathMask_=0;
    coopOtherDeathWorldX_=coopOtherDeathWorldZ_=0.f;
    coopFinaleElapsedMs_=0;
    coopSnapshotApplied_=false;
    coopDeathWorldX_=coopDeathWorldZ_=0.f;
    coopDeathNoticeRemainingMs_=0;
    coopTransitionRemainingMs_=0;
    trailOwners_.clear();
    // Host and client both instantiate the same level. Host alone advances it.
    startNewSession(coopInitialMode_);
    paused_=false;
    std::fprintf(stderr,"AX_MP start: blue=(%d,%d) red=(%d,%d) lives=%d timer=%d\n",
        player_.x(),player_.y(),secondPlayer_.x(),secondPlayer_.y(),lives_,timer_);
}

void Game::endCoopSessionToMenu(){
    coopEnabled_=false;
    coopLinkReady_=false;
    coopRestartActionHeld_=false;
    coopPauseHeld_=false;
    coopBlueHitPending_=coopRedHitPending_=0;
    coopPendingDeaths_.clear();
    trailOwners_.clear();
    enterMainMenu();
}

void Game::updateCoopPresentation(int dtMs){
    if(coopEnabled_ && dtMs>0)
        coopDeathNoticeRemainingMs_=std::max(0,coopDeathNoticeRemainingMs_-std::min(dtMs,100));
}

void Game::coopRefreshTrailOwners(){
    trailOwners_.clear();
    for(const auto& t:player_.trail())
        if(field_.at(t.x,t.y)==Field::Trail)trailOwners_.assign(t.x,t.y,Owner::Blue);
    for(const auto& t:secondPlayer_.trail())
        if(field_.at(t.x,t.y)==Field::Trail)trailOwners_.assign(t.x,t.y,Owner::Red);
}
void Game::coopKill(Owner who,const char* reason){
    if(who==Owner::None)return;
    coopKillMask(who==Owner::Blue?1u:2u,reason);
}

// One authoritative collision frame can kill both Xonix. Deduct the shared
// pool for each victim while publishing a SINGLE atomic event to the client.
void Game::coopKillMask(std::uint8_t mask,const char* reason){
    mask&=3u;
    if(lives_<=0||mask==0)return;
    const float blueX=player_.worldX(),blueZ=player_.worldZ();
    const float redX=secondPlayer_.worldX(),redZ=secondPlayer_.worldZ();
    const auto resetVictim=[&](Player& victim,Owner owner){
        for(const auto& t:victim.trail())
            if(field_.inside(t.x,t.y)&&field_.at(t.x,t.y)==Field::Trail)
                field_.set(t.x,t.y,Field::Empty);
        victim.clearTrailForDeath();
        victim.coopSpawn(owner==Owner::Blue?airxonix::coop::BlueSpawn.x:airxonix::coop::RedSpawn.x,0);
    };
    if(mask&1u)resetVictim(player_,Owner::Blue);
    if(mask&2u)resetVictim(secondPlayer_,Owner::Red);
    const int victims=((mask&1u)?1:0)+((mask&2u)?1:0);
    lives_=std::max(0,lives_-victims);
    lastCoopVictim_=(mask&2u)?Owner::Red:Owner::Blue;
    coopDeathMask_=mask;
    ++coopDeathSerial_;
    coopDeathWorldX_=(mask&2u)?redX:blueX;
    coopDeathWorldZ_=(mask&2u)?redZ:blueZ;
    // When both die, the primary coordinates belong to RED; carry BLUE's
    // impact separately so each console plays both sounds at correct spots.
    coopOtherDeathWorldX_=(mask==3u)?blueX:0.f;
    coopOtherDeathWorldZ_=(mask==3u)?blueZ:0.f;
    coopDeathNoticeRemainingMs_=2200;
    // Keep every unacknowledged event in host memory. A snapshot sends up
    // to MaxPendingDeaths; later packets drain the remainder after guest ACK.
    // Normal sessions contain only three lives, so this is a tiny queue.
    coopPendingDeaths_.push_back({coopDeathSerial_,mask,coopDeathWorldX_,coopDeathWorldZ_,
                                   coopOtherDeathWorldX_,coopOtherDeathWorldZ_});
    if(mask&1u){queueDeathSpatialPlay(0x05u,blueX,.008f,blueZ,1.f);++coopBlueHitPending_;}
    if(mask&2u){queueDeathSpatialPlay(0x05u,redX,.008f,redZ,1.f);++coopRedHitPending_;}
    std::fprintf(stderr,"AX_MP_DEATH mask=%u reason=%s lives=%d serial=%u\n",
                 unsigned(mask),reason,lives_,coopDeathSerial_);
    coopRefreshTrailOwners();
    if(lives_==0){
        phase_=GamePhase::GameOver;
        gameOverScene_={};gameOverScene_.timeoutMs=5000;
        deathScene_={};deathScene_.worldX=coopDeathWorldX_;deathScene_.worldZ=coopDeathWorldZ_;
        gameOverScene_.visual=deathScene_;
    }
}
void Game::coopBeginNextLevel(){
    const auto& levels=database_.modes()[mode_].levels;
    if(level_+1<levels.size()){
        loadLevel(mode_,level_+1);
        coopTransitionRemainingMs_=1500;
        std::fprintf(stderr,"AX_MP_LEVEL mode=%zu level=%zu transitionMs=%d\n",
            mode_,level_,coopTransitionRemainingMs_);
    }else{
        coopFinaleElapsedMs_=0;
        beginFinalSequence();
        std::fprintf(stderr,"AX_MP_FINALE score=%d\n",score_);
    }
}

void Game::acknowledgeCoopDeaths(std::uint32_t epoch,std::uint32_t serial){
    if(!coopEnabled_||epoch!=coopSessionEpoch_||serial>coopDeathSerial_)return;
    // Acknowledgements are cumulative; an old / repeated packet has no effect.
    coopPendingDeaths_.erase(std::remove_if(coopPendingDeaths_.begin(),coopPendingDeaths_.end(),
        [serial](const airxonix::lan::DeathView& e){return e.serial<=serial;}),coopPendingDeaths_.end());
}

void Game::restartCoopSession(){
    // Host alone makes the decision, and assigns a new generation. This
    // prevents delayed death notifications from the previous round replaying.
    if(++coopSessionEpoch_==0)coopSessionEpoch_=1;
    coopDeathSerial_=0;coopDeathMask_=0;lastCoopVictim_=Owner::None;
    coopPendingDeaths_.clear();
    coopDeathWorldX_=coopDeathWorldZ_=0.f;
    coopOtherDeathWorldX_=coopOtherDeathWorldZ_=0.f;
    coopDeathNoticeRemainingMs_=0;coopFinaleElapsedMs_=0;
    coopTransitionRemainingMs_=1500;
    coopPauseHeld_=false;
    coopBlueHitPending_=coopRedHitPending_=0;
    paused_=false;
    startNewSession(coopInitialMode_);
    // startNewSession resets the board but preserves the session epoch.
    std::fprintf(stderr,"AX_MP_RESTART epoch=%u lives=%d score=%d\n",coopSessionEpoch_,lives_,score_);
}

void Game::updateCoop(const InputState& blue,const InputState& red,int dtMs){
    if(!coopEnabled_||dtMs<=0)return;
    if(blue.quit){quit_=true;return;}
    dtMs=std::min(dtMs,100);
    const bool restartPressed=blue.action&&!coopRestartActionHeld_;
    coopRestartActionHeld_=blue.action;
    // Rising-edge shared pause. Press START on either handheld to toggle;
    // holding START never oscillates between paused/unpaused each frame.
    const bool pauseHeld=blue.pause||red.pause;
    const bool pauseEdge=pauseHeld&&!coopPauseHeld_;
    coopPauseHeld_=pauseHeld;
    if(phase_==GamePhase::GameOver || phase_==GamePhase::Complete){
        if(restartPressed)restartCoopSession();
        return;
    }
    // A finished campaign is still host-driven; r378 froze permanently in
    // FinalSequence because only Gameplay frames were handled here.
    if(phase_==GamePhase::FinalSequence){
        coopFinaleElapsedMs_=std::min(8000,coopFinaleElapsedMs_+dtMs);
        finalScene_.elapsedMs=coopFinaleElapsedMs_;
        finalScene_.presentationY=std::min(.1f,finalScene_.presentationY+float(dtMs)*.00002f);
        finalScene_.xonixHeight=std::min(.04f,finalScene_.presentationY);
        // Reuse the original finale orbit and oscillation rates. The client
        // receives these presentation values; it never runs game mechanics.
        constexpr float kTwoPi=6.2831853071795864769f;
        finalScene_.orbitAngle-=float(dtMs)*.0004f;
        while(finalScene_.orbitAngle<0.f)finalScene_.orbitAngle+=kTwoPi;
        finalScene_.xonixWorldX=.5f+std::cos(finalScene_.orbitAngle)*finalScene_.orbitRadius;
        finalScene_.xonixWorldZ=.5f+std::sin(finalScene_.orbitAngle)*finalScene_.orbitRadius;
        finalScene_.cinematicPhase+=float(dtMs)*.002f;
        while(finalScene_.cinematicPhase>=kTwoPi)finalScene_.cinematicPhase-=kTwoPi;
        finalScene_.rotorPhase+=.013f;
        while(finalScene_.rotorPhase>=kTwoPi)finalScene_.rotorPhase-=kTwoPi;
        if(coopFinaleElapsedMs_>=8000){
            phase_=GamePhase::Complete;
            std::fprintf(stderr,"AX_MP_COMPLETE score=%d\n",score_);
        }
        return;
    }
    // Terminal states are stable, never tick the shared clock or mutate score.
    if(phase_!=GamePhase::Gameplay)return;
    // The host first broadcasts the new board and keeps inputs/timer frozen
    // for the same transition interval on both machines. The guest never
    // simulates these frames and displays the countdown from snapshots.
    if(coopTransitionRemainingMs_>0){
        coopTransitionRemainingMs_=std::max(0,coopTransitionRemainingMs_-dtMs);
        return;
    }
    if(pauseEdge){
        paused_=!paused_;
        std::fprintf(stderr,"AX_MP_PAUSE value=%d epoch=%u\n",paused_?1:0,coopSessionEpoch_);
    }
    if(paused_)return;
    timer_=std::max(0,timer_-dtMs);
    if(timer_==0){lives_=0;phase_=GamePhase::GameOver;gameOverScene_={};gameOverScene_.timeoutMs=5000;return;}
    updateLegacyEffects(dtMs);
    secondPlayer_.setMaxSpeed(playerMaxSpeed_);
    auto seeds=entities_.captureSeeds();
    GridSeed eraserSeed{};
    if(specialObjects_.eraserCaptureSeed(eraserSeed))seeds.push_back(eraserSeed);
    // Advance players in a single authoritative field, no local client simulation.
    // A player's own and the rival's Trail cell are both lethal if crossed.
    player_.update(blue,dtMs,field_,seeds);
    secondPlayer_.update(red,dtMs,field_,seeds);
    player_.advanceTrailPresentation(dtMs);
    secondPlayer_.advanceTrailPresentation(dtMs);
    coopRefreshTrailOwners();
    field_.updateCaptureAnimations(dtMs);
    // Both participants share one authoritative world. Specials and bonuses
    // must advance ONCE per host frame, even when two players are present.
    float hx=0.f,hy=0.f,hz=0.f;
    if(specialObjects_.consumeHomingPlayerHit(player_.worldX(),player_.worldZ(),hx,hy,hz))player_.kill();
    if(specialObjects_.consumeHomingPlayerHit(secondPlayer_.worldX(),secondPlayer_.worldZ(),hx,hy,hz))secondPlayer_.kill();
    resolveSpecialPreEntityContacts();
    entities_.update(dtMs,field_,rng_,enemySpeedFactor_);
    consumeEntityCollisionAudioEvents();
    effectEvents_.airErosionImpactEvents+=entities_.consumeAirErosionImpactEvents();
    effectEvents_.airErosionDebrisHelperCalls+=entities_.consumeAirErosionDebrisHelperCalls();
    // Home toward the nearest living Xonix, without changing single-player AI.
    const auto& homing=specialObjects_.homing();
    const auto sq=[](float x){return x*x;};
    const float blueDist=sq(player_.worldX()-homing.worldX)+sq(player_.worldZ()-homing.worldZ);
    const float redDist=sq(secondPlayer_.worldX()-homing.worldX)+sq(secondPlayer_.worldZ()-homing.worldZ);
    const Player& target=redDist<blueDist?secondPlayer_:player_;
    const auto ownersBeforeSpecial=trailOwners_.cells();
    const auto fieldBeforeSpecial=field_.cells();
    specialObjects_.update(dtMs,field_,target,enemySpeedFactor_,rng_);
    consumeSpecialPostUpdateEvents();
    if(specialObjects_.consumeTrailHit()){
        // The eraser removes trail cells directly. Attribute the old erased
        // cells to their owners before refreshing the map (may affect both).
        bool blueHit=false,redHit=false;
        for(std::size_t i=0;i<fieldBeforeSpecial.size();++i){
            if(fieldBeforeSpecial[i]==Field::Trail && field_.cells()[i]!=Field::Trail){
                blueHit |= ownersBeforeSpecial[i]==Owner::Blue;
                redHit  |= ownersBeforeSpecial[i]==Owner::Red;
            }
        }
        if(blueHit)player_.kill();
        if(redHit)secondPlayer_.kill();
    }
    const PickupCollectorState redCollector{secondPlayer_.worldX(),secondPlayer_.visualY(),secondPlayer_.worldZ()};
    updatePickupRuntime(dtMs,{player_.worldX(),player_.visualY(),player_.worldZ()},true,&redCollector);
    advanceLegacyBackgroundScroll(dtMs);
    const auto impacts=entities_.consumeTrailImpacts();
    for(const auto& hit:impacts){
        // Engine uses a strict 3x3 impact neighborhood. Mark both trails
        // if the enemy touches both owners in the same collision frame.
        if(player_.markTrailHazardNear(hit.x,hit.y))
            std::fprintf(stderr,"AX_MP_TRAIL_HIT owner=BLUE x=%d y=%d\n",hit.x,hit.y);
        if(secondPlayer_.markTrailHazardNear(hit.x,hit.y))
            std::fprintf(stderr,"AX_MP_TRAIL_HIT owner=RED x=%d y=%d\n",hit.x,hit.y);
    }
    float hitX=0.f,hitZ=0.f;
    // Crawler collisions are attributed to the actual nearby player.
    if(entities_.consumeCrawlerPlayerHit(player_.worldX(),player_.worldZ(),hitX,hitZ))player_.kill();
    if(entities_.consumeCrawlerPlayerHit(secondPlayer_.worldX(),secondPlayer_.worldZ(),hitX,hitZ))secondPlayer_.kill();
    if(player_.updateTrailHazard(dtMs))player_.kill();
    if(secondPlayer_.updateTrailHazard(dtMs))secondPlayer_.kill();
    const std::uint8_t victims=(player_.dead()?1u:0u)|(secondPlayer_.dead()?2u:0u);
    if(victims)coopKillMask(victims,"body/trail");
    if(phase_!=GamePhase::Gameplay)return;
    // One score, one timer, one pool of lives. Captures from either Xonix add
    // to the same board and the same progress counter.
    updateScoreAndPercent();
    hudDisplayTimer_=timer_;hudDisplayScore_=score_;
    if(capturePercent_>=100&&!player_.cutting()&&!secondPlayer_.cutting()){
        coopBeginNextLevel();
    }
}

airxonix::lan::Snapshot Game::makeCoopSnapshot(std::uint32_t serial) const{
    airxonix::lan::Snapshot s{};
    s.sequence=serial;s.epoch=coopSessionEpoch_;s.mode=std::uint16_t(mode_);s.level=std::uint16_t(level_);
    s.lives=lives_;s.score=score_;s.timeMs=timer_;s.percent=capturePercent_;
    s.lastVictim=std::uint8_t(lastCoopVictim_);s.deathMask=coopDeathMask_;s.phase=std::uint8_t(phase_);s.paused=paused_?1u:0u;s.deathSerial=coopDeathSerial_;
    s.deaths.assign(coopPendingDeaths_.begin(),
                    coopPendingDeaths_.begin()+std::min(coopPendingDeaths_.size(),airxonix::lan::MaxPendingDeaths));
    s.deathWorldX=coopDeathWorldX_;s.deathWorldZ=coopDeathWorldZ_;
    s.otherDeathWorldX=coopOtherDeathWorldX_;s.otherDeathWorldZ=coopOtherDeathWorldZ_;
    s.finaleElapsedMs=std::uint16_t(std::clamp(coopFinaleElapsedMs_,0,8000));
    s.finaleY=std::clamp(finalScene_.presentationY,0.f,.1f);
    s.finaleX=finalScene_.xonixWorldX;s.finaleZ=finalScene_.xonixWorldZ;
    s.finaleCinematicPhase=finalScene_.cinematicPhase;
    s.finaleRotorPhase=finalScene_.rotorPhase;
    s.transitionMs=std::uint16_t(std::clamp(coopTransitionRemainingMs_,0,1500));
    s.players[0]={std::int16_t(player_.x()),std::int16_t(player_.y()),player_.worldX(),player_.worldZ()};
    s.players[1]={std::int16_t(secondPlayer_.x()),std::int16_t(secondPlayer_.y()),secondPlayer_.worldX(),secondPlayer_.worldZ()};
    s.field=field_.cells();
    const auto& owner=trailOwners_.cells();
    for(std::size_t i=0;i<owner.size();++i)s.trailOwners[i]=std::uint8_t(owner[i]);
    for(const auto& e:entities_.air())s.air.push_back({e.x,e.y,e.worldX,e.worldZ,std::int16_t(e.subtype),std::int16_t(e.heading2048),std::uint8_t(e.active)});
    for(const auto& g:entities_.ground())s.ground.push_back({g.x,g.y,g.worldX,g.worldZ,g.respawnDelay,std::uint8_t(g.active)});
    const auto& pickups=pickups_.items();
    for(std::size_t i=0;i<pickups.size();++i){
        const auto& p=pickups[i];
        s.pickups[i]={p.timerMs,std::int16_t(p.gridX),std::int16_t(p.gridY),p.worldX,p.height,p.worldZ};
    }
    s.pickupAngle=pickups_.presentationAngle2048();
    const auto& h=specialObjects_.homing();
    s.homing={std::uint8_t(h.active),h.worldX,h.height,h.worldZ,h.spinPhase};
    const auto& e=specialObjects_.eraser();
    s.eraser={std::uint8_t(e.active),e.worldX,e.worldZ,e.spinPhase,e.rotationAngle2048};
    return s;
}
bool Game::applyCoopSnapshot(const airxonix::lan::Snapshot& s){
    if(!coopEnabled_ || s.mode>=database_.modes().size() ||
       s.level>=database_.modes()[s.mode].levels.size())return false;
    if(s.epoch<coopSessionEpoch_)return false; // never go back to an old round
    if(s.epoch!=coopSessionEpoch_){
        coopSessionEpoch_=s.epoch;
        coopDeathSerial_=0;
        coopDeathMask_=0;
        coopDeathNoticeRemainingMs_=0;
        coopPendingDeaths_.clear();
        coopSnapshotApplied_=false;
        coopBlueHitPending_=coopRedHitPending_=0;
        startNewSession(s.mode); // local display reset; authoritative state follows
        std::fprintf(stderr,"AX_MP_REMOTE_RESTART epoch=%u\n",coopSessionEpoch_);
    }
    if(mode_!=s.mode||level_!=s.level){
        loadLevel(s.mode,s.level);
        std::fprintf(stderr,"AX_MP_REMOTE_LEVEL mode=%u level=%u transitionMs=%u\n",
                     unsigned(s.mode),unsigned(s.level),unsigned(s.transitionMs));
    }
    coopTransitionRemainingMs_=s.transitionMs;
    lives_=s.lives;score_=s.score;timer_=s.timeMs;capturePercent_=s.percent;
    hudDisplayTimer_=timer_;hudDisplayScore_=score_;
    // Snapshots may be coalesced, but events are retained until the guest ACKs
    // them. Process every contiguous missing serial once, including multiple
    // deaths between two received world snapshots.
    if(coopSnapshotApplied_){
        for(const auto& e:s.deaths){
            if(e.serial<=coopDeathSerial_)continue;
            if(e.serial!=coopDeathSerial_+1){
                std::fprintf(stderr,"AX_MP_DEATH_GAP expected=%u got=%u\n",coopDeathSerial_+1,e.serial);
                break; // do not ACK events we have not heard
            }
            if(e.mask&1u){
                queueDeathSpatialPlay(0x05u,e.mask==3u?e.otherX:e.primaryX,
                                      .008f,e.mask==3u?e.otherZ:e.primaryZ,1.f);
                ++coopBlueHitPending_;
            }
            if(e.mask&2u){queueDeathSpatialPlay(0x05u,e.primaryX,.008f,e.primaryZ,1.f);++coopRedHitPending_;}
            coopDeathSerial_=e.serial;
            coopDeathMask_=e.mask;
            lastCoopVictim_=(e.mask&2u)?Owner::Red:Owner::Blue;
            coopDeathNoticeRemainingMs_=2200;
            std::fprintf(stderr,"AX_MP_REMOTE_DEATH mask=%u serial=%u\n",unsigned(e.mask),e.serial);
        }
    }else{
        // First snapshot is authoritative baseline on a fresh join/restart.
        // Suppress replay of deaths that occurred before our local presentation.
        coopDeathSerial_=s.deathSerial;
        coopDeathMask_=s.deathMask;
        lastCoopVictim_=Owner(s.lastVictim);
    }
    coopDeathWorldX_=s.deathWorldX;coopDeathWorldZ_=s.deathWorldZ;
    coopOtherDeathWorldX_=s.otherDeathWorldX;coopOtherDeathWorldZ_=s.otherDeathWorldZ;
    coopFinaleElapsedMs_=s.finaleElapsedMs;
    coopSnapshotApplied_=true;
    const GamePhase previousPhase=phase_;
    phase_=static_cast<GamePhase>(s.phase);
    paused_=s.paused!=0;
    if(phase_==GamePhase::GameOver&&previousPhase!=GamePhase::GameOver){
        gameOverScene_={};gameOverScene_.timeoutMs=5000;
        deathScene_={};deathScene_.worldX=coopDeathWorldX_;deathScene_.worldZ=coopDeathWorldZ_;
        gameOverScene_.visual=deathScene_;
    }
    if(phase_==GamePhase::FinalSequence||phase_==GamePhase::Complete){
        // Client presentation follows host's cinematic Y; do not run any
        // client-side finale physics, rewards or RNG.
        finalScene_.presentationY=s.finaleY;
        finalScene_.xonixHeight=std::min(.04f,s.finaleY);
        finalScene_.elapsedMs=s.finaleElapsedMs;
        finalScene_.xonixWorldX=s.finaleX;
        finalScene_.xonixWorldZ=s.finaleZ;
        finalScene_.cinematicPhase=s.finaleCinematicPhase;
        finalScene_.rotorPhase=s.finaleRotorPhase;
    }
    field_.networkCells(s.field);trailOwners_.fromBytes(s.trailOwners);
    player_.networkPosition(s.players[0].x,s.players[0].y,s.players[0].worldX,s.players[0].worldZ);
    secondPlayer_.networkPosition(s.players[1].x,s.players[1].y,s.players[1].worldX,s.players[1].worldZ);
    std::vector<AirEnemy> air;air.reserve(s.air.size());
    for(const auto& a:s.air){AirEnemy e{};e.x=a.x;e.y=a.y;e.worldX=a.worldX;e.worldZ=a.worldZ;e.subtype=a.subtype;e.heading2048=a.heading;e.active=a.active!=0;air.push_back(e);}
    std::vector<GroundEnemy> ground;ground.reserve(s.ground.size());
    for(const auto& a:s.ground){GroundEnemy e{};e.x=a.x;e.y=a.y;e.worldX=a.worldX;e.worldZ=a.worldZ;e.respawnDelay=a.delay;e.active=a.active!=0;ground.push_back(e);}
    entities_.networkView(std::move(air),std::move(ground));
    std::array<Pickup,Pickups::Count> remotePickups{};
    for(std::size_t i=0;i<remotePickups.size();++i){
        const auto& p=s.pickups[i];
        remotePickups[i]={p.timerMs,p.gridX,p.gridY,p.worldX,p.height,p.worldZ};
    }
    pickups_.networkView(remotePickups,s.pickupAngle);
    specialObjects_.networkPresentation(s.homing.active!=0,s.homing.x,s.homing.y,s.homing.z,s.homing.spin,
        s.eraser.active!=0,s.eraser.x,s.eraser.z,s.eraser.spin,s.eraser.angle);
    return true;
}
