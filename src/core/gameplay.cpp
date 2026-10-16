#include "core/exploration.hpp"
#include <algorithm>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <tuple>

namespace pet {
namespace {
constexpr std::uint64_t kFiniteTimeoutMs = 5000, kFadeMs = 750, kMealMs = 7000;
std::string pendingHit(const ExplorationState& s) {
    const auto& g=s.gameplay;
    return "pending-hit:"+std::to_string(s.runId)+":"+std::to_string(s.floorIndex)+":"+
        std::to_string(g.enemyInstance)+":"+std::to_string(g.actionRound);
}
}
uint64_t navigationFingerprint(const ExplorationState& s) {
    uint64_t hash=14695981039346656037ULL;
    auto add=[&](uint64_t value){for(unsigned byte=0;byte<8;++byte){hash^=uint8_t(value>>(byte*8));hash*=1099511628211ULL;}};
    add(s.floorIndex);add(s.world.version);add(s.mapSeed);add(s.rng.state);add(s.position);
    add(s.target+1);add(static_cast<unsigned>(s.goalKind));add(s.targetInstance);add(s.knownStairs+1);
    add(s.inventory.size());add(s.onceKeys.size());add(s.gameplay.onceKeys.size());add(s.gameplay.hearts);add(s.gameplay.oran);
    const auto& partner=s.gameplay.companion;
    if(partner.enabled){add(partner.tile);add(partner.hearts);add(partner.oran);add(partner.actionRound==s.gameplay.actionRound);}
    for(int tile=0;tile<kTiles;++tile){add(s.visited[tile]);add(s.discovered[tile]);add(s.failedTargets[tile]);add(s.blocked[tile]);}
    for(const auto& e:s.population.entities){add(e.instanceId);add(e.tile);add(e.done);add(e.visible);
        add(static_cast<unsigned>(e.tracking));add(e.lastSeen+1);add(e.chaseTarget+1);add(e.blockedTurns);
        add(e.actionRound==s.gameplay.actionRound);}
    return hash;
}
const char* lifeName(LifePhase phase) {
    static const char* names[]{"Explore", "Pose", "ReturnHome", "Emotion", "Eat", "Sleep", "Wake", "Ready", "Combat", "Death", "Dialogue"};
    return names[static_cast<unsigned>(phase)];
}
const char* combatName(CombatPhase phase) {
    static const char* names[]{"PlayerTurn", "PlayerAttack", "EnemyHurt", "EnemyTurn", "EnemyAttack", "PlayerHurt", "BerryText", "Faint", "CombatVictory", "CombatDefeat", "PlayerFaint",
        "CompanionTurn","CompanionAttack","CompanionHurt","CompanionFaint","PlayerApproach","CompanionApproach","CompanionBerryText"};
    return names[static_cast<unsigned>(phase)];
}
bool dungeonScene(const GameplayState& g) { return !g.homeScene; }
bool bedroomScene(const GameplayState& g) {
    return g.homeScene && ((g.life==LifePhase::Sleep && g.sceneBeat!=SceneBeat::DiningExit) ||
        g.life==LifePhase::Wake || g.life==LifePhase::Ready);
}
bool nightScene(const GameplayState& g) {
    return bedroomScene(g) && !(g.life==LifePhase::Wake && (g.sceneBeat==SceneBeat::Waking || g.sceneBeat==SceneBeat::BedFadeIn)) && g.life!=LifePhase::Ready;
}
bool showFriendlyDisc(const GameplayState& g,bool friendly,bool player) {
    return dungeonScene(g) && g.life!=LifePhase::Dialogue && friendly && !player;
}
std::uint32_t gameplayClip(const GameplayState& g) {
    if(g.independentParty && !g.homeScene && (g.life==LifePhase::Explore || g.life==LifePhase::Combat) && g.sceneBeat==SceneBeat::None) return partyClip(g,0);
    switch (g.life) {
    case LifePhase::Pose: return 17;
    case LifePhase::Emotion: return 6; // New defeat expression cue using the supported Hurt body clip.
    case LifePhase::Eat: return 15;
    case LifePhase::Sleep:
        if(g.sceneBeat==SceneBeat::BedFadeIn || g.sceneBeat==SceneBeat::NightFadeIn) return 7;
        if(g.sceneBeat==SceneBeat::DiningExit) return 15;
        if(g.sceneBeat==SceneBeat::BedFace) return 22;
        if(g.sceneBeat==SceneBeat::BedLay) return 27;
        return 13;
    case LifePhase::Ready: if(g.sceneBeat==SceneBeat::RoomExit) return 0; return g.sceneBeat==SceneBeat::MorningFace ? 22:7;
    case LifePhase::Dialogue:return g.dialogueStage==DialogueStage::Player?22:7;
    case LifePhase::Wake: return g.sceneBeat==SceneBeat::WakeDelay || g.sceneBeat==SceneBeat::BedFadeIn?13:14;
    case LifePhase::Combat:
        if (g.combat == CombatPhase::PlayerAttack) return 1;
        if (g.combat == CombatPhase::PlayerHurt) return 6;
        if (g.combat == CombatPhase::PlayerFaint) return 33;
        if (g.combat == CombatPhase::PlayerApproach) return 0;
        return 7;
    default: return 7;
    }
}
bool holdGameplayPose(const GameplayState& g) {
    return (g.life==LifePhase::Sleep && g.sceneBeat==SceneBeat::BedFadeOut) ||
        (g.life==LifePhase::Wake && (g.sceneBeat==SceneBeat::BedFadeIn || g.sceneBeat==SceneBeat::WakeDelay));
}
bool gameplayLoop(const GameplayState& g) {
    if(holdGameplayPose(g)) return false;
    const auto clip = gameplayClip(g);
    if(g.life==LifePhase::Sleep && (g.sceneBeat==SceneBeat::Sleeping || g.sceneBeat==SceneBeat::BedFadeOut)) return false;
    return clip == 0 || clip == 7 || clip == 5 || clip == 13 || g.life == LifePhase::Eat || g.sceneBeat==SceneBeat::DiningExit;
}
std::uint32_t companionClip(const GameplayState& g) {
    if(g.independentParty && !g.homeScene && (g.life==LifePhase::Explore || g.life==LifePhase::Combat) && g.sceneBeat==SceneBeat::None) return partyClip(g,kCompanionActorId);
    if(g.homeScene) return gameplayClip(g);
    if(g.life==LifePhase::Combat) {
        if(g.combat==CombatPhase::CompanionAttack) return 1;
        if(g.combat==CombatPhase::CompanionHurt) return 6;
        if(g.combat==CombatPhase::CompanionFaint) return 33;
        if(g.combat==CombatPhase::CompanionApproach) return 0;
        return 7;
    }
    if(g.life==LifePhase::Explore) return g.companion.walkStartedMs?0:7;
    return gameplayClip(g);
}
bool companionLoop(const GameplayState& g) {
    const auto clip=companionClip(g);
    if(g.homeScene) return gameplayLoop(g);
    return clip==0 || clip==7;
}
AttackRoll rollBasicAttack(Rng& rng, bool player) {
    AttackRoll result;
    result.hit = rng.bounded(100) < 80;
    if (result.hit) {
        result.critical = rng.bounded(100) < (player ? 20U : 5U);
        result.damage = result.critical ? 2 : 1;
    }
    return result;
}
void Explorer::assignRelationships() {
    state_.gameplay.hostile.clear();
    for (const auto& e : state_.population.entities)
        if (e.kind == EntityKind::Friend && !e.done && state_.rng.bounded(4) != 0)
            state_.gameplay.hostile.push_back(e.instanceId);
}
void Explorer::enableGameplay(std::uint64_t now) {
    if (state_.gameplay.enabled) return;
    auto& g = state_.gameplay;
    g.enabled = true;
    g.oran = std::any_of(state_.inventory.begin(), state_.inventory.end(), [](const auto& i) { return i.originalId == ITEM_ORAN_BERRY; }) ? 1 : 0;
    assignRelationships();
    invalidate();
    if (state_.phase == Phase::Home) (void)beginReturn(now, false);
    else if (state_.phase == Phase::Pose) { g.life = LifePhase::Pose; g.phaseStartedMs = now; g.deadlineMs = now + kFiniteTimeoutMs; }
    else { g.phaseStartedMs = now; g.deadlineMs = now; }
}
GameplayUpdate Explorer::beginReturn(std::uint64_t now, bool defeated) {
    invalidate();
    state_.phase = Phase::Home;
    auto& g = state_.gameplay;
    g.life = LifePhase::ReturnHome; g.defeated = defeated; g.homeScene = false;g.sceneBeat=SceneBeat::None;g.fadeInOnly=false;g.walkStartedMs=0;
    g.companion.walkStartedMs=0;
    g.partyActions.clear();g.partyPlaybacks.clear();g.partyPhase=PartyPhase::Idle;
    g.companionIntent=CompanionIntent::Follow;g.companionTarget=0;
    g.phaseStartedMs = now; g.deadlineMs = now + kFadeMs;
    const auto once = "return:" + std::to_string(state_.runId);
    if (!g.onceKeys.insert(once).second) throw std::logic_error("duplicate return transaction");
    return {GameplayEvent::ReturnHome, once};
}
bool Explorer::canResetExploration() const {
    const auto& g=state_.gameplay;
    return g.enabled && state_.phase==Phase::Explore &&
        (g.life==LifePhase::Explore || g.life==LifePhase::Combat || g.life==LifePhase::Dialogue);
}
GameplayUpdate Explorer::resetExploration(std::uint64_t now) {
    if(!canResetExploration()) return {};
    auto& g=state_.gameplay;
    g.onceKeys.erase(pendingHit(state_)); // Cancel only the attack that has not committed damage yet.
    g.onceKeys.insert("manual-reset:"+std::to_string(state_.runId));
    g.movement={};g.walkStartedMs=0;g.navigationHistory.clear();g.playerHidden=false;
    g.companion.movement={};g.companion.walkStartedMs=0;g.companion.hidden=false;
    g.damage=0;g.hit=g.critical=false;
    for(auto& e:state_.population.entities){e.movement={};e.tracking=Tracking::Idle;e.lastSeen=e.chaseTarget=-1;e.blockedTurns=0;}
    retreatProgress_=false;
    return beginReturn(now,false);
}
void Explorer::completeEncounter() {
    auto& g = state_.gameplay;
    auto it = std::find_if(state_.population.entities.begin(), state_.population.entities.end(), [&](const auto& e) { return e.instanceId == g.enemyInstance; });
    if (it == state_.population.entities.end()) throw std::logic_error("combat entity missing");
    // Preserve the B-1 encounter journal contract; the separate combat journal records victory.
    state_.onceKeys.insert(key("greet", it->instanceId));
    it->done = true;
    invalidate(state_.targetInstance==it->instanceId);
}
GameplayUpdate Explorer::advanceGameplay(std::uint64_t now,bool finished) {
    // Legacy headless driver: completion belongs to the active clip's actor.
    const auto& g=state_.gameplay;ActorCompletion result;
    if((g.life==LifePhase::Dialogue && g.dialogueStage==DialogueStage::Npc) || (g.life==LifePhase::Combat && (g.combat==CombatPhase::EnemyAttack || g.combat==CombatPhase::EnemyHurt || g.combat==CombatPhase::Faint))) result.enemy=finished;
    else {result.player=finished;result.partner=finished;}
    return advanceGameplay(now,result);
}
GameplayUpdate Explorer::advanceGameplay(std::uint64_t now, ActorCompletion completed) {
    if (!state_.gameplay.enabled) throw std::logic_error("gameplay is disabled");
    auto& g = state_.gameplay;
    const bool enemyOwner=g.life==LifePhase::Combat && (g.combat==CombatPhase::EnemyAttack || g.combat==CombatPhase::EnemyHurt || g.combat==CombatPhase::Faint);
    const bool partnerOwner=g.life==LifePhase::Combat && (g.combat==CombatPhase::CompanionAttack || g.combat==CombatPhase::CompanionHurt || g.combat==CombatPhase::CompanionFaint);
    const bool finished=enemyOwner?completed.enemy:partnerOwner?completed.partner:completed.player;
    const bool finiteDone=finished || now>=g.deadlineMs;
    auto life = [&](LifePhase phase, GameplayEvent event, std::uint64_t duration) {
        g.life = phase; g.phaseStartedMs = now; g.deadlineMs = now + duration;
        return GameplayUpdate{event, {}};
    };
    auto combat = [&](CombatPhase phase, GameplayEvent event, std::uint64_t duration = kFiniteTimeoutMs) {
        g.combat = phase; g.phaseStartedMs = now; g.deadlineMs = now + duration;
        return GameplayUpdate{event, {}};
    };
    if(g.life==LifePhase::Death) {
        if(now<g.deadlineMs) return {};
        if(g.deathStage==DeathStage::FadeOut) {
            g.deathStage=DeathStage::Blackout;g.phaseStartedMs=now;g.deadlineMs=now+1000;
            return {GameplayEvent::Blackout,{}};
        }
        auto result=beginReturn(now,true);g.homeScene=true;g.fadeInOnly=true;g.playerHidden=false;g.companion.hidden=false;g.deadlineMs=now+750;
        return result;
    }
    if(g.life==LifePhase::Explore && (g.sceneBeat==SceneBeat::FloorFadeOut || g.sceneBeat==SceneBeat::FloorFadeIn)) {
        if(now<g.deadlineMs) return {};
        if(g.sceneBeat==SceneBeat::FloorFadeIn) {
            g.sceneBeat=SceneBeat::None;g.phaseStartedMs=g.deadlineMs=now;
            return {GameplayEvent::Explore,{}};
        }
        g.sceneBeat=SceneBeat::None;
        const auto event=advance(); // The existing stairs journal and generation run once, at black.
        g.phaseStartedMs=now;g.walkStartedMs=0;
        if(event.kind==EventKind::Pose) {
            g.life=LifePhase::Pose;g.deadlineMs=now+kFiniteTimeoutMs;
            return {GameplayEvent::Pose,event.onceKey};
        }
        if(event.kind!=EventKind::FloorChanged) throw std::logic_error("stairs transition did not advance");
        g.sceneBeat=SceneBeat::FloorFadeIn;g.deadlineMs=now+kFloorFadeMs;
        return {GameplayEvent::FloorChanged,event.onceKey};
    }
    if(g.life==LifePhase::Explore && g.sceneBeat==SceneBeat::ExplorationBerryText) {
        if(now<g.deadlineMs) return {};
        g.sceneBeat=SceneBeat::None;g.phaseStartedMs=g.deadlineMs=now;
        return {GameplayEvent::Explore,{}};
    }
    if(g.independentParty && (g.life==LifePhase::Explore || g.life==LifePhase::Combat) && g.sceneBeat==SceneBeat::None) return advanceParty(now,completed);
    if (g.life == LifePhase::Pose) {
        if (!finiteDone || (g.companion.enabled && !completed.partner && now<g.deadlineMs)) return {};
        (void)advance(true);
        return beginReturn(now, false);
    }
    if (g.life == LifePhase::ReturnHome) {
        if (now < g.deadlineMs) return {};
        if (!g.homeScene) {
            g.homeScene = true;
            g.deadlineMs = g.phaseStartedMs + 2 * kFadeMs;
            // On restore/late update, present the home scene once; skip accumulated render frames.
            return {GameplayEvent::HomeScene, {}};
        }
        if (g.defeated) return life(LifePhase::Emotion, GameplayEvent::Emotion, kFiniteTimeoutMs);
        g.sceneBeat=SceneBeat::DiningPan;
        auto update = life(LifePhase::Eat, GameplayEvent::Eat, kMealMs);
        update.onceKey = "meal:" + std::to_string(state_.runId);
        if (g.onceKeys.insert(update.onceKey).second) ++g.meals;
        return update;
    }
    if (g.life == LifePhase::Emotion) {
        if (!finiteDone) return {};
        g.sceneBeat=SceneBeat::DiningPan;
        auto update = life(LifePhase::Eat, GameplayEvent::Eat, kMealMs);
        update.onceKey = "meal:" + std::to_string(state_.runId);
        if (g.onceKeys.insert(update.onceKey).second) ++g.meals;
        return update;
    }
    if(g.life==LifePhase::Dialogue) {
        if(now<g.deadlineMs) return {};
        if(g.dialogueStage==DialogueStage::Player) {
            if(!completed.player) return {};
            g.dialogueStage=DialogueStage::Npc;g.phaseStartedMs=now;g.deadlineMs=now+1200;
            return {GameplayEvent::DialogueNpc,{}};
        }
        if(!completed.enemy) return {};
        auto& e=*std::find_if(state_.population.entities.begin(),state_.population.entities.end(),[&](const auto& x){return x.instanceId==g.enemyInstance;});
        const auto once=key("greet",e.instanceId);e.done=true;state_.onceKeys.insert(once);
        if(state_.goalKind==GoalKind::Friend) invalidate();
        return life(LifePhase::Explore,GameplayEvent::Explore,0);
    }
    if(g.life==LifePhase::Explore && g.sceneBeat==SceneBeat::ExploreFadeIn) {
        if(now<g.deadlineMs) return {};
        g.sceneBeat=SceneBeat::None;g.phaseStartedMs=g.deadlineMs=now;return {GameplayEvent::Explore,{}};
    }
    if(g.life==LifePhase::Eat) {
        if(now<g.deadlineMs) return {};
        g.sceneBeat=SceneBeat::DiningExit;return life(LifePhase::Sleep,GameplayEvent::Sleep,750);
    }
    if(g.life==LifePhase::Sleep) {
        switch(g.sceneBeat) {
        case SceneBeat::DiningExit:
            if(now<g.deadlineMs) return {};
            g.sceneBeat=SceneBeat::NightFadeIn;return life(LifePhase::Sleep,GameplayEvent::HomeScene,kFadeMs);
        case SceneBeat::NightFadeIn: case SceneBeat::BedFadeIn:
            if(now<g.deadlineMs) return {};
            g.sceneBeat=SceneBeat::BedFace;return life(LifePhase::Sleep,GameplayEvent::Sleep,kFiniteTimeoutMs);
        case SceneBeat::BedFace:
            if(!(completed.player&&completed.partner)) return {};
            g.sceneBeat=SceneBeat::BedLay;return life(LifePhase::Sleep,GameplayEvent::Sleep,400);
        case SceneBeat::BedLay:
            if(now<g.deadlineMs || !(completed.player&&completed.partner)) return {};
            g.sceneBeat=SceneBeat::Sleeping;return life(LifePhase::Sleep,GameplayEvent::Sleep,kFiniteTimeoutMs);
        case SceneBeat::Sleeping: case SceneBeat::None:
            if(!(completed.player&&completed.partner) && now<g.deadlineMs) return {};
            g.sceneBeat=SceneBeat::BedFadeOut;return life(LifePhase::Sleep,GameplayEvent::Sleep,kFadeMs);
        case SceneBeat::BedFadeOut:
            if(now<g.deadlineMs) return {};
            g.sceneBeat=SceneBeat::BedFadeIn;return life(LifePhase::Wake,GameplayEvent::HomeScene,kFadeMs);
        default: // Import a previous bedtime substage without flashing the day background.
            g.sceneBeat=SceneBeat::NightFadeIn;return life(LifePhase::Sleep,GameplayEvent::HomeScene,kFadeMs);
        }
    }
    if(g.life==LifePhase::Wake) {
        if(g.sceneBeat==SceneBeat::WakeDelay || g.sceneBeat==SceneBeat::BedFadeIn) {
            if(now<g.deadlineMs) return {};
            g.sceneBeat=SceneBeat::Waking;return life(LifePhase::Wake,GameplayEvent::Wake,kFiniteTimeoutMs);
        }
        if(!(completed.player&&completed.partner)) return {};
        g.sceneBeat=SceneBeat::MorningFace;
        return life(LifePhase::Ready,GameplayEvent::Ready,kFiniteTimeoutMs);
    }
    if(g.life==LifePhase::Ready) {
        if(g.sceneBeat==SceneBeat::MorningFace && !(completed.player&&completed.partner)) return {};
        if(g.sceneBeat!=SceneBeat::RoomExit && g.sceneBeat!=SceneBeat::MorningFadeOut) { // Resume old morning saves through the exit.
            g.sceneBeat=SceneBeat::RoomExit;g.playerDirection=0;
            return life(LifePhase::Ready,GameplayEvent::Ready,kRoomExitMs);
        }
        if(g.sceneBeat==SceneBeat::RoomExit) {
            if(now<g.deadlineMs || !completed.player) return {};
            g.sceneBeat=SceneBeat::MorningFadeOut;
            return life(LifePhase::Ready,GameplayEvent::Ready,kFadeMs);
        }
        if(now<g.deadlineMs) return {};
        if(state_.runId==UINT64_MAX || g.day==UINT32_MAX) throw std::runtime_error("day counter exhausted");
        const auto previous=g;const auto inventory=state_.inventory;
        Explorer next(state_.runId+1,state_.rng.state,
            nextRunAssetVersion_.empty()?state_.assetVersion:nextRunAssetVersion_,
            nextRunFloors_.empty()?state_.floors:nextRunFloors_,state_.spawnPolicy,recentLayouts());
        next.state_.inventory=inventory;
        for(const auto& entry:inventory) next.state_.onceKeys.insert(entry.onceKey);
        next.state_.gameplay.enabled=true;next.state_.gameplay.day=previous.day+1;next.state_.gameplay.meals=previous.meals;next.state_.gameplay.oran=previous.oran;
        next.state_.gameplay.phaseStartedMs=now;next.state_.gameplay.deadlineMs=now+kFadeMs;
        next.state_.gameplay.sceneBeat=SceneBeat::ExploreFadeIn;
        if(previous.companion.enabled) {next.state_.gameplay.enabled=true;next.enableCompanion();next.state_.gameplay.companion.oran=previous.companion.oran;}
        next.assignRelationships();*this=std::move(next);return {GameplayEvent::Explore,{}};
    }
    if (g.life == LifePhase::Combat) {
        switch (g.combat) {
        case CombatPhase::PlayerTurn: {
            if (now < g.deadlineMs) return {};
            if(g.playerActionRound==g.actionRound) return afterCombatAction(now);
            if(mustRetreat()) {
                g.life=LifePhase::Explore;invalidate();
                return scheduleDungeon(now);
            }
            g.playerActionRound=g.actionRound;
            auto& enemy=*std::find_if(state_.population.entities.begin(),state_.population.entities.end(),[&](const auto& e){return e.instanceId==g.enemyInstance;});
            if(!canAttack(state_.world,state_.position,enemy.tile)) {
                const int next=combatApproach(enemy,false);
                if(next<0) return afterCombatAction(now);
                const int old=state_.position;state_.position=next;state_.visited[next]=1;state_.floorSteps=std::min(kFloorStepLimit+kTiles,state_.floorSteps+1);state_.stalledActions=std::min(kFloorStepLimit+kTiles,state_.stalledActions+1);++state_.occupancyVersion;invalidate(false);
                g.playerDirection=directionBetween(old,next);g.movement={g.actionRound,now,400,g.playerDirection,old,next};
                if(!g.walkStartedMs)g.walkStartedMs=now;
                return combat(CombatPhase::PlayerApproach,GameplayEvent::Move,400);
            }
            const auto once = "turn:" + std::to_string(state_.runId) + ":" + std::to_string(state_.floorIndex) + ":" + std::to_string(g.enemyInstance) + ":" + std::to_string(++g.turn);
            if (!g.onceKeys.insert(once).second) throw std::logic_error("duplicate combat turn");
            if (g.oran && g.hearts+2 <= kMaxHp) {
                --g.oran; g.hearts = std::min(kMaxHp, g.hearts + 2);
                auto update = combat(CombatPhase::BerryText, GameplayEvent::UseOranBerry, 1000); update.onceKey = once;
                return update;
            }
            const auto roll = rollBasicAttack(state_.rng, true);
            g.hit = roll.hit; g.critical = roll.critical; g.damage = roll.damage;
            g.enemyHits -= std::min(g.enemyHits, roll.damage);
            for(auto& e:state_.population.entities) if(e.instanceId==g.enemyInstance) e.remainingHits=g.enemyHits;
            auto update = combat(CombatPhase::PlayerAttack, GameplayEvent::PlayerAttack); update.onceKey = once; update.damage = roll.damage; update.critical = roll.critical;
            return update;
        }
        case CombatPhase::PlayerAttack:
            if (!finiteDone) return {};
            if (g.hit) return combat(CombatPhase::EnemyHurt, GameplayEvent::Hurt);
            return afterCombatAction(now);
        case CombatPhase::PlayerApproach: case CombatPhase::CompanionApproach:
            if(now<g.deadlineMs) return {};
            g.walkStartedMs=0;g.companion.walkStartedMs=0;
            return afterCombatAction(now);
        case CombatPhase::CompanionTurn: {
            if(now<g.deadlineMs) return {};
            auto& partner=g.companion;
            if(!partner.enabled || !partner.hearts || partner.actionRound==g.actionRound) return afterCombatAction(now);
            partner.actionRound=g.actionRound;partner.walkStartedMs=0;
            const auto once="companion-turn:"+std::to_string(state_.runId)+":"+std::to_string(state_.floorIndex)+":"+std::to_string(g.enemyInstance)+":"+std::to_string(g.actionRound);
            if(!g.onceKeys.insert(once).second) throw std::logic_error("duplicate companion turn");
            if(partner.oran && partner.hearts+2<=kMaxHp) {
                --partner.oran;partner.hearts+=2;
                auto update=combat(CombatPhase::CompanionBerryText,GameplayEvent::UseOranBerry,1000);update.onceKey=once;update.actorId=kCompanionActorId;return update;
            }
            if(mustRetreat()) return afterCombatAction(now);
            const auto& enemy=*std::find_if(state_.population.entities.begin(),state_.population.entities.end(),[&](const auto& e){return e.instanceId==g.enemyInstance;});
            partner.direction=directionBetween(partner.tile,enemy.tile);
            if(!canAttack(state_.world,partner.tile,enemy.tile)) {
                const int next=combatApproach(enemy,true);
                if(next<0) return afterCombatAction(now);
                const int old=partner.tile;partner.tile=next;recordCompanionMove(old,now);++state_.occupancyVersion;invalidate(false);
                auto update=combat(CombatPhase::CompanionApproach,GameplayEvent::CompanionMove,400);update.actorId=kCompanionActorId;return update;
            }
            const auto roll=rollBasicAttack(state_.rng,true);g.hit=roll.hit;g.critical=roll.critical;g.damage=roll.damage;
            g.enemyHits-=std::min(g.enemyHits,roll.damage);
            for(auto& e:state_.population.entities)if(e.instanceId==g.enemyInstance)e.remainingHits=g.enemyHits;
            auto update=combat(CombatPhase::CompanionAttack,GameplayEvent::CompanionAttack);update.onceKey=once;update.damage=roll.damage;update.critical=roll.critical;update.actorId=kCompanionActorId;return update;
        }
        case CombatPhase::CompanionAttack:
            if(!finiteDone) return {};
            if(g.hit) return combat(CombatPhase::EnemyHurt,GameplayEvent::Hurt);
            return afterCombatAction(now);
        case CombatPhase::EnemyHurt:
            if (!finiteDone) return {};
            return !g.enemyHits ? combat(CombatPhase::Faint, GameplayEvent::EnemyFaint) : afterCombatAction(now);
        case CombatPhase::BerryText: case CombatPhase::CompanionBerryText:
            if (now < g.deadlineMs) return {};
            return afterCombatAction(now);
        case CombatPhase::EnemyTurn: {
            if (now < g.deadlineMs) return {};
            auto& enemy=*std::find_if(state_.population.entities.begin(),state_.population.entities.end(),[&](const auto& e){return e.instanceId==g.enemyInstance;});
            if(enemy.actionRound==g.actionRound) return afterCombatAction(now);
            enemy.actionRound=g.actionRound;
            const int target=nearestPartyTile(enemy);g.enemyTargetsCompanion=g.companion.enabled && target==g.companion.tile;
            enemy.direction=directionBetween(enemy.tile,target);
            if(!canAttack(state_.world,enemy.tile,target)) return afterCombatAction(now);
            const auto once = "enemy:" + std::to_string(state_.runId) + ":" + std::to_string(state_.floorIndex) + ":" + std::to_string(g.enemyInstance) + ":" + std::to_string(g.actionRound);
            if (!g.onceKeys.insert(once).second) throw std::logic_error("duplicate enemy turn");
            const auto roll = rollBasicAttack(state_.rng, false);
            g.hit = roll.hit; g.critical = roll.critical; g.damage = roll.damage;
            // The rolled attack and pending impact are durable; damage commits only after Attack1 finishes.
            if(roll.hit) g.onceKeys.insert(pendingHit(state_));
            auto update = combat(CombatPhase::EnemyAttack, GameplayEvent::EnemyAttack); update.onceKey = once; update.damage = roll.damage; update.critical = roll.critical;
            return update;
        }
        case CombatPhase::EnemyAttack:
            if (!finiteDone) return {};
            if(g.onceKeys.erase(pendingHit(state_))) {
                auto& hp=g.enemyTargetsCompanion?g.companion.hearts:g.hearts;hp-=std::min(hp,g.damage);
            }
            return g.hit ? combat(g.enemyTargetsCompanion?CombatPhase::CompanionHurt:CombatPhase::PlayerHurt, GameplayEvent::Hurt) : afterCombatAction(now);
        case CombatPhase::PlayerHurt:
            if (!finiteDone) return {};
            return !g.hearts ? combat(CombatPhase::PlayerFaint, GameplayEvent::PlayerFaint) : afterCombatAction(now);
        case CombatPhase::CompanionHurt:
            if(!finiteDone) return {};
            return !g.companion.hearts?combat(CombatPhase::CompanionFaint,GameplayEvent::CompanionFaint):afterCombatAction(now);
        case CombatPhase::Faint:
            if (!finiteDone) return {};
            completeEncounter();
            g.onceKeys.insert("victory:" + key("combat", g.enemyInstance));
            return combat(CombatPhase::Victory, GameplayEvent::CombatVictory, 350);
        case CombatPhase::Victory:
            if (now < g.deadlineMs) return {};
            return life(LifePhase::Explore, GameplayEvent::Explore, 350);
        case CombatPhase::PlayerFaint:
            if(!completed.player) return {};
            invalidate();state_.phase=Phase::Home;g.playerHidden=true;g.deathStage=DeathStage::FadeOut;
            return life(LifePhase::Death,GameplayEvent::CombatDefeat,750);
        case CombatPhase::CompanionFaint:
            if(!completed.partner) return {};
            invalidate();state_.phase=Phase::Home;g.companion.hidden=true;g.deathStage=DeathStage::FadeOut;
            return life(LifePhase::Death,GameplayEvent::CombatDefeat,750);
        case CombatPhase::Defeat:
            if (now < g.deadlineMs) return {};
            return combat(CombatPhase::PlayerFaint, GameplayEvent::PlayerFaint);
        }
    }
    if (now < g.deadlineMs) return {};
    return scheduleDungeon(now);
}
void Explorer::shiftPausedClocks(std::uint64_t delta,std::uint64_t pausedAt) {
    auto& g=state_.gameplay;
    {
        if(g.deadlineMs>UINT64_MAX-delta) throw std::runtime_error("pause clock overflow");
        g.phaseStartedMs+=delta;g.deadlineMs+=delta;
        if(g.walkStartedMs) g.walkStartedMs+=delta;
    }
    auto shift=[&](MoveRecord& move) {
        if(move.actionId && pausedAt>=move.startedMs && pausedAt-move.startedMs<move.durationMs) {
            if(delta>UINT64_MAX-move.durationMs || move.startedMs>UINT64_MAX-delta-move.durationMs) throw std::runtime_error("movement clock overflow");
            move.startedMs+=delta;
        }
    };
    if(g.companion.walkStartedMs) g.companion.walkStartedMs+=delta;
    shift(g.movement);shift(g.companion.movement);for(auto& e:state_.population.entities) shift(e.movement);
    for(auto& a:g.partyActions){
        if(a.startedMs>UINT64_MAX-delta || a.deadlineMs>UINT64_MAX-delta)throw std::runtime_error("party pause clock overflow");
        a.startedMs+=delta;a.deadlineMs+=delta;
    }
    for(auto& v:g.partyPlaybacks){
        if(v.startedMs>UINT64_MAX-delta || v.deadlineMs>UINT64_MAX-delta)throw std::runtime_error("party playback pause overflow");
        v.startedMs+=delta;v.deadlineMs+=delta;
    }
}
GameplayUpdate Explorer::beginCombat(Entity& e,std::uint64_t now,CombatPhase first) {
    invalidate(false);auto& g=state_.gameplay;
    g.life=LifePhase::Combat;g.walkStartedMs=0;g.companion.walkStartedMs=0;g.combat=first;g.enemyInstance=e.instanceId;g.enemyHits=e.remainingHits;
    const int target=nearestPartyTile(e);g.enemyTargetsCompanion=g.companion.enabled && target==g.companion.tile;
    g.playerDirection=directionBetween(state_.position,e.tile);e.direction=directionBetween(e.tile,target);
    if(g.companion.enabled)g.companion.direction=directionBetween(g.companion.tile,e.tile);
    g.phaseStartedMs=now;g.deadlineMs=now+350;
    return {GameplayEvent::HostileEncounter,{}};
}
GameplayUpdate Explorer::afterCombatAction(std::uint64_t now) {
    auto& g=state_.gameplay;
    auto& e=*std::find_if(state_.population.entities.begin(),state_.population.entities.end(),[&](const auto& x){return x.instanceId==g.enemyInstance;});
    g.phaseStartedMs=now;g.deadlineMs=now+350;
    if(g.playerActionRound!=g.actionRound) {
        if(mustRetreat()) {g.life=LifePhase::Explore;g.deadlineMs=now;invalidate();return {GameplayEvent::Explore,{}};}
        g.combat=CombatPhase::PlayerTurn;return {};
    }
    if(g.companion.enabled && g.companion.hearts && g.companion.actionRound!=g.actionRound) {g.combat=CombatPhase::CompanionTurn;return {};}
    if(e.actionRound!=g.actionRound) {g.combat=CombatPhase::EnemyTurn;return {};}
    const bool pending=std::any_of(state_.population.entities.begin(),state_.population.entities.end(),[&](const auto& x){
        return !x.done && x.visible && x.kind==EntityKind::Friend && x.actionRound!=g.actionRound &&
            std::find(g.hostile.begin(),g.hostile.end(),x.instanceId)!=g.hostile.end();});
    if(pending) {g.life=LifePhase::Explore;return {GameplayEvent::Explore,{}};}
    if(g.actionRound==UINT64_MAX) throw std::runtime_error("action round exhausted");
    ++g.actionRound;
    if(mustRetreat()) {g.life=LifePhase::Explore;g.deadlineMs=now;invalidate();return {GameplayEvent::Explore,{}};}
    g.combat=CombatPhase::PlayerTurn;return {};
}
int Explorer::detourTile(unsigned repetitions,uint64_t signature) {
    if(!state_.gameplay.enabled && state_.floorSteps>=kFloorStepLimit+kTiles) return -1;
    auto occupied=occupancy(true);
    if(state_.goalKind!=GoalKind::Stairs) occupied[state_.world.stairs]=1;
    const int planned=pathIndex_<path_.size()?path_[pathIndex_]:-1;
    const auto& previous=state_.gameplay.movement;
    const int back=previous.actionId && previous.to==state_.position?previous.from:-1;
    struct Candidate {int tile,threat,visited,distance;};
    std::vector<Candidate> choices;
    for(auto [dx,dy]:kDirections) {
        const int x=state_.position%kWidth+dx,y=state_.position/kWidth+dy;
        if(x<0 || x>=kWidth || y<0 || y>=kHeight) continue;
        const int tile=y*kWidth+x;
        if(!canMove(state_.world,state_.position,tile,occupied)) continue;
        if(state_.gameplay.companion.enabled && !state_.gameplay.independentParty && companionFollowTile(tile)<0) continue;
        int threat=0;
        for(const auto& e:state_.population.entities)
            if(e.kind==EntityKind::Friend && e.visible && !e.done && !friendly(e) && canAttack(state_.world,e.tile,tile)) ++threat;
        const auto reach=bfs(state_.world,tile,occupied);++bfsCalls_;
        const int distance=state_.target>=0 && reach.distance[state_.target]>=0?reach.distance[state_.target]:kTiles+1;
        choices.push_back({tile,threat,state_.visited[tile],distance});
    }
    if(choices.empty()) return -1;
    // Prefer leaving both ends of the orbit; a corridor may have only one exit.
    if(std::any_of(choices.begin(),choices.end(),[&](const auto& c){return c.tile!=planned && c.tile!=back;}))
        choices.erase(std::remove_if(choices.begin(),choices.end(),[&](const auto& c){return c.tile==planned || c.tile==back;}),choices.end());
    else if(std::any_of(choices.begin(),choices.end(),[&](const auto& c){return c.tile!=planned;}))
        choices.erase(std::remove_if(choices.begin(),choices.end(),[&](const auto& c){return c.tile==planned;}),choices.end());
    std::sort(choices.begin(),choices.end(),[](const auto& a,const auto& b){
        return std::tie(a.threat,a.visited,a.distance,a.tile)<std::tie(b.threat,b.visited,b.distance,b.tile);
    });
    const int next=choices[(repetitions-1)%choices.size()].tile;
    // Keep the current occurrence and restart the candidate rotation after a
    // full pass. This survives save/restore without saturating the 32-slot FIFO.
    if(repetitions>=choices.size()) {
        auto& history=state_.gameplay.navigationHistory;
        history.erase(std::remove(history.begin(),history.end(),signature),history.end());
        history.push_back(signature);
    }
    return next;
}
int Explorer::nearestPartyTile(const Entity& enemy) const {
    const auto& g=state_.gameplay;
    if(!g.companion.enabled || !g.companion.hearts) return state_.position;
    if(!g.hearts) return g.companion.tile;
    auto occupied=occupancy();occupied[enemy.tile]=0;
    occupied[state_.position]=0;occupied[g.companion.tile]=0;
    const auto reach=bfs(state_.world,enemy.tile,occupied);
    const int a=reach.distance[state_.position],b=reach.distance[g.companion.tile];
    return b>=0 && (a<0 || b<a)?g.companion.tile:state_.position;
}
int Explorer::combatApproach(const Entity& enemy,bool partner) const {
    const auto& g=state_.gameplay;
    const int from=partner?g.companion.tile:state_.position;
    const int other=partner?state_.position:g.companion.tile;
    if(partner && (state_.world.room[from]<0 || state_.world.room[from]!=state_.world.room[other])) return -1;
    auto occupied=occupancy();occupied[state_.position]=1;
    occupied[from]=0;
    for(auto [dx,dy]:kDirections) {
        const int x=from%kWidth+dx,y=from/kWidth+dy;
        if(x<0 || x>=kWidth || y<0 || y>=kHeight) continue;
        const int next=y*kWidth+x;
        if(next==state_.world.stairs || !canMove(state_.world,from,next,occupied)) continue;
        if(canAttack(state_.world,next,enemy.tile) && (!g.companion.enabled || canAttack(state_.world,next,other))) return next;
    }
    return -1;
}
GameplayUpdate Explorer::healCompanion(std::uint64_t now) {
    auto& g=state_.gameplay;auto& p=g.companion;
    if(!p.enabled || !p.hearts || !p.oran || p.hearts+2>kMaxHp || p.actionRound==g.actionRound) return {};
    --p.oran;p.hearts+=2;p.actionRound=g.actionRound;p.walkStartedMs=0;
    const auto once="companion-heal:"+std::to_string(state_.runId)+":"+std::to_string(state_.floorIndex)+":"+std::to_string(g.actionRound);
    if(!g.onceKeys.insert(once).second) throw std::logic_error("duplicate companion berry use");
    invalidate();g.sceneBeat=SceneBeat::ExplorationBerryText;g.phaseStartedMs=now;g.deadlineMs=now+1000;
    return {GameplayEvent::UseOranBerry,once,0,false,kCompanionActorId};
}
GameplayUpdate Explorer::scheduleDungeon(std::uint64_t now) {
    auto& g=state_.gameplay;
    retreatProgress_=false;
    if(state_.position==state_.world.stairs) {
        invalidate();g.movement={};g.walkStartedMs=0;
        g.sceneBeat=SceneBeat::FloorFadeOut;g.phaseStartedMs=now;g.deadlineMs=now+kFloorFadeMs;
        return {};
    }
    discover();
    unsigned repetitions=0;
    const auto signature=navigationFingerprint(state_);
    if(g.playerActionRound==g.actionRound) {
        repetitions=static_cast<unsigned>(std::count(g.navigationHistory.begin(),g.navigationHistory.end(),signature));
        if(g.navigationHistory.size()==32) g.navigationHistory.erase(g.navigationHistory.begin());
        g.navigationHistory.push_back(signature);
    }
    bool hostileReversed=false;
    auto hostile=[&](const Entity& e){return std::find(g.hostile.begin(),g.hostile.end(),e.instanceId)!=g.hostile.end();};
    auto visible=[&](int a,int b){
        const int room=state_.world.room[a];
        if(room>=0 && room==state_.world.room[b]) return true;
        const auto sight=bfs(state_.world,a);return sight.distance[b]>=0 && sight.distance[b]<=2;
    };
    // Player then each hostile in stable instance order. A movement completion never grants an action.
    if(g.playerActionRound==g.actionRound) {
        if(g.companion.enabled && g.companion.actionRound!=g.actionRound) {
            auto heal=healCompanion(now);if(heal.event!=GameplayEvent::None) return heal;
            if(!mustRetreat()) for(auto& e:state_.population.entities)
                if(e.kind==EntityKind::Friend && e.visible && !e.done && hostile(e) && canAttack(state_.world,g.companion.tile,e.tile))
                    return beginCombat(e,now,CombatPhase::CompanionTurn);
            g.companion.actionRound=g.actionRound;g.companion.walkStartedMs=0;
        }
        for(auto& e:state_.population.entities) {
            if(e.kind!=EntityKind::Friend || !e.visible || e.done || !hostile(e) || e.actionRound==g.actionRound) continue;
            const int target=nearestPartyTile(e);
            const bool inSight=visible(e.tile,target);
            if(inSight) {e.tracking=Tracking::Chase;e.lastSeen=target;e.chaseTarget=e.lastSeen;}
            if(canAttack(state_.world,e.tile,target)) return beginCombat(e,now,CombatPhase::EnemyTurn);
            e.actionRound=g.actionRound; // a move OR wait, never move followed by attack
            if(e.tracking==Tracking::Chase && e.lastSeen>=0) {
                if(!inSight && e.tile==e.lastSeen) {e.tracking=Tracking::Idle;e.chaseTarget=-1;e.blockedTurns=0;continue;}
                auto occupied=occupancy();occupied[e.tile]=0;occupied[state_.position]=1;
                const auto reach=bfs(state_.world,e.tile,occupied);++bfsCalls_;
                int goal=-1,best=kTiles+1;
                for(int p=0;p<kTiles;++p) if((p==e.lastSeen || (inSight && canAttack(state_.world,p,e.lastSeen))) && reach.distance[p]>0 && reach.distance[p]<best) {
                    goal=p;best=reach.distance[p];
                }
                if(goal>=0) {
                    const auto path=reach.pathTo(goal);const int old=e.tile;e.tile=path.front();
                    hostileReversed=hostileReversed || (e.movement.actionId && e.movement.to==old && e.movement.from==e.tile);
                    if(!lowHealth()) g.walkStartedMs=0;
                    e.direction=directionBetween(old,e.tile);e.movement={g.actionRound,now,400,e.direction,old,e.tile};e.blockedTurns=0;++state_.occupancyVersion;invalidate(false);
                    if(lowHealth() || repetitions) continue; // A detour also shares the current hostile movement interval.
                    g.phaseStartedMs=now;g.deadlineMs=now+400;
                    return {GameplayEvent::ActorMove,{},0,false,e.instanceId};
                }
                if(e.tile==e.lastSeen || (e.blockedTurns=std::min(3U,e.blockedTurns+1))>=3) {e.tracking=Tracking::Idle;e.chaseTarget=-1;}
            }
        }
        if(g.actionRound==UINT64_MAX) throw std::runtime_error("action round exhausted");
        ++g.actionRound;
    }
    if(g.oran && g.hearts+2<=kMaxHp && g.playerActionRound!=g.actionRound) {
        --g.oran;g.hearts+=2;g.playerActionRound=g.actionRound;g.walkStartedMs=0;
        const auto once="heal:"+std::to_string(state_.runId)+":"+std::to_string(state_.floorIndex)+":"+std::to_string(g.actionRound);
        if(!g.onceKeys.insert(once).second) throw std::logic_error("duplicate exploration berry use");
        invalidate();g.sceneBeat=SceneBeat::ExplorationBerryText;g.phaseStartedMs=now;g.deadlineMs=now+1000;
        return {GameplayEvent::UseOranBerry,once};
    }
    auto companionHeal=healCompanion(now);if(companionHeal.event!=GameplayEvent::None) return companionHeal;
    if(state_.target<0 || lowHealth()) plan();
    if(lowHealth() && hostileReversed && g.movement.actionId && g.movement.to==state_.position &&
       !path_.empty() && path_.front()==g.movement.from) {
        // Both bodies would reverse the preceding joint move. The shifting safety
        // mask is orbiting the threat: keep real occupancy but release the future
        // attack-zone mask for this step so the goal can make progress.
        retreatProgress_=true;plan();
    }
    // Complete a pickup underfoot before making any detour.
    const bool reroute=repetitions && !(state_.target==state_.position && state_.goalKind==GoalKind::Item);
    const int detour=reroute?detourTile(repetitions,signature):-1;
    int nextTile=detour;
    if(!reroute && state_.target>=0 && std::any_of(state_.population.entities.begin(),state_.population.entities.end(),[&](const auto& e) {
        return friendly(e) && !e.done && canAttack(state_.world,state_.position,e.tile);
    })) {
        auto occupied=playerOccupancy();if(state_.goalKind!=GoalKind::Stairs) occupied[state_.world.stairs]=1;
        const auto route=bfs(state_.world,state_.position,occupied).pathTo(state_.target);++bfsCalls_;
        if(!route.empty()) nextTile=route.front();
    }
    for(auto& e:state_.population.entities) {
        if(e.done||!e.visible||e.kind!=EntityKind::Friend||!canAttack(state_.world,state_.position,e.tile)||state_.blocked[e.tile]) continue;
        if(hostile(e)) {if(!lowHealth()) return beginCombat(e,now,CombatPhase::PlayerTurn);continue;}
        if(e.tile!=nextTile && (state_.goalKind!=GoalKind::Friend || state_.targetInstance!=e.instanceId)) continue;
        g.enemyInstance=e.instanceId;g.dialogueStage=DialogueStage::Player;
        g.playerDirection=directionBetween(state_.position,e.tile);e.direction=directionBetween(e.tile,state_.position);
        g.walkStartedMs=0;g.life=LifePhase::Dialogue;g.phaseStartedMs=now;g.deadlineMs=now+1200;
        return {GameplayEvent::DialoguePlayer,{}};
    }
    const int old=state_.position;
    const int oldCompanion=g.companion.tile;
    const auto oldActors=state_.population.entities;
    Event update;
    if(reroute) {
        if(detour>=0) update=moveTo(detour);
        invalidate(false); // Resume the same goal from the new location next turn.
    } else update=advance();
    g.phaseStartedMs=now;g.deadlineMs=now+(update.kind==EventKind::Move?400:0);
    recordCompanionMove(oldCompanion,now);
    if(update.kind==EventKind::Move) {
        for(size_t index=0;index<state_.population.entities.size();++index) {
            auto& e=state_.population.entities[index];
            if(friendly(e) && e.tile!=oldActors[index].tile) e.movement={g.actionRound,now,400,e.direction,oldActors[index].tile,e.tile};
        }
        if(!g.walkStartedMs) g.walkStartedMs=now;
        g.playerActionRound=g.actionRound;g.playerDirection=directionBetween(old,state_.position);g.movement={g.actionRound,now,400,g.playerDirection,old,state_.position};}
    if(update.kind!=EventKind::Move) g.walkStartedMs=0;
    switch(update.kind) {
    case EventKind::Pose:
        g.life=LifePhase::Pose;g.deadlineMs=now+5000;return {GameplayEvent::Pose,update.onceKey};
    case EventKind::ReturnedHome:
        if(state_.completed)return beginReturn(now,false);
        // A temporary empty plan is not a completed dungeon or a reset.
        // Actor occupancy may have made a known goal fail in an earlier round.
        state_.phase=Phase::Explore;state_.failedTargets.fill(0);invalidate();
        for(auto& enemy:state_.population.entities)
            if(enemy.kind==EntityKind::Friend && enemy.visible && !enemy.done &&
               hostile(enemy) && canAttack(state_.world,state_.position,enemy.tile)) {
                g.onceKeys.insert("cornered:"+key("combat",enemy.instanceId));
                return beginCombat(enemy,now,CombatPhase::PlayerTurn);
            }
        g.playerActionRound=g.actionRound;g.deadlineMs=now+400;
        return {GameplayEvent::Wait,{}};
    case EventKind::Pickup:
        g.playerActionRound=g.actionRound;g.deadlineMs=now+400;
        if(update.originalId==ITEM_ORAN_BERRY) {
            auto& p=g.companion;
            if(p.enabled && !p.oran && (g.oran || p.hearts<g.hearts)) ++p.oran;
            else if(!g.oran) ++g.oran;
        }
        return {GameplayEvent::Pickup,update.onceKey};
    case EventKind::Move:return {GameplayEvent::Move,{},0,false,0,reroute};
    case EventKind::Greeting:return {GameplayEvent::Greeting,update.onceKey};
    case EventKind::FloorChanged:return {GameplayEvent::FloorChanged,update.onceKey};
    default:
        g.playerActionRound=g.actionRound;g.deadlineMs=now+400;
        if(reroute) {
            // Planning may have dropped an unreachable goal. Keep the waiting
            // decision detectable as well, so a restored dead end keeps waiting
            // for an opening instead of taking the no-goal return fallback.
            const auto waiting=navigationFingerprint(state_);
            if(std::find(g.navigationHistory.begin(),g.navigationHistory.end(),waiting)==g.navigationHistory.end()) {
                if(g.navigationHistory.size()==32) g.navigationHistory.erase(g.navigationHistory.begin());
                g.navigationHistory.push_back(waiting);
            }
        }
        return {GameplayEvent::Wait,{},0,false,0,reroute};
    }
}

} // namespace pet
