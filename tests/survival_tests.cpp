#include "core/exploration.hpp"
#include "legacy_snapshot.hpp"
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
    auto outside=bedroomExit({204,184},g,1000+kRoomExitMs);check(outside.position.x==80&&outside.position.y==184&&outside.finished,"exit did not stop at left corridor entrance");
    int previousX=204,previousCamera=180;
    for(uint64_t time=1000;time<=1000+kRoomExitMs;time+=10) {
        const auto sample=bedroomExit({204,184},g,time),follower=bedroomExit({156,184},g,time,true);
        const auto p=sample.position,partner=follower.position;
        check(p.x<=previousX && p.x>=80 && p.y==184,"exit crossed map boundary or reversed");
        check(partner.y==184,"partner left the straight exit line");
        // Pixel overlap is expected while the leader passes; player renders above partner.
        if(time>=2200)check(p.x<=partner.x-23,"partner led after starting to follow");
        if(time<2200)check(partner.x==156 && partner.y==184 && !follower.moving,"partner moved before leader passed");
        const auto camera=bedroomExitCamera(g,time);
        check(camera.x<=previousCamera && camera.y==184 && previousCamera-camera.x<=1,"exit camera snapped or reversed");
        previousCamera=camera.x;previousX=p.x;
    }
    check(!bedroomExit({204,184},g,1000+kRoomExitMs).moving && !bedroomExit({156,184},g,1000+kRoomExitMs,true).moving,"arrived actors kept walking in place");
    check(bedroomExit({204,184},g,1000).direction==6 && bedroomExit({204,184},g,1300).direction==6 && bedroomExit({204,184},g,3100).direction==6,"exit animation direction did not follow the path");
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
        Explorer swap(s);uint64_t moveAt=1000;
        if(!done) {
            check(swap.advanceGameplay(1000).event==GameplayEvent::DialoguePlayer && swap.state().targetInstance==2,"path NPC skipped greeting or replaced item goal");
            swap=restore(swap);check(swap.advanceGameplay(2200,true).event==GameplayEvent::DialogueNpc,"path NPC skipped response");
            swap=restore(swap);check(swap.advanceGameplay(3400,true).event==GameplayEvent::Explore,"path dialogue stuck");
            check(swap.state().population.entities[0].done && swap.state().gameplay.playerActionRound==0,"greeting consumed walk turn");
            moveAt=3400;
        }
        check(swap.advanceGameplay(moveAt).event==GameplayEvent::Move,"friendly corridor did not permit swap after dialogue");
        const auto& after=swap.state();check(after.position==3*kWidth+4&&after.population.entities[0].tile==3*kWidth+3&&after.population.entities[0].done,"swap lost actor or completed greeting");
        check(after.gameplay.movement.from==after.population.entities[0].movement.to&&after.gameplay.movement.to==after.population.entities[0].movement.from,"swap presentation is not opposite simultaneous moves");
        check(after.gameplay.playerActionRound==1&&after.gameplay.actionRound==1&&after.goalKind==GoalKind::Item,"swap consumes extra turn or loses goal");
        saved=encodeSnapshot(after);swap=restore(swap);swap.advanceGameplay(moveAt+200,true);check(saved==encodeSnapshot(swap.state()),"swap repeated on restore");
        swap.advanceGameplay(moveAt+400);check(swap.state().position==3*kWidth+5&&swap.state().population.entities[0].tile==3*kWidth+3,"swap did not unblock next step");
    }
    // A chasing hostile must not insert a separate 400ms idle between retreat steps.
    s=fixture();s.gameplay.hearts=2;s.position=3*kWidth+5;s.visited.fill(0);s.visited[s.position]=1;
    s.population.entities={{1,4,EntityKind::Friend,3*kWidth+2,false}};s.gameplay.hostile={1};
    s.discovered=s.world.terrain;s.knownStairs=s.world.stairs;s.survivalFirst=SurvivalFirst::Stairs;
    Explorer flee(s);unsigned enemyMoves=0;
    for(uint64_t time=1000;time<3000;time+=400) {
        const int oldEnemy=flee.state().population.entities[0].tile;
        check(flee.advanceGameplay(time).event==GameplayEvent::Move,"retreat inserted idle/hostile-only turn");
        enemyMoves+=flee.state().population.entities[0].tile!=oldEnemy;
        const auto& current=flee.state();check(current.gameplay.deadlineMs==time+400,"retreat cadence changed");
        flee=restore(flee);
        auto before=encodeSnapshot(flee.state());flee.advanceGameplay(time+399,true);check(before==encodeSnapshot(flee.state()),"retreat advanced twice within one movement");
    }
    check(enemyMoves>0,"retreat fixture never exercised chasing enemy");
    for(unsigned hp:{1U,5U}) {
        s=fixture();s.population.entities={{1,4,EntityKind::Friend,3*kWidth+4,false}};
        s.gameplay.hostile={1};s.gameplay.life=LifePhase::Combat;s.gameplay.combat=CombatPhase::EnemyTurn;
        s.gameplay.enemyInstance=1;s.gameplay.hearts=hp;
        for(uint64_t seed=1;seed<10000;++seed) {Rng r{seed};const auto roll=rollBasicAttack(r,false);if(roll.hit) {s.rng.state=seed;break;}}
        Explorer hit(s);check(hit.advanceGameplay(1000).event==GameplayEvent::EnemyAttack && hit.state().gameplay.hearts==hp,"heart removed before enemy attack completion");
        Surface hearts(200,200);gameplayOverlay(hearts,hit.state().gameplay,1000);
        check(hearts.pixels[(8*200+8+18*(hp-1)+2)*4+2]==230,"rendered heart removed during attack windup");
        const auto attackRng=hit.state().rng.state;hit=restore(hit);
        hit.advanceGameplay(1001,ActorCompletion{true,false,false});check(hit.state().gameplay.hearts==hp,"player completion applied enemy damage");
        check(hit.advanceGameplay(1002,ActorCompletion{false,true,false}).event==GameplayEvent::Hurt && hit.state().gameplay.hearts==hp-1,"completed enemy attack did not apply exact impact");
        gameplayOverlay(hearts,hit.state().gameplay,1002);
        check(hearts.pixels[(8*200+8+18*(hp-1)+2)*4+2]==65,"rendered heart retained after completed attack");
        hit=restore(hit);hit.advanceGameplay(1003,ActorCompletion{});check(hit.state().gameplay.hearts==hp-1 && hit.state().rng.state==attackRng,"impact replayed after restore");
        s=hit.state();s.gameplay.combat=CombatPhase::EnemyAttack;Explorer legacyImpact(s);legacyImpact.advanceGameplay(1004,ActorCompletion{false,true,false});
        check(legacyImpact.state().gameplay.hearts==s.gameplay.hearts,"legacy committed attack applied damage twice");
    }
    // A saved MDES4 exit must follow the new corridor duration after upgrading.
    s=fixture();s.phase=Phase::Home;s.gameplay.homeScene=true;s.gameplay.life=LifePhase::Ready;
    s.gameplay.sceneBeat=SceneBeat::RoomExit;s.gameplay.phaseStartedMs=1000;s.gameplay.deadlineMs=2200;s.gameplay.meals=1;
    s.gameplay.onceKeys={"return:910","meal:910"};
    auto old=encodeSnapshot(s);removeCompanionBlock(old);old.erase(old.end()-12,old.end()-4);old[4]=4;uint32_t crc=0xffffffff;
    for(size_t i=0;i<old.size()-4;++i) {crc^=old[i];for(int bit=0;bit<8;++bit) crc=(crc>>1)^((crc&1)?0xedb88320U:0);}
    crc=~crc;for(unsigned b=0;b<4;++b) old[old.size()-4+b]=uint8_t(crc>>(8*b));
    auto upgraded=decodeSnapshot(old,"survival");check(upgraded.gameplay.deadlineMs==1000+kRoomExitMs,"old exit save skipped the left corridor");
    std::cout<<"sleep exit, berry threshold, escape, pickup turn and friendly swap tests pass\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
