#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

namespace mdpet {
constexpr int kWidth = 24, kHeight = 24, kTiles = kWidth * kHeight;
using Mask = std::array<std::uint8_t, kTiles>;

// New deterministic RNG, unrelated to the original game's RNG.
struct Rng {
    std::uint64_t state = 0;
    std::uint64_t next();
    std::uint64_t bounded(std::uint64_t bound);
};
struct World {
    Mask terrain{};                       // 0 wall, 1 floor
    std::array<std::int8_t, kTiles> room{}; // -1 corridor/wall
    std::vector<int> centers;
    int start = 0, stairs = 0;
    std::uint64_t version = 1;
    bool walkable(int tile) const;
};
struct Search {
    std::array<int, kTiles> distance{}, parent{};
    std::vector<int> pathTo(int goal) const; // excludes start; empty at start/unreachable
};
Search bfs(const World& world, int start, const Mask& blocked = {});
World generate(Rng& rng, std::uint64_t version = 1);
void validateWorld(const World& world);

struct Weight {
    std::uint32_t originalId = 0;
    std::uint64_t value = 0;
};
using Weights = std::vector<Weight>;
// Returns false for an empty/all-zero distribution. Never rewrites weights.
bool sample(const Weights& weights, Rng& rng, std::uint32_t& id);
struct ItemCategory {
    std::uint32_t originalId = 0;
    std::uint64_t weight = 0;
    Weights items;
};
struct FloorProfile {
    std::string edition; // "test-fixture" is not a ROM edition
    std::uint32_t dungeonId = 0, localFloor = 0;
    std::vector<ItemCategory> floorItems;
    Weights normalFriends; // caller supplies already filtered normal MD references
    std::vector<std::uint32_t> guaranteed;
};
struct SpawnPolicy {
    std::uint32_t itemRequest = 3, friendRequest = 2;
    std::uint32_t itemBudget = 8, friendBudget = 4;
};
enum class EntityKind : std::uint8_t { Item, Friend };
struct Entity {
    std::uint32_t instanceId = 0, originalId = 0;
    EntityKind kind = EntityKind::Item;
    int tile = 0;
    bool done = false;
};
enum class OmissionReason : std::uint8_t { Budget, Space, EmptyDistribution };
struct Omission {
    EntityKind kind = EntityKind::Item;
    OmissionReason reason = OmissionReason::Space;
    std::uint32_t count = 0;
    bool guaranteed = false;
};
struct Population {
    std::vector<Entity> entities;
    std::vector<Omission> omissions; // aggregated: huge requests don't allocate huge logs
};
Population populate(const World&, const FloorProfile&, Rng&, const SpawnPolicy&,
                    const Mask& occupied = {});

// One relationship policy boundary. No battle attributes or battle state.
enum class EncounterAction { Greet };
EncounterAction encounterPolicy(const Entity&);
enum class Phase : std::uint8_t { Explore, Pose, Home };
enum class GoalKind : std::uint8_t { None, Item, Friend, Frontier, Stairs };
enum class EventKind { None, Move, Pickup, Greeting, FloorChanged, Pose, ReturnedHome };
struct Event {
    EventKind kind = EventKind::None;
    std::string onceKey;
    std::uint32_t originalId = 0;
};
struct InventoryEntry {
    std::string onceKey;
    std::uint32_t originalId = 0;
};
struct ExplorationState {
    std::string assetVersion;
    std::uint64_t runId = 0, mapSeed = 0;
    Rng rng;
    std::vector<FloorProfile> floors;
    SpawnPolicy spawnPolicy;
    std::uint32_t floorIndex = 0;
    World world;
    Population population;
    int position = 0;
    Mask visited{}, blocked{};
    std::uint64_t occupancyVersion = 0;
    std::uint32_t floorSteps = 0, poseSteps = 0;
    Phase phase = Phase::Explore;
    bool completed = false;
    std::vector<InventoryEntry> inventory;
    std::set<std::string> onceKeys;
};
class Explorer {
public:
    static constexpr std::uint32_t kFloorStepLimit = kTiles * 8;
    static constexpr std::uint32_t kPoseTimeout = 8;
    Explorer(std::uint64_t runId, std::uint64_t seed, std::string assetVersion,
             std::vector<FloorProfile> floors, SpawnPolicy policy = {});
    explicit Explorer(ExplorationState state);
    const ExplorationState& state() const { return state_; }
    Event advance(bool poseFinished = false); // one tile/action per event/deadline
    void setBlocked(int tile, bool value);
    GoalKind goal() const { return goalKind_; }
    std::uint64_t bfsCalls() const { return bfsCalls_; }
private:
    ExplorationState state_;
    std::vector<int> path_;
    std::size_t pathIndex_ = 0;
    int target_ = -1;
    GoalKind goalKind_ = GoalKind::None;
    std::uint64_t cachedWorld_ = 0, cachedOccupancy_ = 0, bfsCalls_ = 0;
    void enterFloor();
    void plan();
    void invalidate();
    std::string key(const std::string& action, std::uint32_t instance = 0) const;
};

void validateState(const ExplorationState&);
std::vector<std::uint8_t> encodeSnapshot(const ExplorationState&);
ExplorationState decodeSnapshot(const std::vector<std::uint8_t>&,
                                const std::string& expectedAssetVersion);
// Same-directory temporary file + flush + atomic replacement; failure preserves old file.
void saveSnapshot(const std::filesystem::path&, const ExplorationState&);
ExplorationState loadSnapshot(const std::filesystem::path&,
                              const std::string& expectedAssetVersion);
std::vector<FloorProfile> fixtureProfiles(std::uint32_t floorCount = 3);
} // namespace mdpet
