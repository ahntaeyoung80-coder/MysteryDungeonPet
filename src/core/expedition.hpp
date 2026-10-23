#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace pet {
struct FixedSpawn {
    std::uint32_t md=0,item=0,quantity=0,x=0,y=0,spawnType=0;
};
struct ExpeditionStop {
    std::string groundId,assetFile;
    std::uint32_t width=0,height=0,playerX=0,playerY=0,partnerX=0,partnerY=0;
    bool storage=false;
};
struct ExpeditionFloor {
    std::uint32_t dungeonId=0,localFloor=0,fixedFloorId=0,revisitFixedFloorId=0;
    std::vector<FixedSpawn> fixedSpawns,revisitSpawns;
    std::vector<std::uint32_t> fixedTiles;
    std::uint32_t fixedStart=0,fixedExit=0;
    int backgroundX=0,backgroundY=0;
    std::string exitLink;
    std::vector<ExpeditionStop> stops;
};
enum class ExpeditionOutcome : std::uint8_t { None, Success, VoluntaryReturn, Defeat, Interrupted };
struct ExpeditionState {
    std::string routeId,name;
    std::vector<ExpeditionFloor> floors;
    std::vector<ExpeditionStop> entryStops;
    std::vector<std::uint32_t> branches;
    bool revisit=false,entryPending=false;
    std::uint32_t stopIndex=0;
    ExpeditionOutcome outcome=ExpeditionOutcome::None;
    bool enabled() const { return !routeId.empty(); }
};
struct ExplorationState;
struct Entity;
constexpr std::uint32_t kBossMaxHp=4;
struct FloorProfile;
void validateExpedition(const ExplorationState&);
void validateFloorSequence(const std::vector<FloorProfile>&,const ExpeditionState&);
bool isBossEntity(const ExplorationState&,const Entity&);
bool upcomingBoss(const ExplorationState&);
bool bossPreparation(const ExplorationState&);
std::string bossPreparationKey(const ExplorationState&);
void migrateBossHealth(ExplorationState&);
bool fixedBattlePending(const ExplorationState&);
bool terminalFloor(const ExplorationState&);
bool bossConversation(const ExplorationState&);
bool bossFriendship(const ExplorationState&);
std::string bossDialogueKey(const ExplorationState&);
int floorExitTile(const ExplorationState&);
const ExpeditionStop* expeditionStop(const ExplorationState&);
bool expeditionScene(const ExplorationState&);
ExpeditionState freshExpedition(ExpeditionState);
namespace detail {
struct Reader;struct Writer;
void writeExpedition(Writer&,const ExpeditionState&);
ExpeditionState readExpedition(Reader&);
}
}
