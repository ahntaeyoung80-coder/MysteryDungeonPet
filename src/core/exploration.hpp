#pragma once

#include "core/variety_state.hpp"
#include <array>
#include <cstdint>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

namespace pet {
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
// Original direction numbering; terrain corners and destination actor occupancy stay separate.
extern const std::array<std::pair<int,int>,8> kDirections;
unsigned directionBetween(int from, int to);
bool canMove(const World&, int from, int to, const Mask& occupied = {});
bool canAttack(const World&, int from, int to);
Search bfs(const World& world, int start, const Mask& blocked = {});
World generate(Rng& rng, std::uint64_t version = 1);
void validateWorld(const World& world);

struct Weight {
    std::uint32_t originalId = 0; // MD index for friends, item_p recordIndex for items
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
    std::uint32_t tilesetId = 0, groupFloor = 0;
};
struct SpawnPolicy {
    std::uint32_t itemRequest = 3, friendRequest = 2;
    std::uint32_t itemBudget = 8, friendBudget = 4;
};
enum class EntityKind : std::uint8_t { Item, Friend };
struct MoveRecord { std::uint64_t actionId=0,startedMs=0; std::uint32_t durationMs=400,direction=0; int from=-1,to=-1; };
enum class Tracking : std::uint8_t { Idle, Chase };
struct Entity {
    std::uint32_t instanceId = 0, originalId = 0;
    EntityKind kind = EntityKind::Item;
    int tile = 0;
    bool done = false;
    bool visible = true; // Old completed NPCs remain absent when importing MDES1/2.
    Tracking tracking=Tracking::Idle;
    int lastSeen=-1, chaseTarget=-1;
    std::uint32_t blockedTurns=0, direction=0, remainingHits=2;
    std::uint64_t actionRound=0;
    MoveRecord movement{};
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
enum class GoalKind : std::uint8_t { None, Item, Friend, Frontier, Stairs, Auxiliary };
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
constexpr std::uint32_t kMaxHp=5, kLowHp=2;
constexpr std::uint32_t MOVE_REGULAR_ATTACK = 355, ITEM_ORAN_BERRY = 70;
enum class LifePhase : std::uint8_t { Explore, Pose, ReturnHome, Emotion, Eat, Sleep, Wake, Ready, Combat, Death, Dialogue };
enum class CombatPhase : std::uint8_t { PlayerTurn, PlayerAttack, EnemyHurt, EnemyTurn, EnemyAttack, PlayerHurt, BerryText, Faint, Victory, Defeat, PlayerFaint,
    CompanionTurn, CompanionAttack, CompanionHurt, CompanionFaint, PlayerApproach, CompanionApproach, CompanionBerryText };
enum class SceneBeat : std::uint8_t { None, DiningPan, DiningExit, BedFace, BedLay, BedFadeOut, NightFadeIn, Sleeping, Waking, MorningFace, MorningFadeOut, ExploreFadeIn, BedFadeIn, FloorFadeOut, FloorFadeIn, WakeDelay, RoomExit, ExplorationBerryText, AlarmEnter, AlarmLeave, CompletionFadeIn, CompletionWalk, CompletionPose, CompletionText, CompletionFadeOut };
enum class DialogueStage : std::uint8_t { Player, Npc };
enum class DeathStage : std::uint8_t { FadeOut, Blackout, PartnerTalk };
struct ActorFinished {
    std::uint64_t runId=0,round=0,actionId=0;
    std::uint32_t floorIndex=0,actorId=0;
};
struct ActorCompletion {
    bool player=false,enemy=false,partner=false;
    std::vector<ActorFinished> actions;
    bool keyed=false;
};
enum class PartyPhase : std::uint8_t { Idle, Allies, Enemies, Player, Companion, OrderedEnemies };
enum class PartyActionKind : std::uint8_t { Wait, Move, Attack, Berry };
enum class CompanionIntent : std::uint8_t { Follow, Engage, Return };
struct PartyAction {
    std::uint64_t id=0,round=0,startedMs=0,deadlineMs=0;
    std::uint32_t actorId=0,targetId=0,damage=0;
    PartyActionKind kind=PartyActionKind::Wait;
    bool hit=false,critical=false,finished=false;
};
struct PartyPlayback {
    std::uint64_t id=0,round=0,startedMs=0,deadlineMs=0;
    std::uint32_t actorId=0,clip=7,direction=0;
};
constexpr std::uint32_t kCompanionActorId=UINT32_MAX-1;
struct CompanionState {
    bool enabled=false,hidden=false;
    int tile=-1;
    std::uint32_t hearts=kMaxHp,oran=0,direction=0;
    std::uint64_t actionRound=0,walkStartedMs=0;
    MoveRecord movement{};
};
struct GameplayState {
    bool enabled = false, defeated = false, homeScene = false;
    LifePhase life = LifePhase::Explore;
    CombatPhase combat = CombatPhase::PlayerTurn;
    std::uint32_t hearts = kMaxHp, oran = 0, enemyHits = 2, enemyInstance = 0, turn = 0;
    std::uint32_t damage = 0, meals = 0, day = 1;
    bool hit = false, critical = false;
    std::uint64_t phaseStartedMs = 0, deadlineMs = 0;
    bool playerHidden=false, fadeInOnly=false;
    DeathStage deathStage=DeathStage::FadeOut;
    SceneBeat sceneBeat=SceneBeat::None;
    DialogueStage dialogueStage=DialogueStage::Player;
    MoveRecord movement{};
    std::uint64_t walkStartedMs=0;
    std::uint64_t actionRound=1, playerActionRound=0;
    std::uint32_t playerDirection=0;
    std::vector<std::uint32_t> hostile;
    std::set<std::string> onceKeys;
    std::vector<std::uint64_t> navigationHistory; // MDES6: bounded joint-state cycle watch, excluding clocks/round numbers.
    CompanionState companion;
    bool enemyTargetsCompanion=false;
    bool independentParty=false;
    std::uint64_t nextPartyId=1;
    PartyPhase partyPhase=PartyPhase::Idle;
    CompanionIntent companionIntent=CompanionIntent::Follow;
    std::uint32_t companionTarget=0;
    std::vector<PartyAction> partyActions;
    std::vector<PartyPlayback> partyPlaybacks;
};
const PartyPlayback* partyPlayback(const GameplayState&,std::uint32_t actorId);
std::uint32_t partyClip(const GameplayState&,std::uint32_t actorId);

enum class GameplayEvent { None, Explore, Pose, ReturnHome, HomeScene, Emotion, Eat, Sleep, Wake, Ready,
    HostileEncounter, PlayerAttack, EnemyAttack, Hurt, Miss, UseOranBerry, EnemyFaint, CombatVictory, CombatDefeat, Greeting, Pickup, Move, FloorChanged, ActorMove, Wait, PlayerFaint, Blackout, DialoguePlayer, DialogueNpc,
    CompanionMove, CompanionAttack, CompanionFaint, Warp };
struct ActorEvent {
    GameplayEvent event=GameplayEvent::None;
    std::uint32_t actorId=0,targetId=0,damage=0;
    std::uint64_t actionId=0,round=0;
    bool critical=false;
};
struct GameplayUpdate {
    GameplayEvent event = GameplayEvent::None;
    std::string onceKey;
    std::uint32_t damage = 0;
    bool critical = false;
    std::uint32_t actorId=0;
    bool navigationDetour=false;
    std::vector<ActorEvent> actors;
};
const char* lifeName(LifePhase);
const char* combatName(CombatPhase);
bool dungeonScene(const GameplayState&);
bool completionScene(const GameplayState&);
bool bedroomScene(const GameplayState&);
bool nightScene(const GameplayState&);
bool showFriendlyDisc(const GameplayState&, bool friendly, bool player);
bool companionDialogue(const GameplayState&);
constexpr std::uint64_t kDialogueMs=1600;
constexpr std::uint64_t kFloorFadeMs=1000, kWakeDelayMs=2000, kRoomExitMs=2200, kBedGreetingMs=1600, kCompletionWalkMs=2000, kMealMs=6000;
std::uint32_t gameplayClip(const GameplayState&);
bool gameplayLoop(const GameplayState&);
bool holdGameplayPose(const GameplayState&);
std::uint32_t companionClip(const GameplayState&);
bool companionLoop(const GameplayState&);
// Shared attack roll: both sides hit 80%; only critical chance differs.
struct AttackRoll { bool hit = false, critical = false; std::uint32_t damage = 0; };
AttackRoll rollBasicAttack(Rng&, bool player);
enum class SurvivalFirst : std::uint8_t { None, Oran, Stairs };
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
    Mask visited{}, blocked{}, discovered{}, failedTargets{}, screenSeen{};
    bool screenExploration=false;
    std::uint32_t viewportSize=200;
    std::uint32_t visitedRooms = 0, stalledActions = 0, targetFailures = 0;
    int knownStairs = -1;
    SurvivalFirst survivalFirst=SurvivalFirst::None;
    std::uint32_t targetInstance = 0;
    std::uint64_t occupancyVersion = 0;
    std::uint32_t floorSteps = 0, poseSteps = 0;
    Phase phase = Phase::Explore;
    bool completed = false;
    std::vector<InventoryEntry> inventory;
    std::set<std::string> onceKeys;
    int target = -1;
    GoalKind goalKind = GoalKind::None;
    GameplayState gameplay;
    VarietyState variety;
};
std::uint64_t navigationFingerprint(const ExplorationState&);
void validatePartyState(const ExplorationState&);
struct DialogueCatalogue;
struct EventRegistry;
class Explorer {
public:
    static constexpr std::uint32_t kFloorStepLimit = kTiles * 8;
    static constexpr std::uint32_t kPoseTimeout = 8;
    Explorer(std::uint64_t runId, std::uint64_t seed, std::string assetVersion,
             std::vector<FloorProfile> floors, SpawnPolicy policy = {},
             std::vector<std::uint64_t> recentLayouts = {});
    explicit Explorer(ExplorationState state);
    const ExplorationState& state() const { return state_; }
    Event advance(bool poseFinished = false); // one tile/action per event/deadline
    void setBlocked(int tile, bool value);
    GoalKind goal() const { return state_.goalKind; }
    std::uint64_t bfsCalls() const { return bfsCalls_; }
    void enableGameplay(std::uint64_t utcMs);
    void startGuildMorning(std::uint64_t utcMs);
    bool enableCompanion();
    void enableScreenExploration(std::uint32_t viewportSize);
    void observeViewport(int cameraX,int cameraY);
    bool roomFullyShown(int room) const;
    void setNextRunPackage(std::string assetVersion,std::vector<FloorProfile> floors);
    void shiftPausedClocks(std::uint64_t deltaMs,std::uint64_t pausedAtMs);
    GameplayUpdate resetExploration(std::uint64_t utcMs);
    bool canResetExploration() const;
    GameplayUpdate advanceGameplay(std::uint64_t utcMs, bool clipFinished = false);
    GameplayUpdate advanceGameplay(std::uint64_t utcMs, ActorCompletion);
    GameplayUpdate advanceGameplay(std::uint64_t utcMs, ActorCompletion,const DialogueCatalogue&,const EventRegistry* maps=nullptr);
    void restoreDialogue(const DialogueCatalogue&,std::uint64_t utcMs);
private:
    ExplorationState state_;
    std::string nextRunAssetVersion_;
    std::vector<FloorProfile> nextRunFloors_; // launch policy; reapplied when an old save is restored
    std::vector<int> path_;
    std::size_t pathIndex_ = 0;
    std::uint64_t cachedWorld_ = 0, cachedOccupancy_ = 0, bfsCalls_ = 0;
    bool retreatProgress_ = false;
    std::vector<std::uint64_t> recentLayouts() const;
    void enterFloor();
    void plan();
    Event moveTo(int tile);
    int detourTile(unsigned repetitions, std::uint64_t signature);
    int companionFollowTile(int playerNext) const;
    int friendlyVacateTile(const Entity&,int playerNext) const;
    void recordCompanionMove(int from,std::uint64_t now);
    void discover();
    Mask occupancy(bool playerPath = false) const;
    bool friendly(const Entity&) const;
    bool lowHealth() const;
    std::uint32_t companionOpponent() const;
    bool selectRoomView(const Search&);
    bool mustRetreat() const;
    int nearestPartyTile(const Entity&) const;
    int combatApproach(const Entity&,bool companion) const;
    GameplayUpdate healCompanion(std::uint64_t now);
    bool selectSurvival(const Search&);
    Mask playerOccupancy() const;
    void invalidate(bool clearTarget = true);
    bool selectPriority(const Search&, bool items, bool frontiers);
    bool selectFriendly(const Search&);
    bool frontier(int tile) const;
    void reevaluateStairs();
    std::string key(const std::string& action, std::uint32_t instance = 0) const;
    void assignRelationships();
    GameplayUpdate beginReturn(std::uint64_t now, bool defeated);
    void completeEncounter();
    GameplayUpdate scheduleDungeon(std::uint64_t now);
    bool beginCompanionDialogue(std::uint64_t now);
    GameplayUpdate beginCombat(Entity&, std::uint64_t now, CombatPhase);
    GameplayUpdate afterCombatAction(std::uint64_t now);
    GameplayUpdate advanceParty(std::uint64_t now,const ActorCompletion&);
};

void validateState(const ExplorationState&);
// Entity positions can change after planning; the route is only a cache.
bool discardStaleEntityGoal(ExplorationState&);
std::vector<std::uint8_t> encodeSnapshot(const ExplorationState&);
ExplorationState decodeSnapshot(const std::vector<std::uint8_t>&,
                                const std::string& expectedAssetVersion);
// Same-directory temporary file + flush + atomic replacement. Interrupted .tmp is never loaded.
void saveSnapshot(const std::filesystem::path&, const ExplorationState&);
ExplorationState loadSnapshot(const std::filesystem::path&,
                              const std::string& expectedAssetVersion);
std::vector<FloorProfile> fixtureProfiles(std::uint32_t floorCount = 3);
} // namespace pet
