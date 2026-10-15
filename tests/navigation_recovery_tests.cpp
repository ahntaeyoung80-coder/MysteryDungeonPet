#include "core/exploration.hpp"
#include <iostream>
#include <stdexcept>
using namespace pet;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
ExplorationState cycleFixture(){
 Explorer initial(900,42,"oscillation",fixtureProfiles(1),{0,0,0,0});auto s=initial.state();
 s.world.terrain.fill(0);s.world.room.fill(-1);
 for(int y=2;y<=12;++y)for(int x=2;x<=12;++x){s.world.terrain[y*24+x]=1;s.world.room[y*24+x]=0;}
 s.world.start=75;s.world.stairs=50;s.world.room[50]=1;s.world.centers={75,50};
 s.position=77;s.visited.fill(0);s.visited[77]=1;s.discovered=s.world.terrain;s.knownStairs=50;
 s.population.entities={{1,4,EntityKind::Friend,99,false}};
 s.gameplay.enabled=true;s.gameplay.hostile={1};s.gameplay.hearts=2;s.gameplay.playerActionRound=s.gameplay.actionRound;
 return s;
}
int main(){try{
 Explorer e(cycleFixture());unsigned repeated=0;bool escaped=false;
 for(unsigned tick=0;tick<24;++tick){
  e.advanceGameplay(1000+tick*400,false);const auto& s=e.state();
  std::cout<<tick<<" player="<<s.position<<" enemy="<<s.population.entities[0].tile<<" life="<<lifeName(s.gameplay.life)<<"\n";
  if(tick>=4&&(s.position==102||s.position==125))++repeated;
  if(s.gameplay.life!=LifePhase::Explore||s.gameplay.sceneBeat!=SceneBeat::None||s.position==s.world.stairs){escaped=true;break;}
 }
 check(escaped||repeated<4,"player/enemy remain in the alternating diagonal retreat cycle");
 std::cout<<"navigation recovery regression pass\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
