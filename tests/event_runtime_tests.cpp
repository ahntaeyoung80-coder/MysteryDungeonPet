#include "core/event_runtime.hpp"
#include "core/exploration_package.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace pet;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
#include "event_test_fixture.hpp"
int main(){try {
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path()/"assets/original";
    const auto p=loadExplorationPackage(root/"exploration/dungeon-15/exploration.mdpe");
    const auto c=loadDialogueCatalogue(root/"gameplay/dialogue/catalogue.mdlc");
    for(auto type:{VarietyEvent::Wonder,VarietyEvent::TiredExplorer,VarietyEvent::GuildVisit}) {
        auto s=eventFixture(p);const auto rng=s.rng.state;const auto inventory=s.inventory.size();
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
        s=decodeSnapshot(encodeSnapshot(s),s.assetVersion);
        check(s.rng.state==rng && s.inventory.size()==inventory && s.gameplay.oran==0 && s.gameplay.hearts==kMaxHp,"cosmetic event changed gameplay rewards/RNG");
        check(s.variety.committed.size()==1 && s.variety.committed.begin()->rfind(identity+":",0)==0,"result not committed exactly once");
        check(!seen.empty(),"event did not show authored dialogue");
        if(type==VarietyEvent::GuildVisit)check(moved && std::count_if(s.population.entities.begin(),s.population.entities.end(),[](const auto& a){return (a.originalId==434 || a.originalId==369) && a.done && !a.visible;})==1,"visitor did not walk/dismiss");
    }
    auto s=eventFixture(p);check(startVarietyEvent(s,VarietyEvent::GuildVisit,1000,c),"cancel visitor setup");cancelVariety(s);
    check(s.variety.event.progress==EventProgress::Abandoned && std::none_of(s.population.entities.begin(),s.population.entities.end(),[](const auto& a){return (a.originalId==434 || a.originalId==369) && a.visible;}),"cancel left temporary visitor in population");
    s=decodeSnapshot(encodeSnapshot(s),s.assetVersion);
    s=eventFixture(p);s.spawnPolicy.friendBudget=1;check(!startVarietyEvent(s,VarietyEvent::GuildVisit,1000,c),"visitor exceeded population budget");
    s=eventFixture(p);s.floorSteps=16;const auto original=s.rng.state;(void)tryAutomaticEvent(s,1000,c);const auto eventRng=s.variety.eventRng;
    for(unsigned i=0;i<10;++i)(void)tryAutomaticEvent(s,1000,c);
    check(s.variety.eventRng==eventRng && s.rng.state==original,"automatic event rerolled in same frame");
    std::cout<<"E01 encouragement / E05 / E09: restore each boundary, variant, result once, no rewards, visitor walk/budget/cancel and single trigger pass\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
