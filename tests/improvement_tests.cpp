#include "core/exploration.hpp"
#include "core/motion.hpp"
#include "core/animator.hpp"
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
    if(argc>1) {auto pkg=loadPackage(argv[1]);Animator a;a.start(pkg,0,0,true,0);a.advance(0);
        auto before=a.advance(200000);a.changeDirection(pkg,0,2);auto after=a.advance(200000);
        check(before.frame==after.frame&&before.nextDeadlineUs==after.nextDeadlineUs&&after.markers.empty(),"direction restarts walking");}
    Explorer e(300,42,"improve",fixtureProfiles(1),{0,0,0,0});auto state=e.state();
    const auto reachable=bfs(state.world,state.world.stairs);int near=-1;
    for(int q=0;q<kTiles;++q) if(reachable.distance[q]==1 && state.world.room[q]==state.world.room[state.world.stairs]) {near=q;break;}
    check(near>=0,"stairs test room");state.position=near;state.visited[near]=1;
    state.population.entities.push_back({1,70,EntityKind::Item,near,false});
    e=Explorer(state);auto move=e.advance();check(move.kind==EventKind::Move && e.state().position==state.world.stairs,"visible stairs lose to item");
    check(e.state().knownStairs==state.world.stairs,"stairs not remembered");
    auto blocked=e.state();blocked.position=near;blocked.visited[near]=1;blocked.target=-1;blocked.goalKind=GoalKind::None;blocked.targetInstance=0;
    Explorer temporary(blocked);temporary.setBlocked(state.world.stairs,true);temporary.advance();check(temporary.state().knownStairs==state.world.stairs,"occupancy erased stairs");
    check(e.advance().kind==EventKind::Pose && e.state().completed,"arrival does not clear");
    auto bytes=encodeSnapshot(e.state());check(bytes[4]==3,"writer not MDES3");auto restored=decodeSnapshot(bytes,"improve");check(restored.discovered==e.state().discovered && restored.knownStairs==state.world.stairs,"discovery restore");
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
    auto migrated=decodeSnapshot(readFile(std::filesystem::path(argv[1]).parent_path().parent_path().parent_path().parent_path().parent_path().parent_path()/"tests/fixtures/legacy-v2.mdes",1048576),"legacy");
    check(migrated.gameplay.actionRound==1,"MDES2 action defaults");
    std::cout<<"improvement tests pass\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;} }
