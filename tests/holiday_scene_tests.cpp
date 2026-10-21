#include "core/holiday_scene.hpp"
#include "core/npc_characters.hpp"
#include "event_test_fixture.hpp"
#include <iostream>
#include <set>
#include <stdexcept>
using namespace pet;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){try {
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path()/"assets/original";
    const auto p=loadExplorationPackage(root/"exploration/dungeon-15/exploration.mdpe");
    const auto c=loadDialogueCatalogue(root/"gameplay/dialogue/catalogue.mdlc");const auto visits=loadHolidayRegistry(root/"gameplay/holiday");
    const auto npcs=loadNpcCatalogue(root/"gameplay/npcs/characters.mdnc");
    { Explorer next(eventFixture(p));const auto rng=next.state().rng.state,dialogue=next.state().variety.dialogueRng;
      check(next.chooseNextDungeon({15,18})==18 && next.state().rng.state==rng && next.state().variety.dialogueRng==dialogue,"daily map changed world/dialogue RNG or repeated current map");
      check(next.chooseNextDungeon({15})==15,"single verified map did not terminate"); }
    check(visits.floats.size()==34 && visits.routes.size()==7,"holiday selection count");
    for(const auto& [id,binding]:visits.floats){const auto body=loadPackage(root/"gameplay/holiday"/binding.manifest);check(body.assetKey==binding.key && body.pngSha256==binding.hash,"Float identity/hash mismatch");for(unsigned d=0;d<8;++d)check(!body.frames(20,d).empty(),"Float original direction missing");}
    const auto water=loadPackage(root/"gameplay/holiday/water.mdpa");
    for(unsigned effect=625;effect<=629;++effect)for(unsigned front=0;front<2;++front){const auto& f=water.frames(effect*2+front,0);check(f.size()==3 && f[0].ticks==12 && f[1].ticks==12 && f[2].ticks==12,"water original frame/tick/priority absent");}
    for(auto destination:{DayDestination::Town,DayDestination::HotSpring}) {
        auto s=eventFixture(p);s.variety.holidayPolicy=true;s.variety.completedExplorations=3;s.variety.schedule.destination=destination;s.variety.schedule.dungeon=0;
        const auto rng=s.rng.state,inventory=s.inventory.size();const auto oldDay=s.gameplay.day,meals=s.gameplay.meals;
        check(startHoliday(s,c,visits,1000),"holiday could not start");check(npcs.forActor(s,kHolidayActorId).id==(destination==DayDestination::Town?383U:352U),"holiday actor role confused with dungeon instance");
        std::set<HolidayBeat> seen;bool bath=false;unsigned steps=0;std::uint64_t now=1000;
        while(s.variety.holiday.active && steps++<100) {
            seen.insert(s.variety.holiday.beat);const auto sample=holidaySample(s,visits,now);bath|=sample.bathing;
            check(sample.player.x!=sample.partner.x || sample.player.y!=sample.partner.y,"holiday party overlapped");
            if(sample.moving)for(auto t=s.variety.holiday.startedMs;t<s.variety.holiday.deadlineMs;t+=33) {
                const auto frame=holidaySample(s,visits,t);const auto dx=frame.player.x-frame.partner.x,dy=frame.player.y-frame.partner.y;
                check(dx*dx+dy*dy>=144,"moving holiday party crossed/overlapped");
            }
            auto saved=s;checkpointHoliday(saved,now);checkpointDialogue(saved,c,now);s=decodeSnapshot(encodeSnapshot(saved),p.assetVersion);
            now=nextGameplayDeadline(s);Explorer e(s);e.advanceGameplay(now,ActorCompletion{true,true,true},c,nullptr,&visits);s=e.state();
        }
        check(steps<100 && s.gameplay.life==LifePhase::ReturnHome && s.variety.schedule.holidayCommitted && s.variety.lastHoliday==destination,"holiday did not finitely return once");
        check(s.gameplay.day==oldDay && s.rng.state==rng && s.inventory.size()==inventory && !s.completed,"holiday advanced exploration/rewards/date");
        check(seen.count(HolidayBeat::PathWalk) && (destination==DayDestination::Town?seen.count(HolidayBeat::WestVisit)&&seen.count(HolidayBeat::EastDepart):seen.count(HolidayBeat::SpringVisit)&&seen.count(HolidayBeat::BathExit)&&bath),"visit skipped town/spring phases");
        while(s.gameplay.day==oldDay && steps++<180){Explorer e(s);now=s.gameplay.deadlineMs;e.advanceGameplay(now,ActorCompletion{true,true,true},c,nullptr,&visits);s=decodeSnapshot(encodeSnapshot(e.state()),p.assetVersion);}
        check(s.gameplay.day==oldDay+1 && s.gameplay.meals==meals+1 && s.variety.schedule.destination==DayDestination::Exploration,"holiday evening/day/meal repeated or did not finish");
    }
    auto s=eventFixture(p);s.variety.schedule.destination=DayDestination::HotSpring;s.variety.schedule.dungeon=0;s.variety.holidayPolicy=true;s.variety.completedExplorations=3;
    check(startHoliday(s,c,visits,1000),"resume fixture");checkpointHoliday(s,1100);checkpointDialogue(s,c,1100);const auto variant=s.variety.event.variant;
    Explorer restored(decodeSnapshot(encodeSnapshot(s),p.assetVersion));restored.restoreHolidayClock(100000);restored.restoreDialogue(c,100000);
    check(restored.state().variety.event.variant==variant && restored.state().variety.schedule.destination==DayDestination::HotSpring && revealedCatalogueText(restored.state(),c,100000)==revealedCatalogueText(s,c,1100),"holiday resume rerolled/lost read position");
    Explorer morning(restored.state());morning.startGuildMorning(100000);
    check(!morning.state().variety.holiday.active && morning.state().variety.schedule.destination==DayDestination::HotSpring,"normal launch rerolled same-day destination");
    for(bool spring:{false,true}) {
        auto day=eventFixture(p);auto& g=day.gameplay;day.phase=Phase::Home;g.life=LifePhase::Ready;g.homeScene=true;g.sceneBeat=SceneBeat::MorningFadeOut;
        g.onceKeys.insert("return:1300");g.onceKeys.insert("meal:1300");g.meals=1;g.deadlineMs=1000;
        day.variety.holidayPolicy=true;day.variety.completedExplorations=spring?6:3;day.variety.lastHolidayAt=spring?3:0;day.variety.lastHoliday=spring?DayDestination::Town:DayDestination::Unassigned;
        Explorer next(day);next.advanceGameplay(1000,ActorCompletion{true,true,true},c,nullptr,&visits);
        check(next.state().variety.schedule.destination==(spring?DayDestination::HotSpring:DayDestination::Town),"completed-day policy did not alternate fixed destinations");
    }
    std::cout<<"town/spring full visits, Float34/water625-629, exact actor, per-stage restore, same-day schedule, no explore reward, evening/meal/day once pass\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
