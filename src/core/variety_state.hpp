#pragma once
#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace pet {
struct ExplorationState;
constexpr std::uint32_t kSystemDialogueActorId=UINT32_MAX;
constexpr std::uint32_t kHolidayActorId=UINT32_MAX-2;
// Event numbers deliberately match E01..E10, independently of GameplayEvent.
enum class VarietyEvent : std::uint32_t { None, TiredExplorer, LostPokemon, Shop, MonsterHouse,
    Wonder, LostProperty, JointBattle, Reunion, GuildVisit, Holiday };
enum class EventProgress : std::uint32_t { None, Approach, Dialogue, Action, Result, Dismiss, Finished, Abandoned };
struct EventIdentity {
    VarietyEvent type=VarietyEvent::None;
    std::uint64_t run=0, trigger=0;
    std::uint32_t day=0, floor=0;
};
std::string eventKey(const EventIdentity&);
struct EventReservation {
    std::uint32_t owner=0, item=0, quantity=0;
};
enum class EventTask : std::uint32_t { None, Escort, Collect, FindOwner, Reunite, Battle, Trade };
struct EventGoods {std::uint32_t item=0,quantity=0;};
struct VarietyEventState {
    EventIdentity id;
    EventProgress progress=EventProgress::None;
    std::string variant;
    std::uint64_t startedMs=0, deadlineMs=0;
    std::vector<std::uint32_t> actors; // Existing population IDs; third actor creation belongs to P5.
    std::vector<EventReservation> reservations; // Intent only; never an additional inventory ledger.
    EventTask task=EventTask::None;
    int targetTile=-1,objectTile=-1,sourceTile=-1;
    std::uint32_t item=0,quantity=0,warpedActor=0,stalls=0;
    bool carrying=false;
    std::vector<std::uint32_t> temporaryActors,opponents;
    std::vector<EventGoods> goods;
    bool allyContributed=false,partyContributed=false;
};
struct DialogueSession {
    bool active=false;
    std::uint64_t startedMs=0, deadlineMs=0;
    std::uint32_t catalogueVersion=0, line=0, revealed=0, readRemainingMs=0;
    std::string context, bundle, lineId;
    std::uint32_t speaker=0, listener=0;
};
struct RecentDialogue {
    std::string context, bundle;
    std::uint64_t eligibleAfter=0;
};
constexpr std::uint32_t kMaxStock=99;
struct StockEntry {std::uint32_t owner=0,item=0,quantity=0;};
enum class DayDestination : std::uint32_t { Unassigned, Exploration, Town, HotSpring };
enum class HolidayBeat : std::uint32_t { None, Morning, PathWalk, PathFade, WestWalk, WestVisit, WestExit, WestFade,
    EastWalk, EastDepart, SpringWalk, SpringVisit, BathEnter, BathTalk, BathRest, BathExit, SpringDepart, ReturnFade };
struct HolidayState {
    bool active=false;
    HolidayBeat beat=HolidayBeat::None;
    std::uint32_t npcMd=0,elapsedMs=0;
    std::uint64_t startedMs=0,deadlineMs=0;
};
struct DaySchedule {
    std::uint32_t day=1, dungeon=0;
    DayDestination destination=DayDestination::Unassigned;
    bool mealCommitted=false;
    bool holidayCommitted=false;
};
enum class NoticeKind : std::uint32_t { Pickup=1, Victory, ItemComment };
struct GameplayNotice {
    NoticeKind kind=NoticeKind::Pickup;
    std::uint32_t item=0;
    std::uint64_t startedMs=0,deadlineMs=0;
};
struct VarietyState {
    std::uint64_t eventRng=0, dialogueRng=0, scheduleRng=0;
    std::vector<GameplayNotice> notices; // Committed facts only; no inventory or combat effects.
    DialogueSession dialogue;
    VarietyEventState event;
    DaySchedule schedule;
    std::vector<RecentDialogue> recent;
    std::set<std::string> committed; // Full event/result keys, retained across floor/run replacement.
    std::vector<StockEntry> stock; // Non-Oran quantities only; berries remain in GameplayState/CompanionState.
    bool holidayPolicy=false;
    std::uint32_t completedExplorations=0,lastHolidayAt=0;
    DayDestination lastHoliday=DayDestination::Unassigned;
    HolidayState holiday;
};
VarietyState initialVarietyState(std::uint64_t run, std::uint64_t seed, std::uint32_t day);
enum class VarietyRandomStream { Event, Dialogue, Schedule };
std::uint64_t drawVariety(VarietyState&, VarietyRandomStream, std::uint64_t bound);
void validateVarietyState(const ExplorationState&);
bool varietyBusy(const VarietyState&);
std::uint64_t nextGameplayDeadline(const ExplorationState&);
bool canStartVariety(const ExplorationState&, std::uint64_t now);
// Cancels only pending work; committed results, schedule and RNG states survive.
void cancelVariety(ExplorationState&);
void retireEventActor(ExplorationState&,std::uint32_t);
// Marks one result; caller applies the associated mutation to the same snapshot before saving.
// A duplicate returns false. This function does not grant an item or claim a durable save.
bool commitVarietyResult(ExplorationState&, const std::string& result);
// Catalogue consumers supply the exact selected bundle's line IDs. No substitution on mismatch.
bool reconcileDialogue(ExplorationState&, std::uint32_t catalogueVersion,
                       const std::string& bundle, const std::vector<std::string>& lineIds);
std::uint32_t stockQuantity(const ExplorationState&,std::uint32_t owner,std::uint32_t item);
std::uint32_t reservedQuantity(const ExplorationState&,std::uint32_t owner,std::uint32_t item);
bool changeStock(ExplorationState&,std::uint32_t owner,std::uint32_t item,int delta);
bool eventBattle(const ExplorationState&);
bool partyAlly(const ExplorationState&,std::uint32_t actor);
} // namespace pet
