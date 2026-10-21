#include "core/event_runtime.hpp"
#include "core/exploration_package.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace pet;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
ExplorationState fixture(const ExplorationPackage& p) {
    Explorer e(1300,41,p.assetVersion,p.floors,{0,0,0,0});auto s=e.state();
    s.world.terrain.fill(0);s.world.room.fill(-1);
    for(int y=2;y<10;++y)for(int x=2;x<10;++x){s.world.terrain[y*24+x]=1;s.world.room[y*24+x]=0;}
    for(int y=2;y<10;++y)for(int x=12;x<20;++x){s.world.terrain[y*24+x]=1;s.world.room[y*24+x]=1;}
    for(int x=9;x<=12;++x)s.world.terrain[4*24+x]=1;
    s.world.centers={100,111};s.world.start=100;s.world.stairs=111;s.position=100;s.knownStairs=111;
    s.visited.fill(0);s.visited[100]=1;s.discovered=s.world.terrain;
    s.population.entities={{1,237,EntityKind::Friend,101,false}};s.target=-1;s.targetInstance=0;s.goalKind=GoalKind::None;
    auto& g=s.gameplay;g.enabled=true;g.companion.enabled=true;g.companion.tile=99;g.phaseStartedMs=g.deadlineMs=1000;
    s.spawnPolicy.friendBudget=4;
    return s;
}
int main(){try {
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path()/"assets/original";
    const auto p=loadExplorationPackage(root/"exploration/dungeon-15/exploration.mdpe");
    const auto c=loadDialogueCatalogue(root/"gameplay/dialogue/catalogue.mdlc");
    for(auto type:{VarietyEvent::Wonder,VarietyEvent::TiredExplorer,VarietyEvent::GuildVisit}) {
        auto s=fixture(p);const auto rng=s.rng.state;const auto inventory=s.inventory.size();
        check(startVarietyEvent(s,type,1000,c,1),"supported event failed to start");const auto identity=eventKey(s.variety.event.id);const auto variant=s.variety.event.variant;
        unsigned steps=0;std::set<std::string> seen;bool moved=false;
        while(eventActive(s) && steps++<80) {
            s=decodeSnapshot(encodeSnapshot(s),p.assetVersion);
            const auto now=nextGameplayDeadline(s);const auto before=s.population.entities;
            Explorer running(s);running.advanceGameplay(now,ActorCompletion{true,true,true},c);s=running.state();
            if(s.variety.dialogue.active){seen.insert(s.variety.dialogue.lineId);check(s.variety.dialogue.bundle.rfind(variant+".",0)==0,"event mixed variant family");}
            for(const auto& a:s.population.entities)for(const auto& old:before)if(a.instanceId==old.instanceId && a.tile!=old.tile)moved=true;
        }
        check(steps<80 && s.variety.event.progress==EventProgress::Finished && s.variety.event.actors.empty(),"event did not finitely dismiss");
        check(s.rng.state==rng && s.inventory.size()==inventory && s.gameplay.oran==0 && s.gameplay.hearts==kMaxHp,"cosmetic event changed gameplay rewards/RNG");
        check(s.variety.committed.size()==1 && s.variety.committed.begin()->rfind(identity+":",0)==0,"result not committed exactly once");
        check(!seen.empty(),"event did not show authored dialogue");
        if(type==VarietyEvent::GuildVisit)check(moved && std::count_if(s.population.entities.begin(),s.population.entities.end(),[](const auto& a){return (a.originalId==434 || a.originalId==369) && a.done && !a.visible;})==1,"visitor did not walk/dismiss");
    }
    auto s=fixture(p);check(startVarietyEvent(s,VarietyEvent::GuildVisit,1000,c),"cancel visitor setup");cancelVariety(s);
    check(s.variety.event.progress==EventProgress::Abandoned && std::none_of(s.population.entities.begin(),s.population.entities.end(),[](const auto& a){return (a.originalId==434 || a.originalId==369) && a.visible;}),"cancel left temporary visitor in population");
    s=fixture(p);s.spawnPolicy.friendBudget=1;check(!startVarietyEvent(s,VarietyEvent::GuildVisit,1000,c),"visitor exceeded population budget");
    s=fixture(p);s.floorSteps=16;const auto original=s.rng.state;(void)tryAutomaticEvent(s,1000,c);const auto eventRng=s.variety.eventRng;
    for(unsigned i=0;i<10;++i)(void)tryAutomaticEvent(s,1000,c);
    check(s.variety.eventRng==eventRng && s.rng.state==original,"automatic event rerolled in same frame");
    std::cout<<"E01 encouragement / E05 / E09: restore each boundary, variant, result once, no rewards, visitor walk/budget/cancel and single trigger pass\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
