#include "core/exploration.hpp"
#include "core/exploration_package.hpp"
#include "legacy_snapshot.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
using namespace pet;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
uint64_t nextTime(const Explorer& e,uint64_t now){return std::max(now+1,e.state().gameplay.deadlineMs);}
void crc(std::vector<uint8_t>& bytes) {
    uint32_t value=0xffffffff;
    for(size_t i=0;i<bytes.size()-4;++i) {
        value^=bytes[i];for(unsigned bit=0;bit<8;++bit)value=(value>>1)^((value&1)?0xedb88320U:0);
    }
    value=~value;for(unsigned b=0;b<4;++b)bytes[bytes.size()-4+b]=uint8_t(value>>(8*b));
}
std::vector<uint8_t> legacy7(const ExplorationState& state) {
    auto bytes=encodeSnapshot(state);removePartyBlock(bytes);crc(bytes);
    check(bytes[4]==7,"fixture is not an MDES7 payload");return bytes;
}
void writeLegacy(const std::filesystem::path& path,const ExplorationState& state) {
    const auto bytes=legacy7(state);std::ofstream out(path,std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));
    check(bool(out),"cannot write legacy package fixture");
}
Explorer legacyEntry(const ExplorationPackage& old) {
    Explorer initial(5000,42,old.assetVersion,old.floors,{0,0,0,0});auto s=initial.state();
    const auto reach=bfs(s.world,s.position);int item=-1;
    for(int tile=0;tile<kTiles;++tile)
        if(tile!=s.world.stairs && reach.distance[tile]==1){item=tile;break;}
    check(item>=0,"legacy entry has no legal item neighbor");
    s.population.entities={{1,ITEM_ORAN_BERRY,EntityKind::Item,item,false}};
    Explorer e(s);e.enableGameplay(1000);uint64_t now=1000;
    for(unsigned tick=0;tick<64 && e.state().inventory.empty();++tick) {
        e.advanceGameplay(now,true);now=nextTime(e,now);
    }
    check(e.state().inventory.size()==1 && e.state().gameplay.oran==1,"legacy fixture did not really acquire its berry");
    // This uses the actual MDES7 prefix, not a changed version on MDES8 bytes.
    return Explorer(decodeSnapshot(legacy7(e.state()),old.assetVersion));
}
GameplayUpdate configuredStep(Explorer& e,uint64_t now,const ExplorationPackage& upcoming) {
    const auto before=e.state();
    Explorer copy(decodeSnapshot(encodeSnapshot(before),before.assetVersion));
    copy.setNextRunPackage(upcoming.assetVersion,upcoming.floors);
    const auto expected=e.advanceGameplay(now,true),actual=copy.advanceGameplay(now,true);
    check(expected.event==actual.event && expected.onceKey==actual.onceKey &&
          encodeSnapshot(e.state())==encodeSnapshot(copy.state()),"configured package restore changed state/RNG/journal");
    return expected;
}
void fixtures(const ExplorationPackage& old,const std::filesystem::path& out) {
    std::filesystem::create_directories(out);auto e=legacyEntry(old);
    const auto now=uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    auto active=e.state();active.gameplay.phaseStartedMs=now;active.gameplay.deadlineMs=now+10000;
    active.gameplay.walkStartedMs=0;active.gameplay.movement={};
    writeLegacy(out/"legacy-explore.mdes",active);
    e.resetExploration(100000);uint64_t clock=100000;
    for(unsigned tick=0;tick<100 && e.state().gameplay.sceneBeat!=SceneBeat::RoomExit;++tick) {
        e.advanceGameplay(clock,true);clock=nextTime(e,clock);
    }
    check(e.state().gameplay.life==LifePhase::Ready && e.state().gameplay.sceneBeat==SceneBeat::RoomExit,
          "legacy fixture did not reach its normal morning exit");
    auto ready=e.state();const auto duration=ready.gameplay.deadlineMs-ready.gameplay.phaseStartedMs;
    ready.gameplay.phaseStartedMs=now;ready.gameplay.deadlineMs=now+duration;
    writeLegacy(out/"legacy-ready.mdes",ready);
}
}
int main(int argc,char** argv){try {
    if(argc==5 && std::string(argv[1])=="--fixtures") {
        const auto old=loadExplorationPackage(argv[2]),upcoming=loadExplorationPackage(argv[3]);
        check(old.floors.size()==3 && upcoming.floors.size()==7,"unexpected original package floor counts");
        fixtures(old,argv[4]);return 0;
    }
    check(argc==3,"provide original three-floor and seven-floor packages");
    const auto old=loadExplorationPackage(argv[1]),upcoming=loadExplorationPackage(argv[2]);
    check(old.floors.size()==3 && upcoming.floors.size()==7,"original package floor count is not 3/7");
    auto e=legacyEntry(old);e.enableCompanion();
    auto initial=e.state();initial.gameplay.companion.oran=1;e=Explorer(initial);
    const auto unchanged=encodeSnapshot(e.state());
    e.setNextRunPackage(upcoming.assetVersion,upcoming.floors);
    check(encodeSnapshot(e.state())==unchanged,"upcoming policy rewrote the current run");
    bool rejected=false;try{e.setNextRunPackage("",upcoming.floors);}catch(const std::exception&){rejected=true;}
    check(rejected && encodeSnapshot(e.state())==unchanged,"invalid next package changed the current save");
    const auto inventory=e.state().inventory;const auto originalRun=e.state().runId;
    std::set<uint32_t> oldVisited{e.state().floorIndex},newVisited;
    uint64_t clock=nextTime(e,1000);unsigned restores=0;
    for(unsigned tick=0;tick<5000 && e.state().gameplay.day==1;++tick) {
        configuredStep(e,clock,upcoming);++restores;
        if(e.state().gameplay.day==1) {
            check(e.state().assetVersion==old.assetVersion && e.state().floors.size()==3,
                  "package changed in the middle of the legacy run");
            oldVisited.insert(e.state().floorIndex);
        }
        clock=nextTime(e,clock);
    }
    const auto& day=e.state();
    check(oldVisited==std::set<uint32_t>({0,1,2}),"legacy run did not finish floors one through three");
    check(day.gameplay.day==2 && day.runId==originalRun+1 && day.floors.size()==7 &&
          day.assetVersion==upcoming.assetVersion && day.floorIndex==0,
          "first new day did not select the seven-floor package");
    check(day.inventory.size()==inventory.size() && day.inventory[0].onceKey==inventory[0].onceKey &&
          day.inventory[0].originalId==inventory[0].originalId && day.gameplay.meals==1 &&
          day.gameplay.oran==1 && day.gameplay.companion.oran==1,
          "package transition lost inventory, meal or berry transactions");
    newVisited.insert(0);
    for(unsigned tick=0;tick<5000 && !e.state().completed;++tick) {
        configuredStep(e,clock,upcoming);++restores;newVisited.insert(e.state().floorIndex);
        clock=nextTime(e,clock);
    }
    check(e.state().completed && e.state().floorIndex==6 &&
          newVisited==std::set<uint32_t>({0,1,2,3,4,5,6}),"seven-floor run did not clear its real final floor");
    auto explicitOld=legacyEntry(old);explicitOld.resetExploration(100000);clock=100000;
    for(unsigned tick=0;tick<100 && explicitOld.state().gameplay.day==1;++tick) {
        explicitOld.advanceGameplay(clock,true);clock=nextTime(explicitOld,clock);
    }
    check(explicitOld.state().gameplay.day==2 && explicitOld.state().floors.size()==3 &&
          explicitOld.state().assetVersion==old.assetVersion,"explicit old package unexpectedly upgraded");
    std::cout<<"original 3-floor MDES7 -> full legacy run -> same inventory/new day -> 7-floor clear; "
             <<restores<<" restored decisions; explicit package unchanged\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
