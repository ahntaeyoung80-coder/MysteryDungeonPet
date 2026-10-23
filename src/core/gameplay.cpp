#include "core/exploration.hpp"
#include "core/gameplay_notice.hpp"
#include <algorithm>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <tuple>

namespace pet {
namespace {
constexpr std::uint64_t kFiniteTimeoutMs = 5000, kFadeMs = 750;
std::string pendingHit(const ExplorationState& s) {
    const auto& g=s.gameplay;
    return "pending-hit:"+std::to_string(s.runId)+":"+std::to_string(s.floorIndex)+":"+
        std::to_string(g.enemyInstance)+":"+std::to_string(g.actionRound);
}
}
uint64_t navigationFingerprint(const ExplorationState& s) {
    uint64_t hash=14695981039346656037ULL;
    auto add=[&](uint64_t value){for(unsigned byte=0;byte<8;++byte){hash^=uint8_t(value>>(byte*8));hash*=1099511628211ULL;}};
    add(s.screenExploration);add(s.viewportSize);
    add(s.floorIndex);add(s.world.version);add(s.mapSeed);add(s.rng.state);add(s.position);
    add(s.target+1);add(static_cast<unsigned>(s.goalKind));add(s.targetInstance);add(s.knownStairs+1);
    add(s.inventory.size());add(s.onceKeys.size());add(s.gameplay.onceKeys.size());add(s.gameplay.hearts);add(s.gameplay.oran);
    const auto& partner=s.gameplay.companion;
    if(partner.enabled){add(partner.tile);add(partner.hearts);add(partner.oran);add(partner.actionRound==s.gameplay.actionRound);}
    for(int tile=0;tile<kTiles;++tile){add(s.screenSeen[tile]);add(s.visited[tile]);add(s.discovered[tile]);add(s.failedTargets[tile]);add(s.blocked[tile]);}
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
bool completionScene(const GameplayState& g) {
    return g.life==LifePhase::Pose && g.sceneBeat>=SceneBeat::CompletionFadeIn && g.sceneBeat<=SceneBeat::CompletionFadeOut;
}
bool bedroomScene(const GameplayState& g) {
    return g.homeScene && ((g.life==LifePhase::Sleep && g.sceneBeat!=SceneBeat::DiningExit) ||
        g.life==LifePhase::Wake || g.life==LifePhase::Ready);
}
bool nightScene(const GameplayState& g) {
    return bedroomScene(g) && g.life!=LifePhase::Wake && g.life!=LifePhase::Ready;
}
bool showFriendlyDisc(const GameplayState& g,bool friendly,bool player) {
    return dungeonScene(g) && g.life!=LifePhase::Dialogue && friendly && !player;
}
bool companionDialogue(const GameplayState& g) {
    return g.life==LifePhase::Dialogue && g.enemyInstance==kCompanionActorId;
}
std::uint32_t gameplayClip(const GameplayState& g) {
    if(g.independentParty && !g.homeScene && (g.life==LifePhase::Explore || g.life==LifePhase::Combat) && g.sceneBeat==SceneBeat::None) return partyClip(g,0);
    switch (g.life) {
    case LifePhase::Pose:
        if(g.sceneBeat==SceneBeat::CompletionWalk)return 0;
        return completionScene(g) && g.sceneBeat!=SceneBeat::CompletionPose?7:17;
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
    case LifePhase::Wake: return g.sceneBeat==SceneBeat::WakeDelay || g.sceneBeat==SceneBeat::BedFadeIn || g.sceneBeat==SceneBeat::AlarmEnter || g.sceneBeat==SceneBeat::AlarmLeave?13:14;
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
        (g.life==LifePhase::Wake && (g.sceneBeat==SceneBeat::BedFadeIn || g.sceneBeat==SceneBeat::WakeDelay || g.sceneBeat==SceneBeat::AlarmEnter || g.sceneBeat==SceneBeat::AlarmLeave));
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
    if(g.life==LifePhase::Dialogue) return companionDialogue(g) && g.dialogueStage==DialogueStage::Npc?22:7;
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
        const auto criticalRoll = rng.bounded(100); // Keep the existing RNG draw cadence.
        result.critical = player && criticalRoll < 20U;
        result.damage = result.critical ? 2 : 1;
    }
    return result;
}
void Explorer::assignRelationships() {
    state_.gameplay.hostile.clear();
    if(state_.expedition.enabled() && state_.expedition.floors.at(state_.floorIndex).fixedFloorId) {
        const auto& floor=state_.expedition.floors[state_.floorIndex];
        const auto& spawns=state_.expedition.revisit && floor.revisitFixedFloorId?floor.revisitSpawns:floor.fixedSpawns;
        for(const auto& e:state_.population.entities)if(e.kind==EntityKind::Friend) {
            const auto& source=spawns.at(e.instanceId-1);
            if(source.spawnType!=7 && source.spawnType!=10)state_.gameplay.hostile.push_back(e.instanceId);
        }
        return;
    }
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
void Explorer::startGuildMorning(std::uint64_t now) {
    if(state_.expedition.enabled() && !state_.completed && state_.phase==Phase::Explore)state_.expedition.outcome=ExpeditionOutcome::Interrupted;
    state_.variety.notices.clear();
    enableGameplay(now);
    cancelVariety(state_);
    if(state_.variety.schedule.holidayCommitted && !state_.variety.schedule.mealCommitted) {
        if(state_.gameplay.life!=LifePhase::ReturnHome)(void)beginReturn(now,false);
        return;
    }
    auto& g=state_.gameplay;
    invalidate();state_.phase=Phase::Home;
    g.onceKeys.insert("return:"+std::to_string(state_.runId));
    if(!state_.variety.schedule.holidayCommitted)g.onceKeys.insert("guild-start:"+std::to_string(state_.runId));
    else g.onceKeys.erase("guild-start:"+std::to_string(state_.runId));
    g.life=LifePhase::Wake;g.sceneBeat=SceneBeat::BedFadeIn;g.homeScene=true;g.deathStage=DeathStage::FadeOut;
    g.phaseStartedMs=now;g.deadlineMs=now+kFadeMs;
    g.defeated=false;g.playerHidden=false;g.fadeInOnly=false;
    g.hearts=kMaxHp;g.companion.hearts=kMaxHp;g.companion.hidden=false;
    g.movement={};g.companion.movement={};g.walkStartedMs=g.companion.walkStartedMs=0;
    g.partyActions.clear();g.partyPlaybacks.clear();g.partyPhase=PartyPhase::Idle;
    g.companionIntent=CompanionIntent::Follow;g.companionTarget=0;
    g.enemyTargetsCompanion=false;g.enemyInstance=0;g.enemyHits=0;
    for(auto& e:state_.population.entities)e.movement={};
}
GameplayUpdate Explorer::beginReturn(std::uint64_t now, bool defeated) {
    cancelVariety(state_);
    invalidate();
    state_.phase = Phase::Home;
    auto& g = state_.gameplay;
    if(companionDialogue(g))g.enemyInstance=0;
    if(state_.expedition.enabled() && state_.expedition.outcome!=ExpeditionOutcome::Success)
        state_.expedition.outcome=defeated?ExpeditionOutcome::Defeat:ExpeditionOutcome::VoluntaryReturn;
    g.life = LifePhase::ReturnHome; g.defeated = defeated; g.homeScene = false;g.sceneBeat=SceneBeat::None;g.fadeInOnly=false;g.walkStartedMs=0;
    g.companion.walkStartedMs=0;
    g.partyActions.clear();g.partyPlaybacks.clear();g.partyPhase=PartyPhase::Idle;
    g.companionIntent=CompanionIntent::Follow;g.companionTarget=0;
    g.phaseStartedMs = now; g.deadlineMs = now + kFadeMs;
    const auto once = "return:" + std::to_string(state_.runId);
    if (!g.onceKeys.insert(once).second) throw std::logic_error("duplicate return transaction");
    if(!defeated && state_.completed){if(state_.variety.completedExplorations==UINT32_MAX)throw std::runtime_error("completed exploration counter exhausted");++state_.variety.completedExplorations;}
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
    if(companionDialogue(g) && g.dialogueStage==DialogueStage::Npc) result.partner=finished;
    else if((g.life==LifePhase::Dialogue && g.dialogueStage==DialogueStage::Npc) || (g.life==LifePhase::Combat && (g.combat==CombatPhase::EnemyAttack || g.combat==CombatPhase::EnemyHurt || g.combat==CombatPhase::Faint))) result.enemy=finished;
    else {result.player=finished;result.partner=finished;}
    return advanceGameplay(now,result);
}
GameplayUpdate Explorer::advanceGameplay(std::uint64_t now, ActorCompletion completed) {
    if (!state_.gameplay.enabled) throw std::logic_error("gameplay is disabled");
    auto& g = state_.gameplay;
    if(varietyBusy(state_.variety)) {
        // Never hold pending damage, survival recovery or a mandatory scene transition.
        if((state_.variety.dialogue.active && now>=state_.variety.dialogue.deadlineMs) ||
           (state_.variety.event.progress>=EventProgress::Approach && state_.variety.event.progress<=EventProgress::Dismiss && now>=state_.variety.event.deadlineMs) ||
           g.life!=LifePhase::Explore || g.sceneBeat!=SceneBeat::None || g.hearts<=kLowHp ||
           (g.companion.enabled && g.companion.hearts<=kLowHp) || !g.partyActions.empty() || !g.partyPlaybacks.empty()) {
            cancelVariety(state_);invalidate();
        } else return {};
    }
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
    if(expeditionScene(state_)) {
        if(now<g.deadlineMs)return {};
        auto& e=state_.expedition;
        if(g.sceneBeat==SceneBeat::ExpeditionFadeIn) {
            g.sceneBeat=SceneBeat::ExpeditionRest;g.phaseStartedMs=now;g.deadlineMs=now+4000;
            return {GameplayEvent::Explore,{}};
        }
        if(g.sceneBeat==SceneBeat::ExpeditionRest) {
            g.sceneBeat=SceneBeat::ExpeditionFadeOut;g.phaseStartedMs=now;g.deadlineMs=now+750;
            return {GameplayEvent::Explore,{}};
        }
        const auto& stops=e.entryPending?e.entryStops:e.floors[state_.floorIndex].stops;
        if(e.stopIndex+1<stops.size()) {
            ++e.stopIndex;g.sceneBeat=SceneBeat::ExpeditionFadeIn;g.phaseStartedMs=now;g.deadlineMs=now+750;
            return {GameplayEvent::Explore,{}};
        }
        e.stopIndex=0;
        if(e.entryPending) {
            e.entryPending=false;g.sceneBeat=SceneBeat::ExploreFadeIn;g.phaseStartedMs=now;g.deadlineMs=now+3000;
            return {GameplayEvent::Explore,{}};
        }
        // Commit the pending stairs only after every ground in this link finished.
        g.sceneBeat=SceneBeat::None;const auto event=advance();
        if(event.kind!=EventKind::FloorChanged)throw std::logic_error("expedition link did not advance");
        g.sceneBeat=state_.expedition.enabled() && state_.floors[state_.floorIndex].localFloor==0?SceneBeat::ExploreFadeIn:SceneBeat::FloorFadeIn;g.phaseStartedMs=now;g.deadlineMs=now+(g.sceneBeat==SceneBeat::ExploreFadeIn?3000:kFloorFadeMs);
        return {GameplayEvent::FloorChanged,event.onceKey};
    }
    if(state_.expedition.enabled() && state_.expedition.entryPending && g.life==LifePhase::Explore &&
       (g.sceneBeat==SceneBeat::ExploreFadeIn || g.sceneBeat==SceneBeat::None)) {
        invalidate();g.movement={};g.companion.movement={};g.walkStartedMs=g.companion.walkStartedMs=0;
        g.partyActions.clear();g.partyPlaybacks.clear();g.partyPhase=PartyPhase::Idle;
        g.sceneBeat=SceneBeat::ExpeditionFadeIn;g.phaseStartedMs=now;g.deadlineMs=now+750;
        return {GameplayEvent::Explore,{}};
    }
    if(g.life==LifePhase::Death) {
        if(now<g.deadlineMs) return {};
        if(g.deathStage==DeathStage::PartnerTalk) {
            g.deathStage=DeathStage::FadeOut;g.phaseStartedMs=now;g.deadlineMs=now+750;
            return {GameplayEvent::CombatDefeat,{}};
        }
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
        if(state_.expedition.enabled() && !state_.expedition.floors[state_.floorIndex].stops.empty()) {
            state_.expedition.stopIndex=0;g.sceneBeat=SceneBeat::ExpeditionFadeIn;g.phaseStartedMs=now;g.deadlineMs=now+750;
            return {GameplayEvent::Explore,{}};
        }
        g.sceneBeat=SceneBeat::None;
        const auto event=advance(); // The existing stairs journal and generation run once, at black.
        g.phaseStartedMs=now;g.walkStartedMs=0;
        if(event.kind==EventKind::Pose) {
            g.life=LifePhase::Pose;g.sceneBeat=SceneBeat::CompletionFadeIn;g.deadlineMs=now+kFadeMs;
            g.walkStartedMs=g.companion.walkStartedMs=0;g.playerDirection=g.companion.direction=4;
            return {GameplayEvent::Pose,event.onceKey};
        }
        if(event.kind!=EventKind::FloorChanged) throw std::logic_error("stairs transition did not advance");
        g.sceneBeat=state_.expedition.enabled() && state_.floors[state_.floorIndex].localFloor==0?SceneBeat::ExploreFadeIn:SceneBeat::FloorFadeIn;g.deadlineMs=now+(g.sceneBeat==SceneBeat::ExploreFadeIn?3000:kFloorFadeMs);
        return {GameplayEvent::FloorChanged,event.onceKey};
    }
    if(g.life==LifePhase::Explore && g.sceneBeat==SceneBeat::ExplorationBerryText) {
        if(now<g.deadlineMs) return {};
        g.sceneBeat=SceneBeat::None;g.phaseStartedMs=g.deadlineMs=now;
        return {GameplayEvent::Explore,{}};
    }
    if(beginCompanionDialogue(now)) return {GameplayEvent::DialoguePlayer,{}};
    if(g.independentParty && (g.life==LifePhase::Explore || g.life==LifePhase::Combat) && g.sceneBeat==SceneBeat::None) return advanceParty(now,completed);
    if (g.life == LifePhase::Pose) {
        auto beat=[&](SceneBeat next,std::uint64_t duration,unsigned direction) {
            g.sceneBeat=next;g.phaseStartedMs=now;g.deadlineMs=now+duration;
            g.playerDirection=g.companion.direction=direction;
            g.walkStartedMs=g.companion.walkStartedMs=next==SceneBeat::CompletionWalk?now:0;
            return GameplayUpdate{GameplayEvent::Pose,{}};
        };
        if(!completionScene(g))return beat(SceneBeat::CompletionFadeIn,kFadeMs,4); // MDES1–9 clear pose.
        if(now<g.deadlineMs && g.sceneBeat!=SceneBeat::CompletionPose)return {};
        switch(g.sceneBeat) {
        case SceneBeat::CompletionFadeIn:return beat(SceneBeat::CompletionWalk,kCompletionWalkMs,4);
        case SceneBeat::CompletionWalk:return beat(SceneBeat::CompletionPose,kFiniteTimeoutMs,0);
        case SceneBeat::CompletionPose:
            if(!finiteDone || (g.companion.enabled && !completed.partner && now<g.deadlineMs))return {};
            return beat(SceneBeat::CompletionText,2400,0);
        case SceneBeat::CompletionText:return beat(SceneBeat::CompletionFadeOut,kFadeMs,0);
        case SceneBeat::CompletionFadeOut: {
            (void)advance(true);
            auto result=beginReturn(now,false);g.homeScene=true;g.fadeInOnly=true;g.deadlineMs=now+kFadeMs;
            return result;
        }
        default:throw std::logic_error("invalid completion stage");
        }
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
        g.onceKeys.insert(update.onceKey);
        if(!state_.variety.schedule.mealCommitted) {++g.meals;state_.variety.schedule.mealCommitted=true;}
        return update;
    }
    if (g.life == LifePhase::Emotion) {
        if (!finiteDone) return {};
        g.sceneBeat=SceneBeat::DiningPan;
        auto update = life(LifePhase::Eat, GameplayEvent::Eat, kMealMs);
        update.onceKey = "meal:" + std::to_string(state_.runId);
        g.onceKeys.insert(update.onceKey);
        if(!state_.variety.schedule.mealCommitted) {++g.meals;state_.variety.schedule.mealCommitted=true;}
        return update;
    }
    if(g.life==LifePhase::Dialogue) {
        if(now<g.deadlineMs) return {};
        if(g.dialogueStage==DialogueStage::Player) {
            if(!completed.player) return {};
            g.dialogueStage=DialogueStage::Npc;g.phaseStartedMs=now;g.deadlineMs=now+(companionDialogue(g)?kDialogueMs:1200);
            return {GameplayEvent::DialogueNpc,{}};
        }
        if(companionDialogue(g)) {
            if(!completed.partner) return {};
            g.enemyInstance=0;
            return life(LifePhase::Explore,GameplayEvent::Explore,0);
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
        g.deadlineMs=std::min(g.deadlineMs,g.phaseStartedMs+kMealMs);
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
            g.sceneBeat=SceneBeat::BedFace;return life(LifePhase::Sleep,GameplayEvent::Sleep,kBedGreetingMs);
        case SceneBeat::BedFace:
            if(now<g.deadlineMs || !(completed.player&&completed.partner)) return {};
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
        if(g.sceneBeat==SceneBeat::BedFadeIn) {
            if(now<g.deadlineMs) return {};
            g.sceneBeat=SceneBeat::AlarmEnter;return life(LifePhase::Wake,GameplayEvent::Wake,800);
        }
        if(g.sceneBeat==SceneBeat::AlarmEnter) {
            if(now<g.deadlineMs)return {};
            g.sceneBeat=SceneBeat::WakeDelay;return life(LifePhase::Wake,GameplayEvent::Wake,kWakeDelayMs);
        }
        if(g.sceneBeat==SceneBeat::WakeDelay) {
            if(now<g.deadlineMs)return {};
            g.sceneBeat=SceneBeat::AlarmLeave;return life(LifePhase::Wake,GameplayEvent::Wake,800);
        }
        if(g.sceneBeat==SceneBeat::AlarmLeave) {
            if(now<g.deadlineMs) return {};
            g.sceneBeat=SceneBeat::Waking;return life(LifePhase::Wake,GameplayEvent::Wake,kFiniteTimeoutMs);
        }
        if(!(completed.player&&completed.partner)) return {};
        g.sceneBeat=SceneBeat::MorningFace;
        return life(LifePhase::Ready,GameplayEvent::Ready,kDialogueMs);
    }
    if(g.life==LifePhase::Ready) {
        if(g.sceneBeat==SceneBeat::MorningFace && (now<g.deadlineMs || !(completed.player&&completed.partner))) return {};
        if(g.sceneBeat!=SceneBeat::RoomExit && g.sceneBeat!=SceneBeat::MorningFadeOut) { // Resume old morning saves through the exit.
            g.sceneBeat=SceneBeat::RoomExit;g.playerDirection=0;
            return life(LifePhase::Ready,GameplayEvent::Ready,kRoomExitMs);
        }
        if(g.sceneBeat==SceneBeat::RoomExit) {
            if(now<g.deadlineMs || !completed.player || (g.companion.enabled && !completed.partner)) return {};
            g.sceneBeat=SceneBeat::MorningFadeOut;
            return life(LifePhase::Ready,GameplayEvent::Ready,kFadeMs);
        }
        if(now<g.deadlineMs) return {};
        if(state_.runId==UINT64_MAX || g.day==UINT32_MAX) throw std::runtime_error("day counter exhausted");
        const auto previous=g;const auto inventory=state_.inventory;
        const auto nextDay=previous.day+(previous.onceKeys.count("guild-start:"+std::to_string(state_.runId))?0:1);
        const bool sameScheduledDay=nextDay==state_.variety.schedule.day &&
            state_.variety.schedule.destination==DayDestination::Exploration;
        const bool changePackage=!nextRunFloors_.empty() && (!sameScheduledDay || nextRunAtLaunch_);
        Explorer next(state_.runId+1,state_.rng.state,
            changePackage?nextRunAssetVersion_:state_.assetVersion,
            changePackage?nextRunFloors_:state_.floors,state_.spawnPolicy,recentLayouts(),
            changePackage?nextRunExpedition_:freshExpedition(state_.expedition));
        // Keep a deferred package upgrade available until the scheduled day ends.
        if(!changePackage && !nextRunFloors_.empty())next.setNextRunPackage(nextRunAssetVersion_,nextRunFloors_,nextRunExpedition_);
        next.state_.inventory=inventory;
        if(state_.screenExploration)next.enableScreenExploration(state_.viewportSize);
        for(const auto& entry:inventory) next.state_.onceKeys.insert(entry.onceKey);
        next.state_.gameplay.enabled=true;next.state_.gameplay.day=nextDay;
        cancelVariety(state_);
        next.state_.variety=state_.variety;
        if(next.state_.variety.schedule.day!=next.state_.gameplay.day) {
            next.state_.variety.schedule={};next.state_.variety.schedule.day=next.state_.gameplay.day;
        }
        // P0 enables only the existing exploration destination. Holiday selection is P7.
        auto& schedule=next.state_.variety.schedule;
        if(schedule.destination==DayDestination::Unassigned) {
            const auto& variety=next.state_.variety;
            if(variety.holidayPolicy && previous.companion.enabled && variety.completedExplorations-variety.lastHolidayAt>=3) {
                schedule.destination=variety.lastHoliday==DayDestination::Town?DayDestination::HotSpring:DayDestination::Town;schedule.dungeon=0;
            } else {schedule.destination=DayDestination::Exploration;schedule.dungeon=next.state_.floors.front().dungeonId;}
        }
        if(schedule.destination==DayDestination::Exploration)schedule.dungeon=next.state_.floors.front().dungeonId;
        next.state_.gameplay.meals=previous.meals;next.state_.gameplay.oran=previous.oran;
        next.state_.gameplay.phaseStartedMs=now;next.state_.gameplay.deadlineMs=now+3000;
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
            if (g.oran && g.hearts>0 && g.hearts+2<=kMaxHp) {
                --g.oran; g.hearts = kMaxHp;
                auto update = combat(CombatPhase::BerryText, GameplayEvent::UseOranBerry, 2000); update.onceKey = once;
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
            if(partner.oran && partner.hearts>0 && partner.hearts+2<=kMaxHp) {
                --partner.oran;partner.hearts=kMaxHp;
                auto update=combat(CombatPhase::CompanionBerryText,GameplayEvent::UseOranBerry,2000);update.onceKey=once;update.actorId=kCompanionActorId;return update;
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
            queueGameplayNotice(state_,NoticeKind::Victory,0);
            g.onceKeys.insert("victory:" + key("combat", g.enemyInstance));
            return combat(CombatPhase::Victory, GameplayEvent::CombatVictory, 350);
        case CombatPhase::Victory:
            if (now < g.deadlineMs) return {};
            return life(LifePhase::Explore, GameplayEvent::Explore, 350);
        case CombatPhase::PlayerFaint:
            if(!completed.player) return {};
            invalidate();state_.phase=Phase::Home;g.playerHidden=true;g.deathStage=DeathStage::PartnerTalk;
            return life(LifePhase::Death,GameplayEvent::CombatDefeat,2400);
        case CombatPhase::CompanionFaint:
            if(!completed.partner) return {};
            invalidate();state_.phase=Phase::Home;g.companion.hidden=true;g.deathStage=DeathStage::PartnerTalk;
            return life(LifePhase::Death,GameplayEvent::CombatDefeat,2400);
        case CombatPhase::Defeat:
            if (now < g.deadlineMs) return {};
            return combat(CombatPhase::PlayerFaint, GameplayEvent::PlayerFaint);
        }
    }
    if (now < g.deadlineMs) return {};
    return scheduleDungeon(now);
}
void Explorer::shiftPausedClocks(std::uint64_t delta,std::uint64_t pausedAt) {
    auto& variety=state_.variety;
    auto shiftVariety=[&](std::uint64_t& start,std::uint64_t& deadline) {
        if(deadline>UINT64_MAX-delta || start>UINT64_MAX-delta)throw std::runtime_error("variety pause clock overflow");
        start+=delta;deadline+=delta;
    };
    for(auto& notice:variety.notices)if(notice.startedMs)shiftVariety(notice.startedMs,notice.deadlineMs);
    if(variety.dialogue.active)shiftVariety(variety.dialogue.startedMs,variety.dialogue.deadlineMs);
    if(variety.holiday.active)shiftVariety(variety.holiday.startedMs,variety.holiday.deadlineMs);
    if(variety.event.progress>=EventProgress::Approach && variety.event.progress<=EventProgress::Dismiss)
        shiftVariety(variety.event.startedMs,variety.event.deadlineMs);
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
    return enemyTargetActor(enemy)==kCompanionActorId?state_.gameplay.companion.tile:state_.position;
}
std::uint32_t Explorer::enemyTargetActor(const Entity& enemy) const {
    const auto& s=state_;const auto& g=s.gameplay;
    std::vector<std::pair<unsigned,int>> candidates;
    if(g.hearts)candidates.push_back({0,s.position});
    if(g.companion.enabled && g.companion.hearts)candidates.push_back({kCompanionActorId,g.companion.tile});
    if(eventBattle(s))for(auto id:s.variety.event.actors)if(partyAlly(s,id))
        for(const auto& a:s.population.entities)if(a.instanceId==id && a.visible && !a.done && a.remainingHits)candidates.push_back({id,a.tile});
    auto occupied=occupancy();occupied[enemy.tile]=0;
    occupied[s.position]=1;if(g.companion.enabled)occupied[g.companion.tile]=1;
    std::tuple<int,int,int,unsigned> best{2,2,kTiles+1,UINT32_MAX};unsigned selected=0;
    for(const auto& [id,tile]:candidates) {
        auto mask=occupied;mask[tile]=0;const auto reach=bfs(s.world,enemy.tile,mask);
        const bool adjacent=canAttack(s.world,enemy.tile,tile);
        const auto rank=std::make_tuple(adjacent?0:1,adjacent && directionBetween(enemy.tile,tile)==enemy.direction?0:1,
            reach.distance[tile]<0?kTiles+1:reach.distance[tile],id);
        if(rank<best){best=rank;selected=id;}
    }
    return selected;
}
int Explorer::patrolTile(const Entity& actor,std::uint64_t now) const {
    const auto& s=state_;const auto& g=s.gameplay;auto blocked=occupancy();blocked[actor.tile]=0;
    blocked[s.position]=1;if(g.companion.enabled)blocked[g.companion.tile]=1;blocked[s.world.stairs]=1;
    for(const auto& a:g.partyActions)if(a.kind==PartyActionKind::Move && !a.finished && now<a.deadlineMs && a.actorId!=actor.instanceId) {
        const auto* movement=a.actorId==0?&g.movement:a.actorId==kCompanionActorId?&g.companion.movement:nullptr;
        if(!movement)for(const auto& e:s.population.entities)if(e.instanceId==a.actorId)movement=&e.movement;
        if(movement && movement->actionId)blocked[movement->from]=1;
    }
    std::vector<int> choices;
    for(const auto [dx,dy]:kDirections) {
        const int x=actor.tile%kWidth+dx,y=actor.tile/kWidth+dy;
        if(x>=0 && x<kWidth && y>=0 && y<kHeight && canMove(s.world,actor.tile,y*kWidth+x,blocked))choices.push_back(y*kWidth+x);
    }
    if(choices.size()>1 && actor.movement.actionId)choices.erase(std::remove(choices.begin(),choices.end(),actor.movement.from),choices.end());
    if(choices.empty())return -1;
    Rng random{s.mapSeed^(g.actionRound*0x9e3779b97f4a7c15ULL)^(std::uint64_t(actor.instanceId)*0xbf58476d1ce4e5b9ULL)};
    if(random.bounded(4))for(auto tile:choices)if(directionBetween(actor.tile,tile)==actor.direction)return tile;
    return choices[random.bounded(static_cast<unsigned>(choices.size()))];
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
    --p.oran;p.hearts=kMaxHp;p.actionRound=g.actionRound;p.walkStartedMs=0;
    const auto once="companion-heal:"+std::to_string(state_.runId)+":"+std::to_string(state_.floorIndex)+":"+std::to_string(g.actionRound);
    if(!g.onceKeys.insert(once).second) throw std::logic_error("duplicate companion berry use");
    invalidate();g.sceneBeat=SceneBeat::ExplorationBerryText;g.phaseStartedMs=now;g.deadlineMs=now+2000;
    return {GameplayEvent::UseOranBerry,once,0,false,kCompanionActorId};
}
bool Explorer::beginCompanionDialogue(std::uint64_t now) {
    auto& s=state_;auto& g=s.gameplay;auto& p=g.companion;
    // Only pause at a settled, safe movement boundary. Keep combat RNG and turn rights intact.
    if(g.life!=LifePhase::Explore || g.homeScene || g.sceneBeat!=SceneBeat::None || now<g.deadlineMs ||
       !p.enabled || p.hidden || p.hearts<=kLowHp || g.hearts<=kLowHp ||
       s.position==s.world.stairs || !canAttack(s.world,s.position,p.tile) ||
       !g.partyPlaybacks.empty() || std::any_of(g.partyActions.begin(),g.partyActions.end(),[](const auto& a){return !a.finished;})) return false;
    const auto first=48U+unsigned(s.mapSeed%25);
    if(s.floorSteps<first) return false;
    const auto slot=(s.floorSteps-first)/144;
    const auto once="companion-chat:"+std::to_string(s.runId)+":"+std::to_string(s.floorIndex)+":"+std::to_string(slot);
    if(g.onceKeys.count(once))return false;
    auto moving=[&](const MoveRecord& m){return m.actionId && now>=m.startedMs && now-m.startedMs<m.durationMs;};
    if(moving(g.movement) || moving(p.movement))return false;
    const auto playerReach=bfs(s.world,s.position),partnerReach=bfs(s.world,p.tile);
    for(const auto& e:s.population.entities) {
        if(!e.visible || e.done)continue;
        if(moving(e.movement))return false;
        if(e.kind==EntityKind::Item && e.tile==s.position)return false;
        if(e.kind!=EntityKind::Friend)continue;
        if(std::find(g.hostile.begin(),g.hostile.end(),e.instanceId)==g.hostile.end()) {
            if(canAttack(s.world,s.position,e.tile))return false;
        } else if((s.world.room[e.tile]>=0 && (s.world.room[e.tile]==s.world.room[s.position] || s.world.room[e.tile]==s.world.room[p.tile])) ||
            (playerReach.distance[e.tile]>=0 && playerReach.distance[e.tile]<=4) ||
            (partnerReach.distance[e.tile]>=0 && partnerReach.distance[e.tile]<=4)) return false;
    }
    g.onceKeys.insert(once);g.enemyInstance=kCompanionActorId;g.dialogueStage=DialogueStage::Player;
    g.playerDirection=directionBetween(s.position,p.tile);p.direction=directionBetween(p.tile,s.position);
    g.walkStartedMs=p.walkStartedMs=0;g.movement={};p.movement={};
    g.life=LifePhase::Dialogue;g.phaseStartedMs=now;g.deadlineMs=now+kDialogueMs;
    return true;
}
GameplayUpdate Explorer::scheduleDungeon(std::uint64_t now) {
    auto& g=state_.gameplay;
    retreatProgress_=false;
    if(!fixedBattlePending(state_) && state_.position==state_.world.stairs && (lowHealth() || roomFullyShown(state_.world.room[state_.position]))) {
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
            if(e.kind!=EntityKind::Friend || !e.visible || (e.done && hostile(e)) || !e.remainingHits || e.actionRound==g.actionRound) continue;
            if(!hostile(e)) {
                e.actionRound=g.actionRound;
                const int next=patrolTile(e,now);
                if(next>=0){const int old=e.tile;e.tile=next;e.direction=directionBetween(old,next);e.movement={g.actionRound,now,400,e.direction,old,next};++state_.occupancyVersion;invalidate(false,true);}
                continue;
            }
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
            const int next=patrolTile(e,now);
            if(next>=0){const int old=e.tile;e.tile=next;e.direction=directionBetween(old,next);e.movement={g.actionRound,now,400,e.direction,old,next};++state_.occupancyVersion;invalidate(false,true);}
        }
        if(g.actionRound==UINT64_MAX) throw std::runtime_error("action round exhausted");
        ++g.actionRound;
    }
    if(g.oran && g.hearts>0 && g.hearts+2<=kMaxHp && g.playerActionRound!=g.actionRound) {
        --g.oran;g.hearts=kMaxHp;g.playerActionRound=g.actionRound;g.walkStartedMs=0;
        const auto once="heal:"+std::to_string(state_.runId)+":"+std::to_string(state_.floorIndex)+":"+std::to_string(g.actionRound);
        if(!g.onceKeys.insert(once).second) throw std::logic_error("duplicate exploration berry use");
        invalidate();g.sceneBeat=SceneBeat::ExplorationBerryText;g.phaseStartedMs=now;g.deadlineMs=now+2000;
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
        g.life=LifePhase::Pose;g.sceneBeat=SceneBeat::CompletionFadeIn;g.deadlineMs=now+750;
        g.walkStartedMs=g.companion.walkStartedMs=0;g.playerDirection=g.companion.direction=4;
        return {GameplayEvent::Pose,update.onceKey};
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
            else if(g.oran<kMaxStock) ++g.oran;
        }
        queueGameplayNotice(state_,NoticeKind::Pickup,update.originalId);
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
