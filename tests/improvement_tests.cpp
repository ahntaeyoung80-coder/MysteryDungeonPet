#include "core/exploration.hpp"
#include "core/motion.hpp"
#include "core/animator.hpp"
#include "core/gameplay_scene.hpp"
#include <iostream>
#include <stdexcept>
using namespace pet;
void check(bool ok,const char* s) { if(!ok) throw std::runtime_error(s); }
int main(int argc,char** argv) { try {
    World w;w.terrain.fill(1);int p=10*kWidth+10;
    for(unsigned d=0;d<8;++d) {auto [dx,dy]=kDirections[d];int q=p+dx+dy*kWidth;
        check(directionBetween(p,q)==d,"direction numbering");check(canMove(w,p,q),"8-direction step");
        check(bfs(w,p).distance[q]==1,"diagonal not one action");
        Mask occupied{};occupied[q]=1;check(!canMove(w,p,q,occupied),"destination overlap");
        if(dx&&dy) {occupied[p+dx]=1;occupied[q]=0;check(canMove(w,p,q,occupied),"actor mistaken for terrain corner");
            w.terrain[p+dx]=0;check(!canMove(w,p,q)&&!canAttack(w,p,q),"wall corner pierced");w.terrain[p+dx]=1;}
    }
    check(!canMove(w,23,24),"row wrap");
    Motion motion;motion.begin(1,2,{12,12},{36,36},0,1);
    for(auto time:{0LL,17000LL,200000LL,399999LL,400000LL,900000LL}) {
        auto pos=motion.sample(time);double t=std::min(1.0,double(time)/400000);
        check(std::abs(pos.x-(12+24*t))<1e-6,"frame-dependent position");
    }
    check(!motion.active(400000),"duration changed by diagonal");
    motion.begin(1,3,{36,36},{60,36},400000,2);check(motion.sample(400000).x==36,"continuous endpoint");
    motion.pause(500000);auto held=motion.sample(900000);motion.resume(900000);check(motion.sample(900000).x==held.x,"resume jump");check(!motion.active(1200000),"resume duration");
    auto camera=cameraFor({575,575},576,576,200);check(camera.x==376&&camera.y==376,"camera edge clamp");
    check(cameraFor({1,1},576,576,200).x==0,"camera negative edge");
    Motion scaled;scaled.begin(0,1,{300,300},{324,324},0,1);
    for(auto time:{0LL,8333LL,16667LL,99999LL,200000LL,399999LL}) {
        const auto world=scaled.sample(time);const auto screen=raster(world);const auto origin=cameraFor(world,576,576,67);
        check(screen.x-origin.x==33 && screen.y-origin.y==33,"odd viewport makes camera jitter at scale three");
    }
    if(argc>1) {auto pkg=loadPackage(argv[1]);Animator a;a.start(pkg,0,0,true,0);a.advance(0);
        auto before=a.advance(200000);a.changeDirection(pkg,0,2);auto after=a.advance(200000);
        check(pkg.frames(27,0).size()==1,"invented Laying transition frames");
        check(before.frame==after.frame&&before.nextDeadlineUs==after.nextDeadlineUs&&after.markers.empty(),"direction restarts walking");
        for(const auto& actor:{std::filesystem::path(argv[1]),std::filesystem::path(argv[1]).parent_path().parent_path()/"asset-1/asset.mdpa"}) {
            auto selected=loadPackage(actor);Animator walking;walking.start(selected,0,0,true,0);walking.advance(0);
            const auto phase=walking.advance(200000);
            for(unsigned direction=0;direction<8;++direction) {
                walking.changeDirection(selected,0,direction);auto sample=walking.advance(200000);
                check(sample.nextDeadlineUs==phase.nextDeadlineUs && sample.markers.empty(),"actor direction resets walking clock");
            }
        }
    }
    Explorer e(300,42,"improve",fixtureProfiles(1),{0,0,0,0});auto state=e.state();
    const auto reachable=bfs(state.world,state.world.stairs);int near=-1;
    for(int q=0;q<kTiles;++q) if(reachable.distance[q]==1 && state.world.room[q]==state.world.room[state.world.stairs]) {near=q;break;}
    check(near>=0,"stairs test room");state.position=near;state.visited[near]=1;
    state.population.entities.push_back({1,70,EntityKind::Item,near,false});
    e=Explorer(state);auto move=e.advance();check(move.kind==EventKind::Pickup && e.state().position==near,"visible stairs displace item");
    auto complete=e.state();complete.discovered=complete.world.terrain;complete.knownStairs=complete.world.stairs;e=Explorer(complete);
    check(e.advance().kind==EventKind::Move && e.state().position==state.world.stairs,"stairs missing after item/frontier completion");
    check(e.state().knownStairs==state.world.stairs,"stairs not remembered");
    auto blocked=e.state();blocked.position=near;blocked.visited[near]=1;blocked.target=-1;blocked.goalKind=GoalKind::None;blocked.targetInstance=0;
    Explorer temporary(blocked);temporary.setBlocked(state.world.stairs,true);temporary.advance();check(temporary.state().knownStairs==state.world.stairs,"occupancy erased stairs");
    check(e.advance().kind==EventKind::Pose && e.state().completed,"arrival does not clear");
    auto bytes=encodeSnapshot(e.state());check(bytes[4]==6,"writer not MDES6");auto restored=decodeSnapshot(bytes,"improve");check(restored.discovered==e.state().discovered && restored.knownStairs==state.world.stairs,"discovery restore");
    auto turnFixture=[](int player,int enemy,bool playerMoved,bool enemyMoved) {
        Explorer initial(400,42,"turns",fixtureProfiles(1),{0,0,0,0});auto s=initial.state();
        s.world.terrain.fill(0);s.world.room.fill(-1);
        for(int y=2;y<=7;++y) for(int x=2;x<=7;++x) {s.world.terrain[y*kWidth+x]=1;s.world.room[y*kWidth+x]=0;}
        for(int y=2;y<=7;++y) for(int x=10;x<=15;++x) {s.world.terrain[y*kWidth+x]=1;s.world.room[y*kWidth+x]=1;}
        for(int x=7;x<=10;++x) s.world.terrain[4*kWidth+x]=1;
        s.world.centers={4*kWidth+4,4*kWidth+12};s.world.start=s.world.centers[0];s.world.stairs=s.world.centers[1];
        s.position=player;s.visited.fill(0);s.visited[player]=1;s.discovered.fill(0);s.knownStairs=-1;
        s.population.entities={{1,4,EntityKind::Friend,enemy,false}};
        s.gameplay.enabled=true;s.gameplay.hostile={1};s.gameplay.playerActionRound=playerMoved?1:0;
        s.population.entities[0].actionRound=enemyMoved?1:0;return Explorer(s);
    };
    // Player (3,3)->(4,3); stationary enemy (5,3): enemy has its action left.
    auto playerMoved=turnFixture(3*kWidth+4,3*kWidth+5,true,false);
    playerMoved.advanceGameplay(1000);check(playerMoved.state().gameplay.combat==CombatPhase::EnemyTurn,"player move stole enemy attack");
    // Player stationary (3,3); enemy (5,3)->(4,3): enemy already consumed movement.
    auto enemyMoved=turnFixture(3*kWidth+3,3*kWidth+4,false,true);
    enemyMoved.advanceGameplay(1000);check(enemyMoved.state().gameplay.combat==CombatPhase::PlayerTurn,"enemy move grants extra attack");
    // Player (3,3)->(4,3), enemy (6,3)->(5,3): both consumed; next round starts with player.
    auto bothMoved=turnFixture(3*kWidth+4,3*kWidth+5,true,true);
    bothMoved.advanceGameplay(1000);check(bothMoved.state().gameplay.combat==CombatPhase::PlayerTurn&&bothMoved.state().gameplay.actionRound==2,"both moves not consumed");
    auto chase=turnFixture(3*kWidth+3,3*kWidth+6,true,false);
    auto chaseEvent=chase.advanceGameplay(1000);check(chaseEvent.event==GameplayEvent::ActorMove,"enemy did not chase");
    check(chase.state().population.entities[0].tile==2*kWidth+5&&chase.state().population.entities[0].actionRound==1,"chase wrong step/consumption");
    auto unchanged=encodeSnapshot(chase.state());chase.advanceGameplay(1100,true);check(unchanged==encodeSnapshot(chase.state()),"move completion grants attack early");
    auto savedChase=decodeSnapshot(encodeSnapshot(chase.state()),"turns");
    check(savedChase.population.entities[0].movement.from==3*kWidth+6 && savedChase.population.entities[0].movement.to==2*kWidth+5,"saved actor movement endpoints");
    chase.shiftPausedClocks(500,1100);check(chase.state().population.entities[0].movement.startedMs==1500 && chase.state().gameplay.deadlineMs==1900,"pause movement clock drift");
    const auto round=chase.state().gameplay.actionRound;chase.advanceGameplay(1600,true);check(chase.state().gameplay.actionRound==round,"pause grants turn");
    auto lost=turnFixture(3*kWidth+14,3*kWidth+3,true,false);auto lostState=lost.state();
    lostState.population.entities[0].tracking=Tracking::Chase;
    lostState.population.entities[0].lastSeen=lostState.population.entities[0].chaseTarget=3*kWidth+5;
    lost=Explorer(lostState);
    for(unsigned step=0;step<3;++step) {
        auto pending=lost.state();
        if(step) ++pending.gameplay.actionRound;
        pending.gameplay.playerActionRound=pending.gameplay.actionRound;pending.gameplay.deadlineMs=1000+step*1000;
        lost=Explorer(pending);lost.advanceGameplay(1000+step*1000);
    }
    check(lost.state().population.entities[0].tile==3*kWidth+5 &&
        lost.state().population.entities[0].tracking==Tracking::Idle,"lost target orbits last seen position");
    auto stairEnemy=turnFixture(3*kWidth+3,3*kWidth+4,true,false);auto stairState=stairEnemy.state();
    stairState.position=stairState.world.stairs;stairState.visited[stairState.position]=1;
    stairState.population.entities[0].tile=stairState.position-1;stairEnemy=Explorer(stairState);
    stairEnemy.advanceGameplay(1000);check(stairEnemy.state().gameplay.sceneBeat==SceneBeat::FloorFadeOut,"enemy overrides stairs arrival");
    check(stairEnemy.advanceGameplay(2000).event==GameplayEvent::Pose,"stairs fade did not clear last floor");
    auto berry=turnFixture(3*kWidth+3,3*kWidth+6,false,false);auto berryState=berry.state();
    berryState.population.entities={{1,10,EntityKind::Item,3*kWidth+4,false},{2,ITEM_ORAN_BERRY,EntityKind::Item,3*kWidth+6,false}};
    berryState.gameplay.hostile.clear();berryState.gameplay.hearts=1;berryState.discovered=berryState.world.terrain;
    berryState.knownStairs=berryState.world.stairs;berryState.blocked[berryState.world.stairs]=1;berry=Explorer(berryState);
    berry.advanceGameplay(1000);check(berry.state().targetInstance==2 && berry.goal()==GoalKind::Item,"auxiliary berry priority lost when stairs unavailable");
    auto capped=turnFixture(3*kWidth+3,3*kWidth+4,false,false);capped.advanceGameplay(1000);auto healing=capped.state();
    healing.gameplay.hearts=4;healing.gameplay.oran=1;capped=Explorer(healing);
    check(capped.advanceGameplay(1350).event==GameplayEvent::PlayerAttack && capped.state().gameplay.hearts==4 && capped.state().gameplay.oran==1,"berry wasted at one missing HP");
    auto death=turnFixture(3*kWidth+3,3*kWidth+4,false,false);auto ds=death.state();
    check(ds.gameplay.hearts==kMaxHp,"default HP not five");
    ds.gameplay.life=LifePhase::Combat;ds.gameplay.combat=CombatPhase::PlayerFaint;ds.gameplay.hearts=0;ds.gameplay.enemyInstance=1;ds.gameplay.deadlineMs=5000;death=Explorer(ds);
    death.advanceGameplay(1000,ActorCompletion{false,true,true});check(death.state().gameplay.combat==CombatPhase::PlayerFaint,"other actor completed death");
    death.advanceGameplay(1000,ActorCompletion{true,false,false});check(death.state().gameplay.life==LifePhase::Death&&death.state().gameplay.playerHidden,"player Faint not hidden");
    death.advanceGameplay(1750);check(death.state().gameplay.deathStage==DeathStage::Blackout,"death order");
    Surface blackout(200,200);gameplayOverlay(blackout,death.state().gameplay,1750);
    for(size_t i=0;i<blackout.pixels.size();i+=4) check(blackout.pixels[i]==0&&blackout.pixels[i+3]==255,"blackout transparency");
    death.advanceGameplay(2750);check(death.state().gameplay.fadeInOnly&&death.state().gameplay.homeScene,"blackout target swap");
    ds.gameplay.combat=CombatPhase::Defeat;ds.gameplay.deadlineMs=1000;
    Explorer oldDefeat(ds);
    check(oldDefeat.advanceGameplay(1000).event==GameplayEvent::PlayerFaint &&
        oldDefeat.state().gameplay.hearts==0 && oldDefeat.state().gameplay.onceKeys.empty(),"legacy defeat skips Faint or replays damage");
    check(deadlineFromUtc(1000000,1500,1499)==1000000,"expired deadline underflows");
    check(deadlineFromUtc(1000000,1500,1900)==1400000,"movement deadline drift");
    check(deadlineFromUtc(0,1,UINT64_MAX)==60000000,"unbounded UTC timer overflow");
    const auto fixtureRoot=std::filesystem::path(argv[1]).parent_path().parent_path().parent_path().parent_path().parent_path().parent_path()/"tests/fixtures";
    const auto oldCompleted=decodeSnapshot(readFile(fixtureRoot/"legacy-v2-completed-npc.mdes",1048576),"legacy");
    const auto absent=std::find_if(oldCompleted.population.entities.begin(),oldCompleted.population.entities.end(),[](const auto& actor){return actor.kind==EntityKind::Friend && actor.done;});
    check(absent!=oldCompleted.population.entities.end() && !absent->visible && absent->tile==oldCompleted.position,"old completed NPC blocks saved player tile");
    auto upgradedNpc=decodeSnapshot(encodeSnapshot(oldCompleted),"legacy");check(!upgradedNpc.population.entities[absent->instanceId-1].visible,"NPC migration not durable");
    for(const auto& name:{"legacy-v1-contact.mdes","legacy-v2-contact.mdes"}) {
        const auto oldContact=decodeSnapshot(readFile(fixtureRoot/name,1048576),"legacy");
        for(const auto& actor:oldContact.population.entities) if(actor.kind==EntityKind::Friend) check(actor.tile!=oldContact.position && actor.visible && !actor.done,"legacy unprocessed contact overlaps player or completes dialogue");
        check(oldContact.onceKeys.empty(),"contact migration created an effect");
    }
    auto migrated=decodeSnapshot(readFile(std::filesystem::path(argv[1]).parent_path().parent_path().parent_path().parent_path().parent_path().parent_path()/"tests/fixtures/legacy-v2.mdes",1048576),"legacy");
    check(migrated.gameplay.actionRound==1,"MDES2 action defaults");
    if(argc>1) {
        const auto props=loadPackage(std::filesystem::path(argv[1]).parent_path().parent_path().parent_path()/"props/props.mdpa");
        check(props.frames(0,0)[0].rect.w==16 && props.frames(1,0)[0].rect.w==24 && props.frames(3,0)[0].rect.h==8,"original prop dimensions");
    }
    Surface hearts(200,200);GameplayState hp;gameplayOverlay(hearts,hp,0);
    unsigned red=0;for(size_t i=0;i<hearts.pixels.size();i+=4) if(hearts.pixels[i+2]==230) ++red;
    check(red==5*4*27,"heart scale/count");
    std::cout<<"improvement tests pass\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;} }
