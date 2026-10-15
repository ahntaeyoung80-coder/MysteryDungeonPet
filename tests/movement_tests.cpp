#include "core/exploration_package.hpp"
#include "core/motion.hpp"
#include "core/animator.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>

using namespace pet;
namespace {
void check(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
ExplorationState route(int from,int to,const std::string& version,const std::vector<FloorProfile>& floors,bool straight=false) {
    Explorer seed(20261015,42,version,floors,{0,0,0,0});auto s=seed.state();
    s.world.terrain.fill(0);s.world.room.fill(-1);
    for(int y=1;y<23;++y) for(int x=1;x<23;++x) {
        if(!straight || y==from/kWidth) {s.world.terrain[y*kWidth+x]=1;s.world.room[y*kWidth+x]=0;}
    }
    s.world.start=from;s.world.stairs=to;s.world.centers={from,to};s.world.room[to]=1;
    s.position=from;s.visited.fill(0);s.visited[from]=1;s.discovered=s.world.terrain;
    s.knownStairs=to;s.population={};s.target=-1;s.goalKind=GoalKind::None;s.targetInstance=0;
    s.gameplay.enabled=true;validateState(s);return s;
}
void fixtures(const std::filesystem::path& packagePath,const std::filesystem::path& output) {
    const auto p=loadExplorationPackage(packagePath);std::filesystem::create_directories(output);
    auto write=[&](const std::string& name,ExplorationState s) {saveSnapshot(output/(name+".mdes"),s);};
    write("straight",route(10*kWidth+3,10*kWidth+21,p.assetVersion,p.floors,true));
    write("turn",route(4*kWidth+4,13*kWidth+21,p.assetVersion,p.floors));
    const std::uint64_t utc=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    const std::vector<std::pair<std::string,int>> edges={{"top-left",25},{"top",24+12},{"top-right",24+22},
        {"left",12*24+1},{"right",12*24+22},{"bottom-left",22*24+1},{"bottom",22*24+12},{"bottom-right",22*24+22}};
    for(const auto& [name,tile]:edges) {
        auto s=route(tile,10*kWidth+10,p.assetVersion,p.floors);
        s.gameplay.phaseStartedMs=utc;s.gameplay.deadlineMs=utc+60000;write(name,s);
    }
    std::cout<<"fixed MDES5 routes and eight stationary edge snapshots written\n";
}
}
int main(int argc,char** argv) {try {
    if(argc==4 && std::string(argv[1])=="--fixtures") {fixtures(argv[2],argv[3]);return 0;}
    if(argc==4 && std::string(argv[1])=="--inspect") {
        const auto package=loadExplorationPackage(argv[2]);const auto s=loadSnapshot(argv[3],package.assetVersion);const auto& g=s.gameplay;
        const auto utc=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        std::cout<<"{\"runId\":"<<s.runId<<",\"inventory\":"<<s.inventory.size()<<",\"onceCount\":"<<s.onceKeys.size()
            <<",\"gameplayOnceCount\":"<<g.onceKeys.size()<<",\"day\":"<<g.day<<",\"hearts\":"<<g.hearts
            <<",\"actionId\":"<<g.movement.actionId<<",\"startedMs\":"<<g.movement.startedMs<<",\"durationMs\":"<<g.movement.durationMs
            <<",\"from\":"<<g.movement.from<<",\"to\":"<<g.movement.to<<",\"nowMs\":"<<utc<<"}\n";return 0;
    }
    check(argc==2,"expected original actor package");
    const auto package=loadPackage(argv[1]);
    // Compare every direction at the same nonzero clock to an uninterrupted Walk.
    // Direction changes must neither replay markers nor reset frame deadlines.
    Animator walking;walking.start(package,0,0,true,1000000);walking.advance(1000000);
    for(int64_t time=1016667;time<3000000;time+=16667) {
        const auto before=walking.advance(time);
        for(unsigned direction=0;direction<8;++direction) {
            walking.changeDirection(package,0,direction);const auto after=walking.advance(time);
            check(after.frame==before.frame && after.nextDeadlineUs==before.nextDeadlineUs && after.markers.empty(),"direction changed walking phase");
        }
    }
    unsigned actions=0;
    for(unsigned direction=0;direction<8;++direction) {
        const auto [dx,dy]=kDirections[direction];const int start=12*kWidth+12,target=start+8*(dx+dy*kWidth);
        Explorer e(route(start,target,"movement",fixtureProfiles(1)));
        int64_t monotonic=1000000;uint64_t utc=1000;Motion motion;motion.reset(tilePoint(start));
        for(unsigned step=0;step<8;++step) {
            const int old=e.state().position;const auto walkingStart=e.state().gameplay.walkStartedMs;
            const auto event=e.advanceGameplay(utc);check(event.event==GameplayEvent::Move,"plain route inserted wait/interaction");
            const auto& g=e.state().gameplay;
            check(g.deadlineMs==utc+400 && g.movement.durationMs==400,"normal step not 400ms");
            check(g.movement.from==old && g.movement.to==e.state().position,"movement endpoints differ from committed action");
            check(!step || g.walkStartedMs==walkingStart,"successive steps reset walking phase");
            motion.begin(0,g.movement.actionId,tilePoint(old),tilePoint(e.state().position),monotonic,g.playerDirection);
            const auto committed=encodeSnapshot(e.state());
            for(uint64_t offset:{1U,17U,199U,399U}) {
                e.advanceGameplay(utc+offset);check(encodeSnapshot(e.state())==committed,"movement tick consumed another action");
                Explorer resumed(decodeSnapshot(committed,"movement"));
                const auto& saved=resumed.state().gameplay.movement;Motion restored;
                restored.begin(0,saved.actionId,tilePoint(saved.from),tilePoint(saved.to),monotonic,saved.direction);
                check(raster(restored.sample(monotonic+int64_t(offset)*1000)).x==raster(motion.sample(monotonic+int64_t(offset)*1000)).x &&
                      raster(restored.sample(monotonic+int64_t(offset)*1000)).y==raster(motion.sample(monotonic+int64_t(offset)*1000)).y,"saved movement changed mid-frame position");
            }
            const auto end=raster(motion.sample(monotonic+400000));const auto expected=raster(tilePoint(e.state().position));
            check(end.x==expected.x && end.y==expected.y && !motion.active(monotonic+400000),"step missed endpoint");
            motion.pause(monotonic+200000);const auto held=raster(motion.sample(monotonic+200000));
            check(raster(motion.sample(monotonic+900000)).x==held.x && raster(motion.sample(monotonic+900000)).y==held.y,"paused step moved");
            motion.resume(monotonic+900000);check(!motion.active(monotonic+1100000),"resume changed remaining duration");
            e=Explorer(decodeSnapshot(committed,"movement"));utc+=400;monotonic+=400000;++actions;
        }
        check(e.state().position==target,"eight-step route missed destination");
    }
    // A route with unequal axes must turn without adding a non-movement action.
    Explorer turn(route(4*kWidth+4,13*kWidth+21,"movement",fixtureProfiles(1)));
    unsigned previous=8,changes=0;uint64_t time=1000;
    for(unsigned step=0;step<17;++step,time+=400) {
        check(turn.advanceGameplay(time).event==GameplayEvent::Move,"turning route inserted idle action");
        const auto direction=turn.state().gameplay.playerDirection;
        changes+=previous<8&&direction!=previous;previous=direction;
    }
    check(changes>0 && turn.state().position==turn.state().world.stairs,"turn scenario did not cover a turn");
    std::cout<<"movement pass: "<<actions<<" actions, 8 directions, 400ms, phase, turn, pause and MDES5 restore\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
