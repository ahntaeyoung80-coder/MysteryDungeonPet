#include "core/exploration.hpp"

#include <algorithm>
#include <charconv>
#include <limits>
#include <stdexcept>
#include <utility>

namespace pet {
namespace {
void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
std::uint64_t totalWeight(const Weights& weights) {
    std::uint64_t total = 0;
    for (const auto& w : weights) {
        require(w.value <= std::numeric_limits<std::uint64_t>::max() - total,
                "weight sum overflow");
        total += w.value;
    }
    return total;
}
void validateProfile(const FloorProfile& p) {
    require(!p.edition.empty() && p.edition.size() <= 256, "invalid edition");
    require(p.floorItems.size() <= 1024 && p.normalFriends.size() <= 65536 &&
            p.guaranteed.size() <= 65536, "profile count exceeds limit");
    totalWeight(p.normalFriends);
    Weights categories;
    for (const auto& category : p.floorItems) {
        require(category.items.size() <= 65536, "item count exceeds limit");
        require(category.weight == 0 || totalWeight(category.items) > 0,
                "positive category has no conditional items");
        totalWeight(category.items);
        categories.push_back({category.originalId, category.weight});
    }
    totalWeight(categories);
}
void validatePolicy(const SpawnPolicy& p) {
    require(p.itemBudget <= kTiles && p.friendBudget <= kTiles, "spawn budget exceeds grid");
}
}
std::uint64_t Rng::next() {
    auto z = (state += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}
std::uint64_t Rng::bounded(std::uint64_t bound) {
    require(bound != 0, "zero RNG bound");
    // Rejection removes modulo bias. Spawn *placement* never retries coordinates.
    const auto threshold = (std::uint64_t{0} - bound) % bound;
    for (;;) {
        const auto value = next();
        if (value >= threshold) return value % bound;
    }
}
bool World::walkable(int tile) const {
    return tile >= 0 && tile < kTiles && terrain[static_cast<std::size_t>(tile)] == 1;
}
const std::array<std::pair<int,int>,8> kDirections{{{0,1},{1,1},{1,0},{1,-1},{0,-1},{-1,-1},{-1,0},{-1,1}}};
unsigned directionBetween(int from, int to) {
    const int dx=(to%kWidth>from%kWidth)-(to%kWidth<from%kWidth);
    const int dy=(to/kWidth>from/kWidth)-(to/kWidth<from/kWidth);
    for(unsigned i=0;i<8;++i) if(kDirections[i]==std::make_pair(dx,dy)) return i;
    return 0;
}
bool canAttack(const World& w,int from,int to) {
    if(!w.walkable(from)||!w.walkable(to)||from==to) return false;
    const int dx=to%kWidth-from%kWidth,dy=to/kWidth-from/kWidth;
    if(std::abs(dx)>1||std::abs(dy)>1) return false;
    // Binary normal-floor world: impassable terrain in either orthogonal side blocks a diagonal.
    // Actor occupancy on those side tiles is NOT terrain and does not block the corner.
    return !dx || !dy || (w.walkable(from+dx) && w.walkable(from+dy*kWidth));
}
bool canMove(const World& w,int from,int to,const Mask& occupied) {
    return canAttack(w,from,to) && !occupied[to];
}
Search bfs(const World& world, int start, const Mask& blocked) {
    Search result;
    result.distance.fill(-1);
    result.parent.fill(-1);
    if (!world.walkable(start) || blocked[start]) return result;
    std::array<int, kTiles> queue{};
    int head = 0, tail = 0;
    queue[tail++] = start;
    result.distance[start] = 0;
    while (head != tail) {
        const int p = queue[head++], x = p % kWidth, y = p / kWidth;
        for (const auto& [dx, dy] : kDirections) {
            const int nx = x + dx, ny = y + dy;
            if (nx < 0 || nx >= kWidth || ny < 0 || ny >= kHeight) continue;
            const int n = ny * kWidth + nx;
            if (!canMove(world,p,n,blocked) || result.distance[n] != -1) continue;
            result.distance[n] = result.distance[p] + 1;
            result.parent[n] = p;
            queue[tail++] = n;
        }
    }
    return result;
}
std::vector<int> Search::pathTo(int goal) const {
    if (goal < 0 || goal >= kTiles || distance[goal] < 0) return {};
    std::vector<int> path;
    for (int p = goal; parent[p] != -1; p = parent[p]) path.push_back(p);
    std::reverse(path.begin(), path.end());
    return path;
}
World generate(Rng& rng, std::uint64_t version) {
    World world;
    world.version = version;
    world.room.fill(-1);
    std::array<std::pair<int, int>, 4> slots{{{1, 1}, {13, 1}, {1, 13}, {13, 13}}};
    for (std::size_t i = slots.size() - 1; i > 0; --i)
        std::swap(slots[i], slots[static_cast<std::size_t>(rng.bounded(i + 1))]);
    const int count = 2 + static_cast<int>(rng.bounded(2));
    for (int i = 0; i < count; ++i) {
        const int w = 3 + static_cast<int>(rng.bounded(4));
        const int h = 3 + static_cast<int>(rng.bounded(4));
        const int x = slots[i].first + static_cast<int>(rng.bounded(11 - w));
        const int y = slots[i].second + static_cast<int>(rng.bounded(11 - h));
        for (int yy = y; yy < y + h; ++yy) for (int xx = x; xx < x + w; ++xx) {
            world.terrain[yy * kWidth + xx] = 1;
            world.room[yy * kWidth + xx] = static_cast<std::int8_t>(i);
        }
        world.centers.push_back((y + h / 2) * kWidth + x + w / 2);
    }
    for (int i = 1; i < count; ++i) {
        int x = world.centers[i - 1] % kWidth, y = world.centers[i - 1] / kWidth;
        const int bx = world.centers[i] % kWidth, by = world.centers[i] / kWidth;
        const bool horizontalFirst = rng.bounded(2) != 0;
        for (int axis = 0; axis < 2; ++axis) {
            const bool horizontal = axis == 0 ? horizontalFirst : !horizontalFirst;
            int& coordinate = horizontal ? x : y;
            const int target = horizontal ? bx : by;
            while (coordinate != target) {
                world.terrain[y * kWidth + x] = 1;
                coordinate += target > coordinate ? 1 : -1;
            }
            world.terrain[y * kWidth + x] = 1;
        }
    }
    world.start = world.centers.front();
    const auto search = bfs(world, world.start);
    world.stairs = world.centers[1];
    for (std::size_t i = 2; i < world.centers.size(); ++i)
        if (search.distance[world.centers[i]] > search.distance[world.stairs])
            world.stairs = world.centers[i];
    validateWorld(world);
    return world;
}
void validateWorld(const World& world) {
    require(world.version > 0, "invalid grid version");
    require(world.centers.size() == 2 || world.centers.size() == 3, "invalid room count");
    require(world.walkable(world.start) && world.walkable(world.stairs) &&
            world.start != world.stairs, "invalid start/stairs");
    require(world.start == world.centers.front(), "start is not first room center");
    for (std::size_t i = 0; i < world.centers.size(); ++i)
        require(world.walkable(world.centers[i]) && world.room[world.centers[i]] == static_cast<int>(i),
                "invalid room center");
    require(world.room[world.stairs] > 0, "stairs are in start room");
    const auto search = bfs(world, world.start);
    for (int p = 0; p < kTiles; ++p) {
        require(world.terrain[p] <= 1 && world.room[p] >= -1 &&
                world.room[p] < static_cast<int>(world.centers.size()), "invalid terrain/room");
        require(world.terrain[p] || world.room[p] == -1, "wall has room ID");
        require(!world.terrain[p] || search.distance[p] >= 0, "disconnected floor");
        const int x = p % kWidth, y = p / kWidth;
        require((x != 0 && x != kWidth - 1 && y != 0 && y != kHeight - 1) ||
                !world.terrain[p], "open boundary");
    }
}
bool sample(const Weights& weights, Rng& rng, std::uint32_t& id) {
    const auto total = totalWeight(weights);
    if (total == 0) return false;
    auto draw = rng.bounded(total);
    for (const auto& weight : weights) {
        if (draw < weight.value) { id = weight.originalId; return true; }
        draw -= weight.value;
    }
    throw std::logic_error("invalid weighted sample");
}
Population populate(const World& world, const FloorProfile& profile, Rng& rng,
                    const SpawnPolicy& policy, const Mask& occupied) {
    validateWorld(world);
    validateProfile(profile);
    validatePolicy(policy);
    Population result;
    std::vector<int> candidates;
    const auto search = bfs(world, world.start);
    for (int p = 0; p < kTiles; ++p)
        if (search.distance[p] >= 0 && p != world.start && p != world.stairs && !occupied[p])
            candidates.push_back(p);
    auto omit = [&](EntityKind kind, OmissionReason reason, std::uint32_t count, bool guaranteed) {
        if (count) result.omissions.push_back({kind, reason, count, guaranteed});
    };
    auto place = [&](EntityKind kind, std::uint32_t id) {
        const auto index = static_cast<std::size_t>(rng.bounded(candidates.size()));
        result.entities.push_back({static_cast<std::uint32_t>(result.entities.size() + 1),
                                   id, kind, candidates[index], false});
        candidates[index] = candidates.back();
        candidates.pop_back();
    };
    const auto guaranteedCount = static_cast<std::uint32_t>(profile.guaranteed.size());
    const auto guaranteedBudget = std::min(guaranteedCount, policy.itemBudget);
    const auto guaranteedPlaced = std::min(guaranteedBudget, static_cast<std::uint32_t>(candidates.size()));
    for (std::uint32_t i = 0; i < guaranteedPlaced; ++i)
        place(EntityKind::Item, profile.guaranteed[i]);
    omit(EntityKind::Item, OmissionReason::Budget, guaranteedCount - guaranteedBudget, true);
    omit(EntityKind::Item, OmissionReason::Space, guaranteedBudget - guaranteedPlaced, true);
    auto randomSpawns = [&](EntityKind kind, std::uint32_t request, std::uint32_t budget) {
        const auto allowed = std::min(request, budget);
        const auto attempts = std::min(allowed, static_cast<std::uint32_t>(candidates.size()));
        omit(kind, OmissionReason::Budget, request - allowed, false);
        omit(kind, OmissionReason::Space, allowed - attempts, false);
        Weights categories;
        for (std::size_t i = 0; i < profile.floorItems.size(); ++i)
            categories.push_back({static_cast<std::uint32_t>(i), profile.floorItems[i].weight});
        for (std::uint32_t i = 0; i < attempts; ++i) {
            std::uint32_t id = 0;
            bool available;
            if (kind == EntityKind::Item) {
                std::uint32_t category = 0;
                available = sample(categories, rng, category) &&
                            sample(profile.floorItems[category].items, rng, id);
            } else available = sample(profile.normalFriends, rng, id);
            if (!available) {
                omit(kind, OmissionReason::EmptyDistribution, attempts - i, false);
                break;
            }
            place(kind, id);
        }
    };
    randomSpawns(EntityKind::Item, policy.itemRequest, policy.itemBudget - guaranteedPlaced);
    randomSpawns(EntityKind::Friend, policy.friendRequest, policy.friendBudget);
    return result;
}
EncounterAction encounterPolicy(const Entity&) { return EncounterAction::Greet; }
Explorer::Explorer(std::uint64_t runId, std::uint64_t seed, std::string assetVersion,
                   std::vector<FloorProfile> floors, SpawnPolicy policy) {
    require(!floors.empty() && floors.size() <= 128, "invalid floor count");
    validatePolicy(policy);
    for (const auto& p : floors) validateProfile(p);
    state_.runId = runId;
    state_.rng.state = seed;
    state_.assetVersion = std::move(assetVersion);
    state_.floors = std::move(floors);
    state_.spawnPolicy = policy;
    enterFloor();
    validateState(state_);
}
Explorer::Explorer(ExplorationState state) : state_(std::move(state)) {
    validateState(state_);
    if (state_.target != -1 && (!state_.gameplay.enabled || state_.gameplay.life == LifePhase::Explore)) {
        path_ = bfs(state_.world, state_.position, state_.blocked).pathTo(state_.target);
        ++bfsCalls_;
        cachedWorld_ = state_.world.version;
        cachedOccupancy_ = state_.occupancyVersion;
    }
}
void Explorer::invalidate() { path_.clear(); pathIndex_ = 0; state_.target = -1; state_.goalKind = GoalKind::None; }
std::string Explorer::key(const std::string& action, std::uint32_t instance) const {
    return action + ":" + std::to_string(state_.runId) + ":" +
           std::to_string(state_.floorIndex) + ":" + std::to_string(instance);
}
void Explorer::enterFloor() {
    state_.mapSeed = state_.rng.state;
    state_.world = generate(state_.rng, static_cast<std::uint64_t>(state_.floorIndex) + 1);
    state_.population = populate(state_.world, state_.floors[state_.floorIndex],
                                 state_.rng, state_.spawnPolicy);
    state_.position = state_.world.start;
    state_.visited.fill(0);
    state_.visited[state_.position] = 1;
    state_.blocked.fill(0);
    state_.occupancyVersion = 0;
    state_.floorSteps = 0;
    invalidate();
    if (state_.gameplay.enabled) assignRelationships();
}
void Explorer::setBlocked(int tile, bool value) {
    require(state_.world.walkable(tile) && tile != state_.position, "invalid blocker tile");
    if (state_.blocked[tile] != static_cast<std::uint8_t>(value)) {
        require(state_.occupancyVersion != std::numeric_limits<std::uint64_t>::max(),
                "occupancy version exhausted");
        state_.blocked[tile] = value;
        ++state_.occupancyVersion;
        invalidate();
    }
}
void Explorer::plan() {
    const auto search = bfs(state_.world, state_.position, state_.blocked);
    ++bfsCalls_;
    invalidate();
    auto selectEntities = [&](EntityKind kind, GoalKind goal, bool berryOnly = false) {
        int best = kTiles + 1;
        for (const auto& e : state_.population.entities) {
            const int distance = search.distance[e.tile];
            if (state_.gameplay.enabled && kind == EntityKind::Item &&
                ((berryOnly && e.originalId != ITEM_ORAN_BERRY) ||
                 (state_.gameplay.oran && e.originalId == ITEM_ORAN_BERRY))) continue;
            if (!e.done && e.kind == kind && distance >= 0 && distance < best) {
                best = distance; state_.target = e.tile; state_.goalKind = goal;
            }
        }
    };
    if (state_.floorSteps < kFloorStepLimit) {
        if (state_.gameplay.enabled && state_.gameplay.hearts < 3 && !state_.gameplay.oran)
            selectEntities(EntityKind::Item, GoalKind::Item, true);
        if (state_.target == -1) selectEntities(EntityKind::Item, GoalKind::Item);
        if (state_.target == -1) selectEntities(EntityKind::Friend, GoalKind::Friend);
        if (state_.target == -1) {
            int best = kTiles + 1;
            for (int p = 0; p < kTiles; ++p)
                if (!state_.visited[p] && search.distance[p] >= 0 && search.distance[p] < best) {
                    best = search.distance[p]; state_.target = p; state_.goalKind = GoalKind::Frontier;
                }
        }
    }
    if (state_.target == -1 && search.distance[state_.world.stairs] >= 0) {
        state_.target = state_.world.stairs; state_.goalKind = GoalKind::Stairs;
    }
    if (state_.target != -1) path_ = search.pathTo(state_.target);
    cachedWorld_ = state_.world.version;
    cachedOccupancy_ = state_.occupancyVersion;
}
Event Explorer::advance(bool poseFinished) {
    if (state_.gameplay.enabled && state_.gameplay.life != LifePhase::Explore &&
        !(state_.gameplay.life == LifePhase::Pose && poseFinished)) return {};
    if (state_.phase == Phase::Home) return {};
    if (state_.phase == Phase::Pose) {
        if (poseFinished || ++state_.poseSteps >= kPoseTimeout) {
            state_.phase = Phase::Home;
            return {EventKind::ReturnedHome, {}, 0};
        }
        return {};
    }
    ++state_.floorSteps;
    if (state_.target == -1 || cachedWorld_ != state_.world.version ||
        cachedOccupancy_ != state_.occupancyVersion ||
        (state_.floorSteps == kFloorStepLimit && state_.goalKind != GoalKind::Stairs)) plan();
    if (state_.target == -1 || state_.floorSteps > kFloorStepLimit + kTiles) {
        state_.phase = Phase::Home;
        invalidate();
        return {EventKind::ReturnedHome, {}, 0}; // bounded safe return, not a clear
    }
    if (pathIndex_ < path_.size()) {
        state_.position = path_[pathIndex_++];
        state_.visited[state_.position] = 1;
        return {EventKind::Move, {}, 0};
    }
    Event event;
    if (state_.goalKind == GoalKind::Item || state_.goalKind == GoalKind::Friend) {
        for (auto& entity : state_.population.entities) {
            if (entity.tile != state_.target || entity.done) continue;
            const bool item = entity.kind == EntityKind::Item;
            if (!item) (void)encounterPolicy(entity);
            const auto onceKey = key(item ? "pickup" : "greet", entity.instanceId);
            if (state_.onceKeys.insert(onceKey).second) {
                if (item) state_.inventory.push_back({onceKey, entity.originalId});
                event = {item ? EventKind::Pickup : EventKind::Greeting, onceKey, entity.originalId};
            }
            entity.done = true;
            break;
        }
    } else if (state_.goalKind == GoalKind::Stairs) {
        const auto transitionKey = key("stairs");
        state_.onceKeys.insert(transitionKey);
        if (state_.floorIndex + 1 < state_.floors.size()) {
            ++state_.floorIndex;
            enterFloor();
            return {EventKind::FloorChanged, transitionKey, 0};
        }
        const auto clearKey = key("clear");
        state_.completed = true;
        state_.phase = Phase::Pose;
        state_.poseSteps = 0;
        if (state_.onceKeys.insert(clearKey).second) event = {EventKind::Pose, clearKey, 0};
    }
    invalidate();
    return event;
}
void validateState(const ExplorationState& s) {
    const auto& g = s.gameplay;
    require(static_cast<unsigned>(g.life) <= 8 && static_cast<unsigned>(g.combat) <= 9 &&
            g.hearts <= 3 && g.oran <= 1 && g.enemyHits <= 2 && g.damage <= 2 && g.day > 0 &&
            g.hostile.size() <= kTiles && g.onceKeys.size() <= 4096, "invalid gameplay state");
    if (g.enabled) {
        require(g.deadlineMs >= g.phaseStartedMs, "invalid gameplay deadline");
        require(g.life != LifePhase::Combat || (s.phase == Phase::Explore && s.target == -1 && g.enemyInstance > 0 &&
            std::any_of(s.population.entities.begin(), s.population.entities.end(), [&](const auto& e) {
                return e.instanceId == g.enemyInstance && e.kind == EntityKind::Friend &&
                    (!e.done || g.combat == CombatPhase::Victory);
            })), "invalid combat owner");
        require((g.life == LifePhase::Explore || g.life == LifePhase::Combat) ? s.phase == Phase::Explore :
            (g.life == LifePhase::Pose ? s.phase == Phase::Pose : s.phase == Phase::Home), "lifecycle/explorer mismatch");
        require(!g.homeScene || (g.life != LifePhase::Explore && g.life != LifePhase::Pose && g.life != LifePhase::Combat), "home in dungeon state");
        require(g.life == LifePhase::Explore || g.life == LifePhase::Pose || g.life == LifePhase::Combat ||
                g.life == LifePhase::ReturnHome || g.homeScene, "life event without home scene");
        if (g.life == LifePhase::ReturnHome || g.homeScene)
            require(g.onceKeys.count("return:" + std::to_string(s.runId)) != 0, "return transaction missing");
        if (g.life == LifePhase::Eat || g.life == LifePhase::Sleep || g.life == LifePhase::Wake || g.life == LifePhase::Ready)
            require(g.onceKeys.count("meal:" + std::to_string(s.runId)) && g.meals > 0, "meal transaction missing");
        std::set<std::uint32_t> unique;
        for (auto id : g.hostile) require(unique.insert(id).second &&
            std::any_of(s.population.entities.begin(), s.population.entities.end(), [&](const auto& e) { return e.instanceId == id && e.kind == EntityKind::Friend; }), "invalid hostile catalogue");
        for (const auto& once : g.onceKeys) require(!once.empty() && once.size() <= 256, "invalid gameplay journal");
    }
    require(!s.assetVersion.empty() && s.assetVersion.size() <= 256, "invalid asset version");
    require(s.runId != 0 && !s.floors.empty() && s.floors.size() <= 128 &&
            s.floorIndex < s.floors.size(), "invalid run/floor");
    validatePolicy(s.spawnPolicy);
    for (std::size_t i = 0; i < s.floors.size(); ++i) {
        const auto& p = s.floors[i];
        validateProfile(p);
        require(p.edition == s.floors[0].edition && p.dungeonId == s.floors[0].dungeonId &&
                p.localFloor == i, "floor sequence is not a complete dungeon");
    }
    validateWorld(s.world);
    require(s.world.version == static_cast<std::uint64_t>(s.floorIndex) + 1, "floor/grid version mismatch");
    require(s.world.walkable(s.position) && !s.blocked[s.position] && s.visited[s.position],
            "invalid explorer position");
    require(static_cast<unsigned>(s.phase) <= static_cast<unsigned>(Phase::Home), "invalid phase");
    require(static_cast<unsigned>(s.goalKind) <= static_cast<unsigned>(GoalKind::Stairs) &&
            ((s.target == -1) == (s.goalKind == GoalKind::None)), "invalid goal");
    if (s.target != -1) {
        require(s.phase == Phase::Explore && s.world.walkable(s.target) &&
                bfs(s.world, s.position, s.blocked).distance[s.target] >= 0, "invalid goal tile");
        if (s.goalKind == GoalKind::Stairs) require(s.target == s.world.stairs, "invalid stairs goal");
        if (s.goalKind == GoalKind::Item || s.goalKind == GoalKind::Friend)
            require(std::any_of(s.population.entities.begin(), s.population.entities.end(), [&](const auto& e) {
                return e.tile == s.target && !e.done &&
                       e.kind == (s.goalKind == GoalKind::Item ? EntityKind::Item : EntityKind::Friend);
            }), "invalid entity goal");
    }
    require(s.floorSteps <= Explorer::kFloorStepLimit + kTiles + 1 &&
            s.poseSteps <= Explorer::kPoseTimeout, "invalid deadline counter");
    require(s.inventory.size() <= 128 * kTiles && s.onceKeys.size() <= 128 * (kTiles + 2),
            "journal count exceeds limit");
    for (int p = 0; p < kTiles; ++p)
        require(s.visited[p] <= 1 && s.blocked[p] <= 1 &&
                (!(s.visited[p] || s.blocked[p]) || s.world.walkable(p)), "invalid tile mask");
    const auto prefix = [&](const std::string& action, std::uint32_t floor, std::uint32_t instance) {
        return action + ":" + std::to_string(s.runId) + ":" + std::to_string(floor) + ":" + std::to_string(instance);
    };
    std::set<std::string> expected;
    for (std::uint32_t i = 0; i < s.floorIndex; ++i) expected.insert(prefix("stairs", i, 0));
    if (s.completed) {
        require(s.floorIndex + 1 == s.floors.size() && s.phase != Phase::Explore,
                "invalid completed phase");
        require(s.position == s.world.stairs, "clear away from stairs");
        expected.insert(prefix("stairs", s.floorIndex, 0));
        expected.insert(prefix("clear", s.floorIndex, 0));
    } else require(s.phase != Phase::Pose, "pose without clear");
    require(s.population.entities.size() <= kTiles && s.population.omissions.size() <= 16,
            "population count exceeds limit");
    Mask occupied{};
    for (std::size_t i = 0; i < s.population.entities.size(); ++i) {
        const auto& e = s.population.entities[i];
        require(e.instanceId == i + 1 && s.world.walkable(e.tile) &&
                e.tile != s.world.start && e.tile != s.world.stairs && !occupied[e.tile] &&
                static_cast<unsigned>(e.kind) <= 1, "invalid entity");
        occupied[e.tile] = 1;
        const auto onceKey = prefix(e.kind == EntityKind::Item ? "pickup" : "greet",
                                   s.floorIndex, e.instanceId);
        require((s.onceKeys.count(onceKey) != 0) == e.done, "entity/journal mismatch");
        if (e.done) expected.insert(onceKey);
        if (e.done && e.kind == EntityKind::Item) {
            const auto it = std::find_if(s.inventory.begin(), s.inventory.end(),
                                        [&](const auto& item) { return item.onceKey == onceKey; });
            require(it != s.inventory.end() && it->originalId == e.originalId, "pickup/inventory mismatch");
        }
    }
    for (const auto& omission : s.population.omissions)
        require(static_cast<unsigned>(omission.kind) <= 1 && static_cast<unsigned>(omission.reason) <= 2 &&
                omission.count > 0, "invalid omission");
    std::set<std::string> inventoryKeys;
    for (const auto& item : s.inventory) {
        require(item.onceKey.size() <= 256 && s.onceKeys.count(item.onceKey) &&
                inventoryKeys.insert(item.onceKey).second, "invalid inventory journal");
        require(item.onceKey.rfind("pickup:", 0) == 0, "inventory is not a pickup");
        expected.insert(item.onceKey);
    }
    for (const auto& onceKey : s.onceKeys) {
        require(onceKey.size() <= 256, "onceKey exceeds limit");
        const auto a = onceKey.find(':'); const auto b = onceKey.find(':', a + 1);
        const auto c = onceKey.find(':', b + 1);
        require(a != std::string::npos && b != std::string::npos && c != std::string::npos,
                "invalid journal key syntax");
        const auto action = onceKey.substr(0, a);
        auto parse = [&](std::size_t start, std::size_t end) {
            std::uint64_t n = 0;
            const auto result = std::from_chars(onceKey.data() + start, onceKey.data() + end, n);
            require(result.ec == std::errc{} && result.ptr == onceKey.data() + end, "invalid journal key number");
            return n;
        };
        require(parse(a + 1, b) == s.runId, "journal run mismatch");
        const auto floor = parse(b + 1, c); const auto instance = parse(c + 1, onceKey.size());
        require(floor <= s.floorIndex && instance <= kTiles &&
                onceKey == prefix(action, static_cast<std::uint32_t>(floor), static_cast<std::uint32_t>(instance)),
                "invalid journal floor/instance");
        if (action == "pickup" || action == "greet") {
            require(instance > 0, "zero entity instance");
            if (action == "pickup") require(inventoryKeys.count(onceKey), "pickup without inventory");
            if (floor == s.floorIndex) require(expected.count(onceKey), "current entity missing from journal");
        } else require((action == "stairs" || action == "clear") && expected.count(onceKey), "unknown journal key");
    }
    for (const auto& onceKey : expected) require(s.onceKeys.count(onceKey), "missing transaction key");
}
std::vector<FloorProfile> fixtureProfiles(std::uint32_t floorCount) {
    require(floorCount > 0 && floorCount <= 128, "invalid fixture floor count");
    std::vector<FloorProfile> floors;
    for (std::uint32_t i = 0; i < floorCount; ++i)
        floors.push_back({"test-fixture", 0, i,
                          {{0, 1, {{10, 1}, {11, 3}}}, {1, 3, {{20, 2}, {21, 1}}}},
                          {{100, 1}, {101, 2}, {102, 7}}, {30}});
    return floors;
}
} // namespace pet
