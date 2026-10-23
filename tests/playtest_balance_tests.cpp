#include "core/holiday_scene.hpp"
#include "core/exploration_package.hpp"
#include <algorithm>
#include <iostream>
#include <map>
#include <stdexcept>
using namespace pet;
namespace {void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}}
int main(){try {
    Rng enemy{20261023},player{20261023};unsigned playerCriticals=0;
    for(unsigned i=0;i<100000;++i) {
        auto a=rollBasicAttack(enemy,false),b=rollBasicAttack(player,true);
        check(!a.critical && a.damage<=1,"enemy can still critically hit");
        check(a.hit==b.hit && enemy.state==player.state,"removing enemy critical changed RNG cadence");
        playerCriticals+=b.critical;
    }
    check(playerCriticals>14000 && playerCriticals<18000,"player/partner critical policy changed");
    const std::vector<unsigned> ids{1,4,8,14,17,21,35,41};std::map<unsigned,unsigned> chosen;
    for(unsigned seed=1;seed<=4000;++seed) {
        auto v=initialVarietyState(seed,seed,1);const auto event=v.eventRng,dialogue=v.dialogueRng;
        const auto first=chooseDungeon(v,ids);++chosen[first];
        check(v.eventRng==event && v.dialogueRng==dialogue,"first schedule consumed other RNG streams");
        check(chooseDungeon(v,ids,first)!=first,"next expedition repeats current choice");
    }
    check(chosen.size()==8,"first schedule cannot select all eight expeditions");
    for(auto id:ids)check(chosen[id]>350 && chosen[id]<650,"first choice distribution is biased");
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path()/"assets/original/exploration";
    for(auto id:ids) {
        auto p=loadExplorationPackage(root/("dungeon-"+std::to_string(id))/"exploration.mdpe");
        Explorer e(1000,42,p.assetVersion,p.floors,SpawnPolicy{},{},p.expedition);
        const auto count=[](const ExplorationState& s){return std::count_if(s.population.entities.begin(),s.population.entities.end(),[](const auto& x){return x.kind==EntityKind::Item && x.originalId==ITEM_ORAN_BERRY;});};
        if(!p.expedition.floors[0].fixedFloorId)check(count(e.state())>=1,"ordinary expedition floor has no recovery supply");
        auto saved=decodeSnapshot(encodeSnapshot(e.state()),p.assetVersion);
        check(count(saved)==count(e.state()),"restoration regenerated recovery items");
        check(saved.floors[0].guaranteed==p.floors[0].guaranteed,"product supply rewrote original profile");
        Explorer empty(1000,42,p.assetVersion,p.floors,{0,0,0,0},{},p.expedition);
        if(!p.expedition.floors[0].fixedFloorId)check(empty.state().population.entities.empty(),"no-spawn fixture gained supply");
    }
    std::cout<<"enemy noncritical/player critical cadence, eight-way first/next schedules, per-floor recovery supply and restore pass\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
