#pragma once
#include "core/exploration_package.hpp"
using namespace pet;
inline ExplorationState eventFixture(const ExplorationPackage& p) {
    Explorer e(1300,41,p.assetVersion,p.floors,{0,0,0,0});auto s=e.state();
    s.world.terrain.fill(0);s.world.room.fill(-1);
    for(int y=2;y<10;++y)for(int x=2;x<10;++x){s.world.terrain[y*24+x]=1;s.world.room[y*24+x]=0;}
    for(int y=2;y<10;++y)for(int x=12;x<20;++x){s.world.terrain[y*24+x]=1;s.world.room[y*24+x]=1;}
    for(int x=9;x<=12;++x)s.world.terrain[4*24+x]=1;
    s.world.centers={100,111};s.world.start=100;s.world.stairs=111;s.position=100;s.knownStairs=111;
    s.visited.fill(0);s.visited[100]=1;s.discovered=s.world.terrain;
    s.population.entities={{1,237,EntityKind::Friend,101,false}};s.target=-1;s.targetInstance=0;s.goalKind=GoalKind::None;
    auto& g=s.gameplay;g.enabled=true;g.companion.enabled=true;g.companion.tile=99;g.phaseStartedMs=g.deadlineMs=1000;
    s.spawnPolicy.friendBudget=4;
    return s;
}
