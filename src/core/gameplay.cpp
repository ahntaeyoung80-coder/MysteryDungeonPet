#include "core/exploration.hpp"
#include <algorithm>
#include <cstdlib>
#include <limits>
#include <stdexcept>

namespace pet {
namespace {
constexpr std::uint64_t kFiniteTimeoutMs = 5000, kFadeMs = 750, kSleepMs = 10000;
int distance(int a, int b) { return std::abs(a % kWidth - b % kWidth) + std::abs(a / kWidth - b / kWidth); }
}
const char* lifeName(LifePhase phase) {
    static const char* names[]{"Explore", "Pose", "ReturnHome", "Emotion", "Eat", "Sleep", "Wake", "Ready", "Combat"};
    return names[static_cast<unsigned>(phase)];
}
const char* combatName(CombatPhase phase) {
    static const char* names[]{"PlayerTurn", "PlayerAttack", "EnemyHurt", "EnemyTurn", "EnemyAttack", "PlayerHurt", "BerryText", "Faint", "CombatVictory", "CombatDefeat"};
    return names[static_cast<unsigned>(phase)];
}
bool dungeonScene(const GameplayState& g) { return !g.homeScene; }
std::uint32_t gameplayClip(const GameplayState& g) {
    switch (g.life) {
    case LifePhase::Pose: return 17;
    case LifePhase::Emotion: return 6; // New defeat expression cue using the supported Hurt body clip.
    case LifePhase::Eat: return 15;
    case LifePhase::Sleep: return 5;
    case LifePhase::Wake: return 14;
    case LifePhase::Combat:
        if (g.combat == CombatPhase::PlayerAttack) return 1;
        if (g.combat == CombatPhase::PlayerHurt) return 6;
        return 7;
    default: return 7;
    }
}
bool gameplayLoop(const GameplayState& g) {
    const auto clip = gameplayClip(g);
    return clip == 0 || clip == 7 || clip == 5 || clip == 13;
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
    g.life = LifePhase::ReturnHome; g.defeated = defeated; g.homeScene = false;
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
GameplayUpdate Explorer::advanceGameplay(std::uint64_t now, bool finished) {
    if (!state_.gameplay.enabled) throw std::logic_error("gameplay is disabled");
    auto& g = state_.gameplay;
    const bool finiteDone = finished || now >= g.deadlineMs;
    auto life = [&](LifePhase phase, GameplayEvent event, std::uint64_t duration) {
        g.life = phase; g.phaseStartedMs = now; g.deadlineMs = now + duration;
        return GameplayUpdate{event, {}};
    };
    auto combat = [&](CombatPhase phase, GameplayEvent event, std::uint64_t duration = kFiniteTimeoutMs) {
        g.combat = phase; g.phaseStartedMs = now; g.deadlineMs = now + duration;
        return GameplayUpdate{event, {}};
    };
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
        auto update = life(LifePhase::Eat, GameplayEvent::Eat, kFiniteTimeoutMs);
        update.onceKey = "meal:" + std::to_string(state_.runId);
        if (g.onceKeys.insert(update.onceKey).second) ++g.meals;
        return update;
    }
    if (g.life == LifePhase::Emotion) {
        if (!finiteDone) return {};
        auto update = life(LifePhase::Eat, GameplayEvent::Eat, kFiniteTimeoutMs);
        update.onceKey = "meal:" + std::to_string(state_.runId);
        if (g.onceKeys.insert(update.onceKey).second) ++g.meals;
        return update;
    }
    if (g.life == LifePhase::Eat) return finiteDone ? life(LifePhase::Sleep, GameplayEvent::Sleep, kSleepMs) : GameplayUpdate{};
    if (g.life == LifePhase::Sleep) return now >= g.deadlineMs ? life(LifePhase::Wake, GameplayEvent::Wake, kFiniteTimeoutMs) : GameplayUpdate{};
    if (g.life == LifePhase::Wake) {
        if (!finiteDone) return {};
        g.hearts = 3;
        return life(LifePhase::Ready, GameplayEvent::Ready, 350);
    }
    if (g.life == LifePhase::Ready) {
        if (now < g.deadlineMs) return {};
        if (state_.runId == std::numeric_limits<std::uint64_t>::max() || g.day == UINT32_MAX) throw std::runtime_error("day counter exhausted");
        const auto previous = g;
        Explorer next(state_.runId + 1, state_.rng.state, state_.assetVersion, state_.floors, state_.spawnPolicy);
        next.state_.gameplay.enabled = true;
        next.state_.gameplay.day = previous.day + 1;
        next.state_.gameplay.meals = previous.meals;
        next.state_.gameplay.oran = previous.oran;
        next.state_.gameplay.phaseStartedMs = now; next.state_.gameplay.deadlineMs = now + 350;
        next.assignRelationships();
        *this = std::move(next);
        return {GameplayEvent::Explore, {}};
    }
    if (g.life == LifePhase::Combat) {
        switch (g.combat) {
        case CombatPhase::PlayerTurn: {
            if (now < g.deadlineMs) return {};
            const auto once = "turn:" + std::to_string(state_.runId) + ":" + std::to_string(state_.floorIndex) + ":" + std::to_string(g.enemyInstance) + ":" + std::to_string(++g.turn);
            if (!g.onceKeys.insert(once).second) throw std::logic_error("duplicate combat turn");
            if (g.oran && g.hearts < 3) {
                --g.oran; g.hearts = std::min(3U, g.hearts + 2);
                auto update = combat(CombatPhase::BerryText, GameplayEvent::UseOranBerry, 1000); update.onceKey = once;
                return update;
            }
            const auto roll = rollBasicAttack(state_.rng, true);
            g.hit = roll.hit; g.critical = roll.critical; g.damage = roll.damage;
            g.enemyHits -= std::min(g.enemyHits, roll.damage);
            auto update = combat(CombatPhase::PlayerAttack, GameplayEvent::PlayerAttack); update.onceKey = once; update.damage = roll.damage; update.critical = roll.critical;
            return update;
        }
        case CombatPhase::PlayerAttack:
            if (!finiteDone) return {};
            if (g.hit) return combat(CombatPhase::EnemyHurt, GameplayEvent::Hurt);
            return combat(CombatPhase::EnemyTurn, GameplayEvent::Miss, 350);
        case CombatPhase::EnemyHurt:
            if (!finiteDone) return {};
            return !g.enemyHits ? combat(CombatPhase::Faint, GameplayEvent::EnemyFaint) : combat(CombatPhase::EnemyTurn, GameplayEvent::None, 350);
        case CombatPhase::BerryText:
            if (now < g.deadlineMs) return {};
            return combat(CombatPhase::EnemyTurn, GameplayEvent::None, 350);
        case CombatPhase::EnemyTurn: {
            if (now < g.deadlineMs) return {};
            const auto once = "enemy:" + std::to_string(state_.runId) + ":" + std::to_string(state_.floorIndex) + ":" + std::to_string(g.enemyInstance) + ":" + std::to_string(g.turn);
            if (!g.onceKeys.insert(once).second) throw std::logic_error("duplicate enemy turn");
            const auto roll = rollBasicAttack(state_.rng, false);
            g.hit = roll.hit; g.critical = roll.critical; g.damage = roll.damage;
            g.hearts -= std::min(g.hearts, roll.damage);
            auto update = combat(CombatPhase::EnemyAttack, GameplayEvent::EnemyAttack); update.onceKey = once; update.damage = roll.damage; update.critical = roll.critical;
            return update;
        }
        case CombatPhase::EnemyAttack:
            if (!finiteDone) return {};
            return g.hit ? combat(CombatPhase::PlayerHurt, GameplayEvent::Hurt) : combat(CombatPhase::PlayerTurn, GameplayEvent::Miss, 350);
        case CombatPhase::PlayerHurt:
            if (!finiteDone) return {};
            return !g.hearts ? combat(CombatPhase::Defeat, GameplayEvent::CombatDefeat, 350) : combat(CombatPhase::PlayerTurn, GameplayEvent::None, 350);
        case CombatPhase::Faint:
            if (!finiteDone) return {};
            completeEncounter();
            g.onceKeys.insert("victory:" + key("combat", g.enemyInstance));
            return combat(CombatPhase::Victory, GameplayEvent::CombatVictory, 350);
        case CombatPhase::Victory:
            if (now < g.deadlineMs) return {};
            return life(LifePhase::Explore, GameplayEvent::Explore, 350);
        case CombatPhase::Defeat:
            if (now < g.deadlineMs) return {};
            return beginReturn(now, true);
        }
    }
    if (now < g.deadlineMs) return {};
    // Recognition/contact at an adjacent tile. Friendly actors never initiate attacks.
    for (auto& e : state_.population.entities) {
        if (e.done || e.kind != EntityKind::Friend || distance(e.tile, state_.position) > 1 || state_.blocked[e.tile]) continue;
        const bool hostile = std::find(g.hostile.begin(), g.hostile.end(), e.instanceId) != g.hostile.end();
        invalidate();
        if (hostile) {
            g.life = LifePhase::Combat; g.combat = CombatPhase::PlayerTurn;
            g.enemyInstance = e.instanceId; g.enemyHits = 2; g.turn = 0;
            g.phaseStartedMs = now; g.deadlineMs = now + 350;
            return {GameplayEvent::HostileEncounter, {}};
        }
        const auto once = key("greet", e.instanceId);
        state_.onceKeys.insert(once); e.done = true;
        g.deadlineMs = now + 700;
        return {GameplayEvent::Greeting, once};
    }
    const auto update = advance();
    g.deadlineMs = now + 350;
    switch (update.kind) {
    case EventKind::Pose: return life(LifePhase::Pose, GameplayEvent::Pose, kFiniteTimeoutMs);
    case EventKind::ReturnedHome: return beginReturn(now, false);
    case EventKind::Pickup:
        if (update.originalId == ITEM_ORAN_BERRY && !g.oran) ++g.oran;
        return {GameplayEvent::Pickup, update.onceKey};
    case EventKind::Move: return {GameplayEvent::Move, {}};
    case EventKind::Greeting: return {GameplayEvent::Greeting, update.onceKey};
    case EventKind::FloorChanged: return {GameplayEvent::FloorChanged, update.onceKey};
    default: return {};
    }
}
} // namespace pet
