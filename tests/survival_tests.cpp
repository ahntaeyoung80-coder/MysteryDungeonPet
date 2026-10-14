#include "core/exploration.hpp"
#include "core/gameplay_scene.hpp"
#include <iostream>
#include <stdexcept>
using namespace pet;
void check(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
ExplorationState fixture() {
    Explorer initial(910,42,"survival",fixtureProfiles(2),{0,0,0,0});auto s=initial.state();
    s.world.terrain.fill(0);s.world.room.fill(-1);
    for(int y=2;y<=7;++y) for(int x=2;x<=7;++x) {s.world.terrain[y*kWidth+x]=1;s.world.room[y*kWidth+x]=0;}
    for(int y=2;y<=7;++y) for(int x=10;x<=15;++x) {s.world.terrain[y*kWidth+x]=1;s.world.room[y*kWidth+x]=1;}
    for(int x=7;x<=10;++x) s.world.terrain[4*kWidth+x]=1;
    s.world.centers={4*kWidth+4,4*kWidth+12};s.world.start=s.world.centers[0];s.world.stairs=s.world.centers[1];
    s.position=3*kWidth+3;s.visited.fill(0);s.visited[s.position]=1;s.discovered.fill(0);s.knownStairs=-1;s.survivalFirst=SurvivalFirst::None;
    s.gameplay.enabled=true;return s;
}
Explorer restore(const Explorer& e) {return Explorer(decodeSnapshot(encodeSnapshot(e.state()),"survival"));}
int main() {try {
    GameplayState g;g.sceneBeat=SceneBeat::RoomExit;g.phaseStartedMs=1000;
    auto bed=bedroomExit({204,184},g,1000);check(bed.position.x==204&&bed.position.y==184&&!bed.finished,"exit start teleport");
    auto floor=bedroomExit({204,184},g,1400);check(floor.position.x==204&&floor.position.y==208&&floor.direction==6,"bed did not join left floor route");
    auto bend=bedroomExit({204,184},g,2800);check(bend.position.x==120&&bend.position.y==208&&bend.direction==4,"exit cuts over other bed");
    auto hall=bedroomExit({204,184},g,3200);check(hall.position.x==120&&hall.position.y==184&&hall.direction==6,"exit missed left hall");
    auto outside=bedroomExit({204,184},g,1000+kRoomExitMs);check(outside.position.x<0&&outside.position.y==184&&outside.finished,"exit went through bottom wall");
    auto s=fixture();s.gameplay.oran=1;s.gameplay.hearts=4;Explorer one(s);
    check(one.advanceGameplay(1000).event!=GameplayEvent::UseOranBerry&&one.state().gameplay.oran==1&&one.state().gameplay.hearts==4,"one missing heart wastes berry");
    s.gameplay.hearts=3;Explorer two(s);check(two.advanceGameplay(1000).event==GameplayEvent::UseOranBerry&&two.state().gameplay.hearts==5&&two.state().gameplay.oran==0,"two missing hearts did not heal outside combat");
    auto saved=encodeSnapshot(two.state());two=restore(two);two.advanceGameplay(1500,true);two.advance();check(saved==encodeSnapshot(two.state()),"healing restore consumed twice or ticked during text");
    check(two.state().gameplay.playerActionRound==two.state().gameplay.actionRound,"berry action not consumed");
    s=fixture();s.gameplay.hearts=2;s.population.entities={{1,10,EntityKind::Item,3*kWidth+4,false},{2,70,EntityKind::Item,3*kWidth+6,false}};
    Explorer oran(s);oran.advanceGameplay(1000);check(oran.goal()==GoalKind::Item&&oran.state().targetInstance==2,"low HP prefers ordinary item over berry");
    check(oran.state().survivalFirst==SurvivalFirst::Oran,"first berry observation not saved");
    s=oran.state();s.discovered[s.world.stairs]=1;s.knownStairs=s.world.stairs;oran=Explorer(s);oran.advanceGameplay(1400);check(oran.goal()==GoalKind::Item&&oran.state().targetInstance==2,"later stairs override first berry");
    s=fixture();s.gameplay.hearts=2;s.discovered[s.world.stairs]=1;s.knownStairs=s.world.stairs;s.survivalFirst=SurvivalFirst::Stairs;
    s.population.entities={{1,70,EntityKind::Item,3*kWidth+6,false}};Explorer firstStairs(s);firstStairs.advanceGameplay(1000);check(firstStairs.goal()==GoalKind::Stairs,"stairs-first retreat ignored");
    firstStairs=restore(firstStairs);check(firstStairs.state().survivalFirst==SurvivalFirst::Stairs,"discovery order lost on restore");
    s.population.entities.clear();Explorer noBerry(s);noBerry.advanceGameplay(1000);check(noBerry.goal()==GoalKind::Stairs,"missing berry did not retreat");
    s.population.entities={{1,4,EntityKind::Friend,3*kWidth+4,false}};s.gameplay.hostile={1};Explorer avoid(s);
    check(avoid.advanceGameplay(1000).event==GameplayEvent::Move&&avoid.state().gameplay.life==LifePhase::Explore,"low HP initiated adjacent combat");
    check(!canAttack(avoid.state().world,avoid.state().population.entities[0].tile,avoid.state().position),"retreat chose enemy attack range with safe alternative");
    s=fixture();s.population.entities={{1,10,EntityKind::Item,s.position,false},{2,4,EntityKind::Friend,3*kWidth+6,false}};s.gameplay.hostile={2};Explorer pickup(s);
    check(pickup.advanceGameplay(1000).event==GameplayEvent::Pickup&&pickup.state().gameplay.playerActionRound==1&&pickup.state().gameplay.deadlineMs==1400,"pickup did not consume exactly one turn");
    saved=encodeSnapshot(pickup.state());pickup=restore(pickup);pickup.advanceGameplay(1399,true);check(saved==encodeSnapshot(pickup.state()),"pickup acted before deadline");
    check(pickup.advanceGameplay(1400).event==GameplayEvent::ActorMove&&pickup.state().inventory.size()==1,"pickup did not grant remaining enemy action or duplicated item");
    for(bool done:{false,true}) {
        s=fixture();s.population.entities={{1,4,EntityKind::Friend,3*kWidth+4,done},{2,10,EntityKind::Item,3*kWidth+6,false}};
        if(done) s.onceKeys.insert("greet:910:0:1");
        for(int tile=0;tile<kTiles;++tile) if(s.world.walkable(tile)&&tile!=s.position&&tile/kWidth!=3) s.blocked[tile]=1;
        Explorer swap(s);check(swap.advanceGameplay(1000).event==GameplayEvent::Move,"friendly corridor did not permit swap");
        const auto& after=swap.state();check(after.position==3*kWidth+4&&after.population.entities[0].tile==3*kWidth+3&&after.population.entities[0].done==done,"swap lost actor or completed greeting");
        check(after.gameplay.movement.from==after.population.entities[0].movement.to&&after.gameplay.movement.to==after.population.entities[0].movement.from,"swap presentation is not opposite simultaneous moves");
        check(after.gameplay.playerActionRound==1&&after.gameplay.actionRound==1&&after.goalKind==GoalKind::Item,"swap consumes extra turn or loses goal");
        saved=encodeSnapshot(after);swap=restore(swap);swap.advanceGameplay(1200,true);check(saved==encodeSnapshot(swap.state()),"swap repeated on restore");
        swap.advanceGameplay(1400);check(swap.state().position==3*kWidth+5&&swap.state().population.entities[0].tile==3*kWidth+3,"swap did not unblock next step");
    }
    // A saved MDES4 exit must follow the new corridor duration after upgrading.
    s=fixture();s.phase=Phase::Home;s.gameplay.homeScene=true;s.gameplay.life=LifePhase::Ready;
    s.gameplay.sceneBeat=SceneBeat::RoomExit;s.gameplay.phaseStartedMs=1000;s.gameplay.deadlineMs=2200;s.gameplay.meals=1;
    s.gameplay.onceKeys={"return:910","meal:910"};
    auto old=encodeSnapshot(s);old.erase(old.end()-8,old.end()-4);old[4]=4;uint32_t crc=0xffffffff;
    for(size_t i=0;i<old.size()-4;++i) {crc^=old[i];for(int bit=0;bit<8;++bit) crc=(crc>>1)^((crc&1)?0xedb88320U:0);}
    crc=~crc;for(unsigned b=0;b<4;++b) old[old.size()-4+b]=uint8_t(crc>>(8*b));
    auto upgraded=decodeSnapshot(old,"survival");check(upgraded.gameplay.deadlineMs==1000+kRoomExitMs,"old exit save skipped the left corridor");
    std::cout<<"sleep exit, berry threshold, escape, pickup turn and friendly swap tests pass\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
