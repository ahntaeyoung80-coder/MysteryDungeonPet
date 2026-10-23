#include "core/exploration.hpp"
#include <iostream>
#include <stdexcept>
using namespace pet;
namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
void rejected(ExplorationState state,const std::string& key) {
    state.inventory.clear();state.onceKeys.clear();
    state.inventory.push_back({key,70});state.onceKeys.insert(key);
    bool failed=false;try{Explorer invalid(state);}catch(const std::exception&){failed=true;}
    check(failed,"malformed historical pickup accepted");
}
}
int main(){try {
    for(bool partner:{false,true}) {
        Explorer initial(1000,42,"long-route",fixtureProfiles(24),{0,0,0,0});
        initial.enableGameplay(1000);if(partner)initial.enableCompanion();
        auto state=initial.state();const std::string key="pickup:999:23:1";
        state.inventory.push_back({key,70});state.onceKeys.insert(key);
        Explorer live(state);live.resetExploration(1000);
        live.setNextRunPackage("short-route",fixtureProfiles(5));
        unsigned ticks=0;
        while(live.state().gameplay.day==1 && ++ticks<100) {
            live.advanceGameplay(live.state().gameplay.deadlineMs,true);
            (void)encodeSnapshot(live.state());
        }
        check(live.state().gameplay.day==2 && live.state().floors.size()==5,"new day failed to select short route");
        auto restored=decodeSnapshot(encodeSnapshot(live.state()),"short-route");
        check(restored.inventory.size()==1 && restored.inventory[0].onceKey==key && restored.onceKeys.count(key),"historical inventory lost on new day/restore");
        Explorer imported(restored);imported.startGuildMorning(100000);
        check(imported.state().inventory[0].onceKey==key,"normal startup discarded historical stock");
        auto launch=restored;launch.gameplay.day=launch.variety.schedule.day=2;
        launch.variety.schedule.destination=DayDestination::Exploration;launch.variety.schedule.dungeon=0;
        Explorer restarted(launch);restarted.startGuildMorning(200000);
        const auto oldMeals=restarted.state().gameplay.meals;
        restarted.setNextRunPackage("launch-route",fixtureProfiles(3),{},true);
        for(unsigned step=0;step<100 && restarted.state().gameplay.life!=LifePhase::Explore;++step) {
            restarted.advanceGameplay(restarted.state().gameplay.deadlineMs,true);
            (void)encodeSnapshot(restarted.state());
        }
        check(restarted.state().assetVersion=="launch-route" && restarted.state().floors.size()==3 &&
              restarted.state().gameplay.day==2 && restarted.state().gameplay.meals==oldMeals &&
              restarted.state().inventory[0].onceKey==key,"random startup route lost the date, meals or historical inventory");
        rejected(restored,"pickup:999:128:1");
        rejected(restored,"pickup:0:23:1");
        rejected(restored,"pickup:999:23:0");
        rejected(restored,"pickup:999:23:577");
        rejected(restored,"pickup:999:broken:1");
        rejected(restored,"pickup:999999:23:1");
    }
    std::cout<<"long-to-short next-day inventory, partner/solo, startup/restore and malformed historical keys pass\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
