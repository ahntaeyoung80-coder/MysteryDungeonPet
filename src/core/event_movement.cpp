#include "core/event_movement.hpp"
#include <algorithm>
#include <functional>
#include <limits>
namespace pet {
EventRoute advanceEventRoute(ExplorationState& s,std::uint32_t leader,int target,unsigned stop,std::uint64_t now) {
    auto& g=s.gameplay;auto& event=s.variety.event;
    std::vector<unsigned> ids{0,kCompanionActorId};
    if(leader!=0 && leader!=kCompanionActorId)ids.push_back(leader);
    auto actor=[&](unsigned id)->Entity*{const auto i=std::find_if(s.population.entities.begin(),s.population.entities.end(),[&](auto& a){return a.instanceId==id;});return i==s.population.entities.end()?nullptr:&*i;};
    std::vector<int> old{s.position,g.companion.tile};if(ids.size()==3){if(!actor(leader))return EventRoute::Blocked;old.push_back(actor(leader)->tile);}
    const auto owner=std::size_t(std::find(ids.begin(),ids.end(),leader)-ids.begin());
    auto blocked=s.blocked;
    for(const auto& a:s.population.entities)if(a.kind==EntityKind::Friend && a.visible && std::find(ids.begin(),ids.end(),a.instanceId)==ids.end())blocked[a.tile]=1;
    for(const auto& a:s.population.entities)if(a.kind==EntityKind::Friend && a.visible && !a.done && std::find(g.hostile.begin(),g.hostile.end(),a.instanceId)!=g.hostile.end())
        for(int tile=0;tile<kTiles;++tile)if(canAttack(s.world,tile,a.tile))blocked[tile]=1;
    blocked[s.world.stairs]=1;
    if(target<0 || target>=kTiles || !s.world.walkable(target))return EventRoute::Blocked;
    auto pathMask=blocked;pathMask[target]=0;const auto distance=bfs(s.world,target,pathMask).distance;
    if(distance[old[owner]]>=0 && unsigned(distance[old[owner]])<=stop)return EventRoute::Arrived;
    if(distance[old[owner]]<0)return EventRoute::Blocked;
    const bool reunion=event.task==EventTask::Reunite;
    auto cost=[&](const std::vector<int>& positions) {
        int value=distance[positions[owner]]*100;
        for(std::size_t i=0;i<positions.size();++i)if(i!=owner)
            value+=5*std::max(0,distance[positions[i]])+10*std::max(std::abs(positions[i]%kWidth-positions[owner]%kWidth),std::abs(positions[i]/kWidth-positions[owner]/kWidth));
        return value;
    };
    int bestCost=cost(old);std::vector<int> candidate(old.size()),best;
    std::function<void(std::size_t)> choose=[&](std::size_t i) {
        if(i==old.size()) {
            if(!reunion && !canAttack(s.world,candidate[0],candidate[1]))return;
            if(ids.size()==3 && std::max(std::abs(candidate[2]%kWidth-candidate[0]%kWidth),std::abs(candidate[2]/kWidth-candidate[0]/kWidth))>2)return;
            const auto score=cost(candidate);if(score<bestCost){bestCost=score;best=candidate;}return;
        }
        std::vector<int> next{old[i]};
        if(!reunion || ids[i]==leader)for(const auto& [dx,dy]:kDirections) {
            const auto tile=old[i]+dy*kWidth+dx;
            if(tile>=0 && tile<kTiles && !blocked[tile] && canMove(s.world,old[i],tile,blocked) && distance[tile]>=0)next.push_back(tile);
        }
        for(int tile:next) {
            bool valid=true;
            for(std::size_t j=0;j<i;++j)if(tile==candidate[j] || (tile==old[j] && candidate[j]==old[i]) ||
                (tile!=old[i] && candidate[j]!=old[j] && tile%kWidth+old[i]%kWidth==candidate[j]%kWidth+old[j]%kWidth && tile/kWidth+old[i]/kWidth==candidate[j]/kWidth+old[j]/kWidth))valid=false;
            if(valid){candidate[i]=tile;choose(i+1);}
        }
    };
    choose(0);if(best.empty())return EventRoute::Blocked;
    if(g.actionRound==UINT64_MAX || !g.partyPlaybacks.empty() ||
       std::any_of(g.partyActions.begin(),g.partyActions.end(),[](const auto& a){return !a.finished;}))return EventRoute::Blocked;
    // This also handles an event restored after entry with its old, completed turn.
    g.partyActions.clear();g.partyPhase=PartyPhase::Idle;
    ++g.actionRound;
    for(std::size_t i=0;i<ids.size();++i)if(best[i]!=old[i]) {
        const unsigned dir=directionBetween(old[i],best[i]);MoveRecord move{g.actionRound,now,400,dir,old[i],best[i]};
        if(ids[i]==0){s.position=best[i];s.visited[best[i]]=1;s.discovered[best[i]]=1;g.playerDirection=dir;g.movement=move;g.walkStartedMs=now;g.playerActionRound=g.actionRound;++s.floorSteps;}
        else if(ids[i]==kCompanionActorId){g.companion.tile=best[i];g.companion.direction=dir;g.companion.movement=move;g.companion.walkStartedMs=now;g.companion.actionRound=g.actionRound;}
        else {auto* a=actor(ids[i]);a->tile=best[i];a->direction=dir;a->movement=move;a->actionRound=g.actionRound;}
    }
    g.phaseStartedMs=now;g.deadlineMs=now+400;++s.occupancyVersion;return EventRoute::Moved;
}
} // namespace pet
