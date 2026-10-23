#include "core/expedition.hpp"
#include "core/exploration.hpp"
#include "core/exploration_io.hpp"
#include <algorithm>
#include <set>
#include <stdexcept>

namespace pet {
namespace {
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
void checkStop(const ExpeditionStop& s) {
    require(s.groundId.rfind("GND-L",0)==0 && s.groundId.size()<64,"invalid expedition ground ID");
    const auto path=std::filesystem::path(s.assetFile);
    require(!path.empty() && !path.has_parent_path() && s.assetFile.find_first_of("\\:")==std::string::npos && path.extension()==".mdpa","unsafe expedition ground asset");
    require(s.width>0 && s.height>0 && s.width<=4096 && s.height<=4096 && s.playerX<s.width && s.partnerX<s.width && s.playerY<s.height && s.partnerY<s.height,"invalid expedition ground dimensions/positions");
}
}
void validateFloorSequence(const std::vector<FloorProfile>& floors,const ExpeditionState& expedition) {
    require(!floors.empty() && floors.size()<=128,"invalid expedition floor count");
    if(!expedition.enabled()) {
        require(expedition.floors.empty() && expedition.entryStops.empty() && expedition.branches.empty() && !expedition.revisit && !expedition.entryPending && !expedition.stopIndex && expedition.outcome==ExpeditionOutcome::None,"expedition data without route");
        for(std::size_t i=0;i<floors.size();++i)require(floors[i].edition==floors[0].edition && floors[i].dungeonId==floors[0].dungeonId && floors[i].localFloor==i,"floor sequence is not a complete dungeon");
        return;
    }
    require((expedition.routeId.rfind("RUN-D",0)==0 || expedition.routeId.rfind("ARC-",0)==0) && expedition.routeId.size()<64 && !expedition.name.empty() && expedition.name.size()<=256,"invalid expedition identity");
    require(expedition.floors.size()==floors.size(),"expedition/profile count mismatch");
    for(std::size_t i=0;i<floors.size();++i) {
        const auto& p=floors[i];const auto& f=expedition.floors[i];
        require(p.edition==floors[0].edition && p.dungeonId==f.dungeonId && p.localFloor==f.localFloor,"expedition floor binding mismatch");
        const bool beginning=i==0 || !expedition.floors[i-1].exitLink.empty();
        require(beginning?p.localFloor==0:(p.dungeonId==floors[i-1].dungeonId && p.localFloor==floors[i-1].localFloor+1),"invalid expedition segment sequence");
        require(f.exitLink.empty() || (i+1<floors.size() && f.exitLink.rfind("LNK-",0)==0 && f.exitLink.size()<=128),"invalid expedition exit link");
        require(f.stops.empty() || !f.exitLink.empty(),"ground stop without advancing link");
        require(f.fixedTiles.size()<=kTiles && f.fixedStart<kTiles && f.fixedExit<kTiles && f.backgroundX>=-4096 && f.backgroundX<=4096 && f.backgroundY>=-4096 && f.backgroundY<=4096,"invalid fixed floor layout");
        if(!f.fixedTiles.empty()){std::set<unsigned> tiles;for(auto t:f.fixedTiles)require(t<kTiles && tiles.insert(t).second,"duplicate fixed terrain tile");require(f.fixedFloorId && tiles.count(f.fixedStart) && tiles.count(f.fixedExit) && f.fixedStart!=f.fixedExit,"invalid fixed floor entry/exit");}
        require(f.stops.size()<=16 && f.fixedSpawns.size()<=128 && f.revisitSpawns.size()<=128 && f.fixedFloorId<=255 && f.revisitFixedFloorId<=255,"expedition floor metadata exceeds limit");
        for(const auto& stop:f.stops)checkStop(stop);
        auto allSpawns=f.fixedSpawns;allSpawns.insert(allSpawns.end(),f.revisitSpawns.begin(),f.revisitSpawns.end());
        for(const auto& spawn:allSpawns)require(f.fixedFloorId && (spawn.md || spawn.item) && spawn.md<=65535 && spawn.item<=65535 && spawn.quantity<=999 && spawn.x<256 && spawn.y<256 && spawn.spawnType<=15,"invalid fixed expedition spawn");
    }
    require(expedition.floors.back().exitLink.empty(),"expedition terminal has outgoing link");
    require(expedition.entryStops.size()<=16 && expedition.branches.size()<=16,"expedition entry/branch count exceeds limit");
    for(const auto& stop:expedition.entryStops)checkStop(stop);
    std::set<unsigned> branches;
    for(auto branch:expedition.branches)require(branch && branch<=191 && branches.insert(branch).second,"invalid expedition branch history");
    require(static_cast<unsigned>(expedition.outcome)<=4,"invalid expedition outcome");
}
bool terminalFloor(const ExplorationState& s) {
    return s.expedition.enabled() && s.floorIndex+1==s.floors.size();
}
int floorExitTile(const ExplorationState& s) {
    if(!terminalFloor(s))return s.world.stairs;
    // Use the room's actual bounds, not the padded 24x24 rendering grid.
    int left=kWidth,right=0,top=kHeight,bottom=0;
    for(int t=0;t<kTiles;++t)if(s.world.walkable(t)) {
        left=std::min(left,t%kWidth);right=std::max(right,t%kWidth);
        top=std::min(top,t/kWidth);bottom=std::max(bottom,t/kWidth);
    }
    const auto reach=bfs(s.world,s.world.start);
    int best=s.world.stairs,score=kTiles*kTiles;
    for(int t=0;t<kTiles;++t)if(reach.distance[t]>=0) {
        const int dx=2*(t%kWidth)-left-right,dy=2*(t/kWidth)-top-bottom;
        const int distance=dx*dx+dy*dy;
        if(distance<score){best=t;score=distance;}
    }
    return best;
}
bool fixedBattlePending(const ExplorationState& s) {
    if(!s.expedition.enabled() || !s.expedition.floors.at(s.floorIndex).fixedFloorId)return false;
    return std::any_of(s.population.entities.begin(),s.population.entities.end(),[&](const auto& e) {
        return e.kind==EntityKind::Friend && !e.done && e.visible &&
            std::find(s.gameplay.hostile.begin(),s.gameplay.hostile.end(),e.instanceId)!=s.gameplay.hostile.end();
    });
}
const ExpeditionStop* expeditionStop(const ExplorationState& s) {
    if(!s.expedition.enabled())return nullptr;
    const auto& e=s.expedition;
    const auto& stops=e.entryPending?e.entryStops:e.floors.at(s.floorIndex).stops;
    return e.stopIndex<stops.size()?&stops[e.stopIndex]:nullptr;
}
bool expeditionScene(const ExplorationState& s) {
    return s.gameplay.sceneBeat>=SceneBeat::ExpeditionFadeIn && s.gameplay.sceneBeat<=SceneBeat::ExpeditionFadeOut;
}
ExpeditionState freshExpedition(ExpeditionState e) {
    e.stopIndex=0;e.entryPending=!e.entryStops.empty();e.outcome=ExpeditionOutcome::None;return e;
}
void validateExpedition(const ExplorationState& s) {
    validateFloorSequence(s.floors,s.expedition);
    if(!s.expedition.enabled())return;
    const auto& e=s.expedition;
    require(!e.entryPending || (!e.entryStops.empty() && s.floorIndex==0),"invalid expedition entry stop");
    require(!e.stopIndex || expeditionStop(s),"invalid expedition stop index");
    require(e.outcome!=ExpeditionOutcome::Success || s.completed,"successful expedition without terminal clear");
    require(!s.completed || e.outcome==ExpeditionOutcome::Success,"terminal clear without expedition success");
    if(expeditionScene(s))require(expeditionStop(s) && s.gameplay.enabled && s.gameplay.life==LifePhase::Explore && s.phase==Phase::Explore && !s.gameplay.homeScene && !s.gameplay.movement.actionId && s.target==-1,"invalid expedition presentation");
}
bool Explorer::canTakeExpeditionBranch(std::uint32_t dungeon) const {
    const auto& s=state_;const auto& e=s.expedition;
    if(e.routeId!="ARC-M07" || s.gameplay.sceneBeat!=SceneBeat::ExpeditionRest ||
       std::find(e.branches.begin(),e.branches.end(),dungeon)!=e.branches.end())return false;
    const auto* stop=expeditionStop(s);if(!stop)return false;
    return (dungeon==9 && stop->groundId=="GND-L023-D06P11A") ||
        (dungeon==11 && stop->groundId=="GND-L024-D07P11A") ||
        (dungeon==13 && stop->groundId=="GND-L025-D08P11A");
}
bool Explorer::takeExpeditionBranch(std::vector<FloorProfile> floors,ExpeditionState branch,std::uint64_t now) {
    if(floors.empty() || !canTakeExpeditionBranch(floors.front().dungeonId))return false;
    validateFloorSequence(floors,branch);
    const auto dungeon=floors.front().dungeonId;
    require(state_.floors.size()+floors.size()<=128 && branch.floors.back().exitLink.empty() &&
        std::all_of(floors.begin(),floors.end(),[&](const auto& f){return f.dungeonId==dungeon;}),"invalid sidepath package");
    auto& e=state_.expedition;const auto stop=*expeditionStop(state_);
    const auto index=e.entryPending?0:state_.floorIndex+1;
    branch.floors.back().exitLink="LNK-BRANCH-D"+std::to_string(dungeon)+"-RETURN";
    branch.floors.back().stops={stop};
    state_.floors.insert(state_.floors.begin()+index,floors.begin(),floors.end());
    e.floors.insert(e.floors.begin()+index,branch.floors.begin(),branch.floors.end());e.branches.push_back(dungeon);
    if(e.entryPending)enterFloor();
    auto& g=state_.gameplay;g.sceneBeat=SceneBeat::ExpeditionRest;g.phaseStartedMs=now;g.deadlineMs=now+4000;
    invalidate();validateState(state_);return true;
}
bool Explorer::setExpeditionRevisit(bool revisit) {
    if(!state_.expedition.enabled() || state_.completed)return false;
    for(unsigned i=0;i<=state_.floorIndex;++i)if(state_.expedition.floors[i].fixedFloorId)return false;
    state_.expedition.revisit=revisit;return true;
}
namespace detail {
void writeExpedition(Writer& w,const ExpeditionState& e) {
    w.str(e.routeId);w.str(e.name);w.u32(static_cast<unsigned>(e.floors.size()));
    auto stops=[&](const std::vector<ExpeditionStop>& rows){w.u32(static_cast<unsigned>(rows.size()));for(const auto& s:rows){w.str(s.groundId);w.str(s.assetFile);for(auto n:{s.width,s.height,s.playerX,s.playerY,s.partnerX,s.partnerY})w.u32(n);w.u32(s.storage);}};
    for(const auto& f:e.floors){w.u32(f.dungeonId);w.u32(f.localFloor);w.u32(f.fixedFloorId);w.u32(f.revisitFixedFloorId);for(const auto* rows:{&f.fixedSpawns,&f.revisitSpawns}){w.u32(static_cast<unsigned>(rows->size()));for(const auto& s:*rows)for(auto n:{s.md,s.item,s.quantity,s.x,s.y,s.spawnType})w.u32(n);}w.u32(static_cast<unsigned>(f.fixedTiles.size()));for(auto tile:f.fixedTiles)w.u32(tile);w.u32(f.fixedStart);w.u32(f.fixedExit);w.u32(f.backgroundX+4096);w.u32(f.backgroundY+4096);w.str(f.exitLink);stops(f.stops);}
    stops(e.entryStops);w.u32(static_cast<unsigned>(e.branches.size()));for(auto b:e.branches)w.u32(b);
    w.u32(e.revisit);w.u32(e.entryPending);w.u32(e.stopIndex);w.u32(static_cast<unsigned>(e.outcome));
}
ExpeditionState readExpedition(Reader& r) {
    ExpeditionState e;e.routeId=r.str();e.name=r.str();const auto count=r.count(128);
    auto stops=[&](){std::vector<ExpeditionStop> rows;const auto n=r.count(16);for(unsigned i=0;i<n;++i){ExpeditionStop s;s.groundId=r.str();s.assetFile=r.str();s.width=r.count(4096);s.height=r.count(4096);s.playerX=r.count(4095);s.playerY=r.count(4095);s.partnerX=r.count(4095);s.partnerY=r.count(4095);s.storage=r.boolean();rows.push_back(std::move(s));}return rows;};
    for(unsigned i=0;i<count;++i){ExpeditionFloor f;f.dungeonId=r.count(191);f.localFloor=r.count(127);f.fixedFloorId=r.count(255);f.revisitFixedFloorId=r.count(255);for(auto* rows:{&f.fixedSpawns,&f.revisitSpawns}){const auto n=r.count(128);for(unsigned j=0;j<n;++j){FixedSpawn s;s.md=r.count(65535);s.item=r.count(65535);s.quantity=r.count(999);s.x=r.count(255);s.y=r.count(255);s.spawnType=r.count(15);rows->push_back(s);}}const auto tiles=r.count(kTiles);for(unsigned j=0;j<tiles;++j)f.fixedTiles.push_back(r.count(kTiles-1));f.fixedStart=r.count(kTiles-1);f.fixedExit=r.count(kTiles-1);f.backgroundX=int(r.count(8192))-4096;f.backgroundY=int(r.count(8192))-4096;f.exitLink=r.str();f.stops=stops();e.floors.push_back(std::move(f));}
    e.entryStops=stops();const auto n=r.count(16);for(unsigned i=0;i<n;++i)e.branches.push_back(r.count(191));
    e.revisit=r.boolean();e.entryPending=r.boolean();e.stopIndex=r.count(15);e.outcome=ExpeditionOutcome(r.count(4));return e;
}
}
}
