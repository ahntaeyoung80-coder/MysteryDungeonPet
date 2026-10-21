#include "core/event_runtime.hpp"
#include "event_test_fixture.hpp"
#include <array>
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace pet;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(){try {
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path()/"assets/original";
    const auto c=loadDialogueCatalogue(root/"gameplay/dialogue/catalogue.mdlc");
    const auto maps=loadEventRegistry(root/"gameplay/events/contexts.mder");
    std::vector<ExplorationPackage> packages;
    for(unsigned id:{0U,15U,18U})packages.push_back(loadExplorationPackage(root/"exploration"/("dungeon-"+std::to_string(id))/"exploration.mdpe"));
    unsigned starts=0;
    for(const auto& p:packages)for(unsigned floor=0;floor<p.floors.size();++floor)for(unsigned type=1;type<=9;++type) {
        auto s=eventFixture(p);s.floorIndex=floor;s.world.version=floor+1;
        for(unsigned prior=0;prior<floor;++prior)s.onceKeys.insert("stairs:1300:"+std::to_string(prior)+":0");
        const auto rng=s.rng.state;check(startVarietyEvent(s,VarietyEvent(type),1000,c,1,&maps),"event blocked on supported map/floor");
        auto restored=decodeSnapshot(encodeSnapshot(s),p.assetVersion);
        check(restored.variety.event.id.type==VarietyEvent(type) && restored.variety.event.id.floor==floor && restored.rng.state==rng,"event restore/map identity or combat RNG changed");
        if(type==3) {
            const auto* facts=maps.floor(restored);const auto& items=facts?facts->shopItems:p.floors[floor].floorItems;
            for(const auto& good:restored.variety.event.goods)check(std::any_of(items.begin(),items.end(),[&](const auto& category){return category.weight && std::any_of(category.items.begin(),category.items.end(),[&](const auto& item){return item.originalId==good.item && item.value;});}),"shop fabricated stock from another dungeon");
        }
        if(type==4) {
            const auto* facts=maps.floor(restored);const auto& weights=facts?facts->monsterHouse:p.floors[floor].normalFriends;
            for(auto id:restored.variety.event.opponents){const auto& npc=restored.population.entities.at(id-1);check(std::any_of(weights.begin(),weights.end(),[&](const auto& row){return row.originalId==npc.originalId && row.value;}),"monster house borrowed another dungeon's species");}
        }
        if(type==5 || type==8) {
            unsigned steps=0;
            while(eventActive(restored) && steps++<100){Explorer live(restored);live.advanceGameplay(nextGameplayDeadline(restored),ActorCompletion{true,true,true},c,&maps);restored=decodeSnapshot(encodeSnapshot(live.state()),p.assetVersion);}
            check(steps<100 && restored.variety.event.progress==EventProgress::Finished && restored.variety.committed.size()==1,"new-map wonder/reunion did not finish exactly once");
        }
        ++starts;
    }
    // Equal opportunity outcomes on identical geometry, including original zero-chance maps.
    // Actual scheduler entry, not the frequency-bypassing scenario helper.
    std::array<unsigned,11> offered{};unsigned houses=0;
    for(unsigned seed=0;seed<1024;++seed) {
        unsigned reference=0,referenceHouse=0;
        for(unsigned map=0;map<packages.size();++map) {
            auto s=eventFixture(packages[map]);s.floorSteps=16;s.variety.eventRng=seed;
            const auto combat=s.rng.state;const bool began=tryAutomaticEvent(s,1000,c,&maps);const auto type=began?unsigned(s.variety.event.id.type):0;
            if(!map){reference=type;++offered[type];}else check(type==reference,"regular event probability depends on map");
            check(s.rng.state==combat,"opportunity consumes combat RNG");
            const auto after=s.variety.eventRng;check(!tryAutomaticEvent(s,1000,c,&maps) && s.variety.eventRng==after,"same opportunity rerolled");
            s=eventFixture(packages[map]);s.position=108;s.visited[108]=1;s.gameplay.companion.tile=109;s.population.entities.clear();
            s.gameplay.movement={1,500,400,6,107,108};s.variety.eventRng=seed;
            const bool house=tryAutomaticEvent(s,1000,c,&maps);
            check(!house || s.variety.event.id.type==VarietyEvent::MonsterHouse,"room entry started unrelated event");
            if(!map){referenceHouse=house;houses+=house;}else check(house==bool(referenceHouse),"room-entry MH chance depends on map");
            check(s.gameplay.onceKeys.count("event-room:1300:0:1"),"room entry was not evaluated");
        }
    }
    for(unsigned type:{1U,2U,3U,5U,6U,7U,8U,9U})check(offered[type]>0,"automatic event unreachable on all maps");
    check(houses>20 && houses<85 && offered[8]>0 && offered[8]<30,"special opportunities out of expected range");
    // Frequency, survival and population constraints still apply.
    for(const auto& p:packages) {
        auto s=eventFixture(p);s.floorSteps=16;s.gameplay.hearts=1;check(!tryAutomaticEvent(s,1000,c,&maps),"optional event overrides survival");
        s=eventFixture(p);s.floorSteps=16;s.gameplay.onceKeys.insert("event-floor:1300:0");check(!tryAutomaticEvent(s,1000,c,&maps),"floor limit removed");
        s=eventFixture(p);s.floorSteps=16;s.gameplay.onceKeys.insert("event-start:1300:1");s.gameplay.onceKeys.insert("event-start:1300:2");check(!tryAutomaticEvent(s,1000,c,&maps),"run limit removed");
        s=eventFixture(p);s.spawnPolicy.friendBudget=1;check(!startVarietyEvent(s,VarietyEvent::MonsterHouse,1000,c,0,&maps),"MH exceeded population budget");
        Explorer next(s);const auto other=p.floors.front().dungeonId==18?15:18;
        for(unsigned i=0;i<20;++i){const auto selected=next.chooseNextDungeon({15,18});check(p.floors.front().dungeonId==0?(selected==15 || selected==18):selected==unsigned(other),"two-map selection no longer alternates");}
    }
    std::cout<<starts<<" map/floor/event starts+restore; per-map identical 1024-seed regular outcomes:";
    for(unsigned type=0;type<10;++type)std::cout<<' '<<type<<'='<<offered[type];
    std::cout<<"; room MH="<<houses<<"/1024; local stock/species, wonder/reunion finish once, safety/budget/frequency and map alternation pass\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
