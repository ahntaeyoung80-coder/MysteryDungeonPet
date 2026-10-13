#include "core/exploration.hpp"
#include <algorithm>
#include <cstdlib>
#include <limits>
#include <stdexcept>

namespace pet {
namespace {
constexpr std::uint64_t kFiniteTimeoutMs = 5000, kFadeMs = 750, kSleepMs = 10000, kMealMs = 8000;
}
const char* lifeName(LifePhase phase) {
    static const char* names[]{"Explore", "Pose", "ReturnHome", "Emotion", "Eat", "Sleep", "Wake", "Ready", "Combat", "Death", "Dialogue"};
    return names[static_cast<unsigned>(phase)];
}
const char* combatName(CombatPhase phase) {
    static const char* names[]{"PlayerTurn", "PlayerAttack", "EnemyHurt", "EnemyTurn", "EnemyAttack", "PlayerHurt", "BerryText", "Faint", "CombatVictory", "CombatDefeat", "PlayerFaint"};
    return names[static_cast<unsigned>(phase)];
}
bool dungeonScene(const GameplayState& g) { return !g.homeScene; }
bool bedroomScene(const GameplayState& g) {
    return g.sceneBeat==SceneBeat::BedFadeIn || (g.sceneBeat>=SceneBeat::BedFace && g.sceneBeat<=SceneBeat::MorningFadeOut) ||
        (g.sceneBeat==SceneBeat::None && (g.life==LifePhase::Sleep || g.life==LifePhase::Wake || g.life==LifePhase::Ready));
}
bool nightScene(const GameplayState& g) { return g.sceneBeat>=SceneBeat::NightFadeIn && g.sceneBeat<=SceneBeat::MorningFadeOut; }
std::uint32_t gameplayClip(const GameplayState& g) {
    switch (g.life) {
    case LifePhase::Pose: return 17;
    case LifePhase::Emotion: return 6; // New defeat expression cue using the supported Hurt body clip.
    case LifePhase::Eat: return 15;
    case LifePhase::Sleep:
        if(g.sceneBeat==SceneBeat::BedFadeIn) return 7;
        if(g.sceneBeat==SceneBeat::DiningExit) return 15;
        if(g.sceneBeat==SceneBeat::BedFace) return 22;
        if(g.sceneBeat==SceneBeat::BedLay || g.sceneBeat==SceneBeat::BedFadeOut) return 27;
        return 13;
    case LifePhase::Ready: return g.sceneBeat==SceneBeat::MorningFace || g.sceneBeat==SceneBeat::MorningFadeOut ? 22:7;
    case LifePhase::Dialogue:return g.dialogueStage==DialogueStage::Player?22:7;
    case LifePhase::Wake: return 14;
    case LifePhase::Combat:
        if (g.combat == CombatPhase::PlayerAttack) return 1;
        if (g.combat == CombatPhase::PlayerHurt) return 6;
        if (g.combat == CombatPhase::PlayerFaint) return 33;
        return 7;
    default: return 7;
    }
}
bool gameplayLoop(const GameplayState& g) {
    const auto clip = gameplayClip(g);
    return clip == 0 || clip == 7 || clip == 5 || clip == 13 || g.life == LifePhase::Eat || g.sceneBeat==SceneBeat::DiningExit;
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
    g.life = LifePhase::ReturnHome; g.defeated = defeated; g.homeScene = false;g.sceneBeat=SceneBeat::None;g.fadeInOnly=false;
    g.phaseStartedMs = now; g.deadlineMs = now + kFadeMs;
    const auto once = "return:" + std::to_string(state_.runId);
    if (!g.onceKeys.insert(once).second) throw std::logic_error("duplicate return transaction");
    return {GameplayEvent::ReturnHome, once};
}
void Explorer::completeEncounter() {
    auto& g = state_.gameplay;
    auto it = std::find_if(state_.population.entities.begin(), state_.population.entities.end(), [&](const auto& e) { return e.instanceId == g.enemyInstance; });
    if (it == state_.population.entities.end()) throw std::logic_error("combat entity missing");
    // Preserve the B-1 encounter journal contract; the separate combat journal records victory.
    state_.onceKeys.insert(key("greet", it->instanceId));
    it->done = true;
    invalidate();
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
    const bool finished=enemyOwner?completed.enemy:completed.player;
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
        auto result=beginReturn(now,true);g.homeScene=true;g.fadeInOnly=true;g.playerHidden=false;g.deadlineMs=now+750;
        return result;
    }
    if (g.life == LifePhase::Pose) {
        if (!finiteDone) return {};
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
        if(g.sceneBeat==SceneBeat::BedFace) {
            if(!(completed.player&&completed.partner)) return {};
            g.sceneBeat=SceneBeat::BedLay;return life(LifePhase::Sleep,GameplayEvent::Sleep,500);
        }
        if(now<g.deadlineMs) return {};
        switch(g.sceneBeat) {
        case SceneBeat::DiningExit:g.sceneBeat=SceneBeat::BedFadeIn;return life(LifePhase::Sleep,GameplayEvent::HomeScene,750);
        case SceneBeat::BedFadeIn:g.sceneBeat=SceneBeat::BedFace;return life(LifePhase::Sleep,GameplayEvent::Sleep,5000);
        case SceneBeat::BedLay:g.sceneBeat=SceneBeat::BedFadeOut;return life(LifePhase::Sleep,GameplayEvent::Sleep,750);
        case SceneBeat::BedFadeOut:g.sceneBeat=SceneBeat::NightFadeIn;return life(LifePhase::Sleep,GameplayEvent::HomeScene,750);
        case SceneBeat::NightFadeIn:g.sceneBeat=SceneBeat::Sleeping;return life(LifePhase::Sleep,GameplayEvent::Sleep,kSleepMs);
        default:g.sceneBeat=SceneBeat::Waking;return life(LifePhase::Wake,GameplayEvent::Wake,5000);
        }
    }
    if(g.life==LifePhase::Wake) {
        if(!(completed.player&&completed.partner)) return {};
        g.hearts=kMaxHp;g.sceneBeat=SceneBeat::MorningFace;return life(LifePhase::Ready,GameplayEvent::Ready,5000);
    }
    if(g.life==LifePhase::Ready) {
        if(g.sceneBeat!=SceneBeat::MorningFadeOut) {
            if(!(completed.player&&completed.partner)) return {};
            g.sceneBeat=SceneBeat::MorningFadeOut;return life(LifePhase::Ready,GameplayEvent::Ready,750);
        }
        if(now<g.deadlineMs) return {};
        if(state_.runId==UINT64_MAX || g.day==UINT32_MAX) throw std::runtime_error("day counter exhausted");
        const auto previous=g;
        Explorer next(state_.runId+1,state_.rng.state,state_.assetVersion,state_.floors,state_.spawnPolicy);
        next.state_.gameplay.enabled=true;next.state_.gameplay.day=previous.day+1;next.state_.gameplay.meals=previous.meals;next.state_.gameplay.oran=previous.oran;
        next.state_.gameplay.sceneBeat=SceneBeat::ExploreFadeIn;next.state_.gameplay.phaseStartedMs=now;next.state_.gameplay.deadlineMs=now+750;
        next.assignRelationships();*this=std::move(next);return {GameplayEvent::Explore,{}};
    }
    if (g.life == LifePhase::Combat) {
        switch (g.combat) {
        case CombatPhase::PlayerTurn: {
            if (now < g.deadlineMs) return {};
            if(g.playerActionRound==g.actionRound) return afterCombatAction(now);
            g.playerActionRound=g.actionRound;
            const auto once = "turn:" + std::to_string(state_.runId) + ":" + std::to_string(state_.floorIndex) + ":" + std::to_string(g.enemyInstance) + ":" + std::to_string(++g.turn);
            if (!g.onceKeys.insert(once).second) throw std::logic_error("duplicate combat turn");
            if (g.oran && g.hearts < kMaxHp) {
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
        case CombatPhase::EnemyHurt:
            if (!finiteDone) return {};
            return !g.enemyHits ? combat(CombatPhase::Faint, GameplayEvent::EnemyFaint) : afterCombatAction(now);
        case CombatPhase::BerryText:
            if (now < g.deadlineMs) return {};
            return afterCombatAction(now);
        case CombatPhase::EnemyTurn: {
            if (now < g.deadlineMs) return {};
            auto& enemy=*std::find_if(state_.population.entities.begin(),state_.population.entities.end(),[&](const auto& e){return e.instanceId==g.enemyInstance;});
            if(enemy.actionRound==g.actionRound) return afterCombatAction(now);
            enemy.actionRound=g.actionRound;
            const auto once = "enemy:" + std::to_string(state_.runId) + ":" + std::to_string(state_.floorIndex) + ":" + std::to_string(g.enemyInstance) + ":" + std::to_string(g.actionRound);
            if (!g.onceKeys.insert(once).second) throw std::logic_error("duplicate enemy turn");
            const auto roll = rollBasicAttack(state_.rng, false);
            g.hit = roll.hit; g.critical = roll.critical; g.damage = roll.damage;
            g.hearts -= std::min(g.hearts, roll.damage);
            auto update = combat(CombatPhase::EnemyAttack, GameplayEvent::EnemyAttack); update.onceKey = once; update.damage = roll.damage; update.critical = roll.critical;
            return update;
        }
        case CombatPhase::EnemyAttack:
            if (!finiteDone) return {};
            return g.hit ? combat(CombatPhase::PlayerHurt, GameplayEvent::Hurt) : afterCombatAction(now);
        case CombatPhase::PlayerHurt:
            if (!finiteDone) return {};
            return !g.hearts ? combat(CombatPhase::PlayerFaint, GameplayEvent::PlayerFaint) : afterCombatAction(now);
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
            state_.phase=Phase::Home;g.playerHidden=true;g.deathStage=DeathStage::FadeOut;
            return life(LifePhase::Death,GameplayEvent::CombatDefeat,750);
        case CombatPhase::Defeat:
            if (now < g.deadlineMs) return {};
            return beginReturn(now, true);
        }
    }
    if (now < g.deadlineMs) return {};
    return scheduleDungeon(now);
}
GameplayUpdate Explorer::beginCombat(Entity& e,std::uint64_t now,CombatPhase first) {
    invalidate();auto& g=state_.gameplay;
    g.life=LifePhase::Combat;g.combat=first;g.enemyInstance=e.instanceId;g.enemyHits=e.remainingHits;
    g.playerDirection=directionBetween(state_.position,e.tile);e.direction=directionBetween(e.tile,state_.position);
    g.phaseStartedMs=now;g.deadlineMs=now+350;
    return {GameplayEvent::HostileEncounter,{}};
}
GameplayUpdate Explorer::afterCombatAction(std::uint64_t now) {
    auto& g=state_.gameplay;
    auto& e=*std::find_if(state_.population.entities.begin(),state_.population.entities.end(),[&](const auto& x){return x.instanceId==g.enemyInstance;});
    g.phaseStartedMs=now;g.deadlineMs=now+350;
    if(g.playerActionRound!=g.actionRound) {g.combat=CombatPhase::PlayerTurn;return {};}
    if(e.actionRound!=g.actionRound) {g.combat=CombatPhase::EnemyTurn;return {};}
    const bool pending=std::any_of(state_.population.entities.begin(),state_.population.entities.end(),[&](const auto& x){
        return !x.done && x.kind==EntityKind::Friend && x.actionRound!=g.actionRound &&
            std::find(g.hostile.begin(),g.hostile.end(),x.instanceId)!=g.hostile.end();});
    if(pending) {g.life=LifePhase::Explore;return {GameplayEvent::Explore,{}};}
    if(g.actionRound==UINT64_MAX) throw std::runtime_error("action round exhausted");
    ++g.actionRound;g.combat=CombatPhase::PlayerTurn;return {};
}
GameplayUpdate Explorer::scheduleDungeon(std::uint64_t now) {
    auto& g=state_.gameplay;
    auto hostile=[&](const Entity& e){return std::find(g.hostile.begin(),g.hostile.end(),e.instanceId)!=g.hostile.end();};
    auto visible=[&](int a,int b){
        const int room=state_.world.room[a];
        return (room>=0 && room==state_.world.room[b]) || (bfs(state_.world,a).distance[b]>=0 && bfs(state_.world,a).distance[b]<=2);
    };
    // Player then each hostile in stable instance order. A movement completion never grants an action.
    if(g.playerActionRound==g.actionRound) {
        for(auto& e:state_.population.entities) {
            if(e.kind!=EntityKind::Friend || e.done || !hostile(e) || e.actionRound==g.actionRound) continue;
            if(visible(e.tile,state_.position)) {e.tracking=Tracking::Chase;e.lastSeen=state_.position;e.chaseTarget=e.lastSeen;}
            if(canAttack(state_.world,e.tile,state_.position)) return beginCombat(e,now,CombatPhase::EnemyTurn);
            e.actionRound=g.actionRound; // a move OR wait, never move followed by attack
            if(e.tracking==Tracking::Chase && e.lastSeen>=0) {
                auto occupied=occupancy();occupied[e.tile]=0;occupied[state_.position]=1;
                const auto reach=bfs(state_.world,e.tile,occupied);++bfsCalls_;
                int goal=-1,best=kTiles+1;
                for(int p=0;p<kTiles;++p) if((p==e.lastSeen || canAttack(state_.world,p,e.lastSeen)) && reach.distance[p]>0 && reach.distance[p]<best) {
                    goal=p;best=reach.distance[p];
                }
                if(goal>=0) {
                    const auto path=reach.pathTo(goal);const int old=e.tile;e.tile=path.front();
                    e.direction=directionBetween(old,e.tile);e.blockedTurns=0;++state_.occupancyVersion;invalidate();
                    g.phaseStartedMs=now;g.deadlineMs=now+400;
                    return {GameplayEvent::ActorMove,{},0,false,e.instanceId};
                }
                if(e.tile==e.lastSeen || (e.blockedTurns=std::min(3U,e.blockedTurns+1))>=3) {e.tracking=Tracking::Idle;e.chaseTarget=-1;}
            }
        }
        if(g.actionRound==UINT64_MAX) throw std::runtime_error("action round exhausted");
        ++g.actionRound;
    }
    for(auto& e:state_.population.entities) {
        if(e.done||e.kind!=EntityKind::Friend||!canAttack(state_.world,state_.position,e.tile)||state_.blocked[e.tile]) continue;
        if(hostile(e)) return beginCombat(e,now,CombatPhase::PlayerTurn);
        invalidate();g.enemyInstance=e.instanceId;g.dialogueStage=DialogueStage::Player;
        g.playerDirection=directionBetween(state_.position,e.tile);e.direction=directionBetween(e.tile,state_.position);
        g.playerActionRound=g.actionRound;g.life=LifePhase::Dialogue;g.phaseStartedMs=now;g.deadlineMs=now+1200;
        return {GameplayEvent::DialoguePlayer,{}};
    }
    const int old=state_.position;
    const auto update=advance();
    g.phaseStartedMs=now;g.deadlineMs=now+(update.kind==EventKind::Move?400:0);
    if(update.kind==EventKind::Move) {g.playerActionRound=g.actionRound;g.playerDirection=directionBetween(old,state_.position);}
    switch(update.kind) {
    case EventKind::Pose:
        g.life=LifePhase::Pose;g.deadlineMs=now+5000;return {GameplayEvent::Pose,update.onceKey};
    case EventKind::ReturnedHome:return beginReturn(now,false);
    case EventKind::Pickup:
        if(update.originalId==ITEM_ORAN_BERRY && !g.oran) ++g.oran;
        return {GameplayEvent::Pickup,update.onceKey};
    case EventKind::Move:return {GameplayEvent::Move,{}};
    case EventKind::Greeting:return {GameplayEvent::Greeting,update.onceKey};
    case EventKind::FloorChanged:return {GameplayEvent::FloorChanged,update.onceKey};
    default:g.playerActionRound=g.actionRound;g.deadlineMs=now+400;return {GameplayEvent::Wait,{}};
    }
}

} // namespace pet
