#include "core/exploration_package.hpp"
#include "core/exploration_io.hpp"
#include <iostream>
#include <stdexcept>
using namespace pet;
namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
Explorer fixture() {
    FloorProfile first;first.edition="test-fixture";first.dungeonId=14;
    auto second=first;second.dungeonId=15;
    auto last=first;last.dungeonId=16;
    ExpeditionState route;route.routeId="RUN-D014";route.name="fixture expedition";
    ExpeditionStop stop;stop.groundId="GND-L027-D10P21A";stop.assetFile="fixture.mdpa";
    stop.width=stop.height=200;stop.playerX=stop.partnerX=100;stop.playerY=100;stop.partnerY=124;
    ExpeditionFloor a;a.dungeonId=14;a.exitLink="LNK-D014-D015";a.stops={stop};
    ExpeditionFloor b;b.dungeonId=15;b.exitLink="LNK-D015-D016";
    ExpeditionFloor c;c.dungeonId=16;c.fixedFloorId=3;c.fixedSpawns={{415,0,0,8,2,6}};
    route.floors={a,b,c};
    Explorer e(9200,31,"expedition-fixture",{first,second,last},{0,0,0,0},{},route);
    e.enableGameplay(1000);return e;
}
void stairs(Explorer& e) {
    auto s=e.state();s.position=s.world.stairs;s.visited[s.position]=1;s.discovered=s.world.terrain;
    s.target=s.world.stairs;s.goalKind=GoalKind::Stairs;s.targetInstance=0;
    s.gameplay.sceneBeat=SceneBeat::None;s.gameplay.movement={};e=Explorer(std::move(s));
}
void replay(Explorer& e,std::uint64_t now) {
    Explorer restored(decodeSnapshot(encodeSnapshot(e.state()),e.state().assetVersion));
    const auto a=e.advanceGameplay(now,true),b=restored.advanceGameplay(now,true);
    check(a.event==b.event && a.onceKey==b.onceKey && encodeSnapshot(e.state())==encodeSnapshot(restored.state()),"expedition resume differs from uninterrupted play");
}
void quick() {
    auto e=fixture();stairs(e);replay(e,1000);replay(e,2000);
    check(expeditionScene(e.state()) && e.state().floorIndex==0 && !e.state().completed,"segment returned home before shelter");
    check(e.state().gameplay.life==LifePhase::Explore && e.state().gameplay.meals==0,"shelter executed guild lifecycle");
    const auto position=e.state().position;const auto rng=e.state().rng.state;
    replay(e,2750);check(e.state().gameplay.sceneBeat==SceneBeat::ExpeditionRest,"shelter failed to enter rest");
    replay(e,6749);check(e.state().position==position && e.state().rng.state==rng,"shelter advanced exploration");
    replay(e,6750);replay(e,7500);
    check(e.state().floorIndex==1 && e.state().floors[1].dungeonId==15 && !e.state().completed,"shelter did not advance to next segment");
    replay(e,8500);stairs(e);replay(e,8501);replay(e,9501);
    check(e.state().floorIndex==2 && e.state().population.entities.size()==1 && e.state().population.entities[0].originalId==415 && fixedBattlePending(e.state()),"terminal fixed boss was replaced by random profile");
    replay(e,10501);stairs(e);replay(e,10502);
    check(!e.state().completed && e.state().gameplay.sceneBeat!=SceneBeat::FloorFadeOut,"boss could be skipped by stairs");
    auto s=e.state();s.target=-1;s.goalKind=GoalKind::None;s.population.entities[0].done=true;
    s.onceKeys.insert("greet:9200:2:1");e=Explorer(std::move(s));stairs(e);
    replay(e,11000);replay(e,12000);
    check(e.state().completed && e.state().expedition.outcome==ExpeditionOutcome::Success,"final segment did not complete expedition");
    for(unsigned beat=0;beat<8 && e.state().gameplay.life==LifePhase::Pose;++beat)replay(e,e.state().gameplay.deadlineMs);
    check(e.state().gameplay.life==LifePhase::ReturnHome && e.state().variety.completedExplorations==1,"terminal success did not return exactly once");
    replay(e,e.state().gameplay.deadlineMs);check(e.state().gameplay.life==LifePhase::Eat && e.state().gameplay.meals==1,"final return did not start one meal");
    auto voluntary=fixture();voluntary.resetExploration(1001);
    check(voluntary.state().expedition.outcome==ExpeditionOutcome::VoluntaryReturn && !voluntary.state().completed && !voluntary.state().variety.completedExplorations,"voluntary return counted as success");
    auto broken=fixture().state();broken.expedition.floors[1].dungeonId=18;
    bool rejected=false;try{Explorer invalid(broken);}catch(const std::exception&){rejected=true;}
    check(rejected,"broken expedition binding accepted");
}
}
int main(){try{quick();std::cout<<"expedition minimal transition, shelter, fixed boss, terminal return and snapshot replay passed\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
