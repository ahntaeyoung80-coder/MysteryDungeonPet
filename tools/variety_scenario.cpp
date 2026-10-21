#include "core/event_runtime.hpp"
#include "core/holiday_scene.hpp"
#include "core/exploration_package.hpp"
#include "../tests/event_test_fixture.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>
using namespace pet;
int main(int argc,char** argv){try {
    if(argc!=5)throw std::runtime_error("usage: pet_variety_scenario ROOT CASE DUNGEON SNAPSHOT; CASE inspect|E01..E09|share|trade|town|spring|bath|completion");
    const std::filesystem::path root=argv[1],output=argv[4];const std::string name=argv[2];
    const auto p=loadExplorationPackage(root/"exploration"/("dungeon-"+std::string(argv[3]))/"exploration.mdpe");
    if(name=="inspect") {const auto s=loadSnapshot(output,p.assetVersion);std::cout<<"{\"event\":"<<unsigned(s.variety.event.id.type)<<",\"progress\":"<<unsigned(s.variety.event.progress)<<",\"commits\":"<<s.variety.committed.size()<<",\"holiday\":"<<s.variety.holiday.active<<",\"beat\":"<<unsigned(s.variety.holiday.beat)<<",\"day\":"<<s.gameplay.day<<",\"meals\":"<<s.gameplay.meals<<",\"oran\":"<<s.gameplay.oran<<",\"apples\":"<<stockQuantity(s,0,109)<<",\"life\":"<<unsigned(s.gameplay.life)<<",\"floor\":"<<s.floorIndex<<",\"completed\":"<<s.completed<<"}\n";return 0;}
    const auto c=loadDialogueCatalogue(root/"gameplay/dialogue/catalogue.mdlc");const auto maps=loadEventRegistry(root/"gameplay/events/contexts.mder");
    const auto now=std::uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    auto s=eventFixture(p);s.gameplay.phaseStartedMs=s.gameplay.deadlineMs=now;
    if(name=="pool") {
        s.variety.holidayPolicy=true;s.variety.completedExplorations=3;
        s.variety.schedule.destination=DayDestination::Town;s.variety.schedule.dungeon=0;
        const auto visits=loadHolidayRegistry(root/"gameplay/holiday");
        if(!startHoliday(s,c,visits,now))throw std::runtime_error("pool holiday ineligible");
        for(unsigned step=0;s.variety.holiday.active && step<100;++step) {
            Explorer e(s);e.advanceGameplay(nextGameplayDeadline(s),ActorCompletion{true,true,true},c,&maps,&visits);s=e.state();
        }
        if(s.gameplay.life!=LifePhase::ReturnHome)throw std::runtime_error("pool visit did not return");
        s.gameplay.phaseStartedMs=now;s.gameplay.deadlineMs=now+500;
    } else if(name=="town" || name=="spring" || name=="bath") {
        s.variety.holidayPolicy=true;s.variety.completedExplorations=3;s.variety.schedule.destination=name=="town"?DayDestination::Town:DayDestination::HotSpring;s.variety.schedule.dungeon=0;
        const auto visits=loadHolidayRegistry(root/"gameplay/holiday");if(!startHoliday(s,c,visits,now))throw std::runtime_error("holiday fixture ineligible");
        if(name=="bath"){s.variety.dialogue={};s.variety.holiday.beat=HolidayBeat::BathRest;s.variety.holiday.startedMs=now;s.variety.holiday.deadlineMs=now+8000;s.variety.event.progress=EventProgress::Action;s.gameplay.deadlineMs=now+8000;}
    } else if(name=="completion") {
        Explorer e(1400,41,p.assetVersion,p.floors,{0,0,0,0});for(unsigned step=0;e.state().floorIndex+1<p.floors.size() && step<100000;++step)e.advance();s=e.state();
        if(s.floorIndex+1!=p.floors.size())throw std::runtime_error("completion fixture did not reach final floor");
        s.position=s.world.stairs;s.visited[s.position]=1;s.discovered=s.world.terrain;s.knownStairs=s.world.stairs;
        s.target=-1;s.targetInstance=0;s.goalKind=GoalKind::None;s.gameplay.enabled=true;s.gameplay.life=LifePhase::Explore;s.gameplay.sceneBeat=SceneBeat::FloorFadeOut;s.gameplay.phaseStartedMs=now;s.gameplay.deadlineMs=now+500;
    } else {
        const auto type=name=="share"?VarietyEvent::TiredExplorer:name=="trade"?VarietyEvent::Shop:VarietyEvent(std::stoul(name.substr(1)));
        if(type==VarietyEvent::MonsterHouse || type==VarietyEvent::JointBattle){s.rng.state=42;s.gameplay.oran=s.gameplay.companion.oran=5;}
        if(name=="share")s.gameplay.oran=2;
        bool started=false;
        for(unsigned seed=0;seed<64 && !started;++seed) {
            auto candidate=s;candidate.variety.eventRng=seed;if(name=="trade")changeStock(candidate,0,109,3);
            if(!startVarietyEvent(candidate,type,now,c,1,&maps))continue;
            if(name=="trade" && candidate.variety.event.task!=EventTask::Trade)continue;
            s=std::move(candidate);started=true;
        }
        if(!started)throw std::runtime_error("event fixture ineligible");
    }
    checkpointDialogue(s,c,now);checkpointHoliday(s,now);saveSnapshot(output,s);std::cout<<"fixture "<<name<<" saved\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
