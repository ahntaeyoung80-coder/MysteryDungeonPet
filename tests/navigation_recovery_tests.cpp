#include "core/exploration.hpp"
#include <iostream>
#include <stdexcept>
#include <map>
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
 // Persist a genuinely observed signature, then revisit that exact joint state.
 Explorer observed(cycleFixture());observed.advanceGameplay(1000);
 auto recurring=cycleFixture();recurring.gameplay.navigationHistory=observed.state().gameplay.navigationHistory;
 check(!recurring.gameplay.navigationHistory.empty(),"cycle watch did not record a joint state");
 auto saved=decodeSnapshot(encodeSnapshot(recurring),"oscillation");
 check(saved.gameplay.navigationHistory==recurring.gameplay.navigationHistory,"restore erased cycle watch");
 Explorer stuck(saved);check(stuck.advanceGameplay(1000).event==GameplayEvent::ReturnHome,"recurring joint state did not auto-reset");
 check(stuck.state().gameplay.onceKeys.count("navigation-reset:900") && stuck.state().phase==Phase::Home &&
       stuck.state().gameplay.hearts==2 && stuck.state().inventory.empty() && !stuck.state().completed,"automatic recovery changed health/rewards or granted clear");
 const auto committed=encodeSnapshot(stuck.state());stuck.resetExploration(1001);check(encodeSnapshot(stuck.state())==committed,"reset repeated the return transaction");
 recurring.visited[75]=1;Explorer progress(recurring);
 check(progress.advanceGameplay(1000).event!=GameplayEvent::ReturnHome,"new exploration progress was mistaken for a cycle");
 // MDES5 migration has no history; MDES1..4 retain their existing fixtures/checks.
 auto old=encodeSnapshot(cycleFixture());old.erase(old.end()-8,old.end()-4);old[4]=5;
 uint32_t crc=0xffffffff;
 for(size_t i=0;i<old.size()-4;++i){crc^=old[i];for(unsigned bit=0;bit<8;++bit)crc=(crc>>1)^((crc&1)?0xedb88320U:0);}
 crc=~crc;for(unsigned b=0;b<4;++b)old[old.size()-4+b]=uint8_t(crc>>(8*b));
 check(decodeSnapshot(old,"oscillation").gameplay.navigationHistory.empty(),"MDES5 did not initialize empty watch");
 auto invalid=cycleFixture();invalid.gameplay.navigationHistory.resize(33);bool rejected=false;
 try{encodeSnapshot(invalid);}catch(const std::exception&){rejected=true;}check(rejected,"unbounded watch accepted");
 // Sweep relative player/enemy geometry at low HP; restore after every action.
 Rng random{20261015};unsigned recovered=0;
 for(unsigned run=0;run<200;++run){
  auto s=cycleFixture();s.position=(2+random.bounded(11))*24+2+random.bounded(11);
  auto& enemy=s.population.entities[0];
  do{enemy.tile=(2+random.bounded(11))*24+2+random.bounded(11);}while(enemy.tile==s.position);
  if(s.position==50 || enemy.tile==50){--run;continue;}
  s.visited.fill(0);s.visited[s.position]=1;s.gameplay.hearts=1+run%2;
  Explorer sample(s);bool finished=false;
  for(unsigned tick=0;tick<128;++tick){
   sample.advanceGameplay(1000+tick*400,true);
   if(sample.state().gameplay.life!=LifePhase::Explore || sample.state().gameplay.sceneBeat!=SceneBeat::None){finished=true;break;}
   sample=Explorer(decodeSnapshot(encodeSnapshot(sample.state()),"oscillation"));
  }
  check(finished,"low HP chase/retreat exceeded bounded liveness sweep");
  recovered+=sample.state().gameplay.onceKeys.count("navigation-reset:900")!=0;
 }
 std::cout<<"200 restored low-HP geometries escaped; automatic recoveries="<<recovered<<"\n";
 std::cout<<"navigation recovery regression pass\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
