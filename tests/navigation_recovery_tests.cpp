#include "core/exploration.hpp"
#include "legacy_snapshot.hpp"
#include "core/exploration_package.hpp"
#include <iostream>
#include <stdexcept>
#include <map>
#include <set>
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
int main(int argc,char** argv){try{
 if(argc==4 && std::string(argv[1])=="--fixtures"){
  const auto p=loadExplorationPackage(argv[2]);const std::filesystem::path output=argv[3];std::filesystem::create_directories(output);
  auto s=cycleFixture();s.assetVersion=p.assetVersion;s.floors=p.floors;
  saveSnapshot(output/"cycle.mdes",s);saveSnapshot(output/"manual.mdes",s);
  Explorer observed(s);observed.advanceGameplay(1000);s.gameplay.navigationHistory=observed.state().gameplay.navigationHistory;
  saveSnapshot(output/"watch.mdes",s);return 0;
 }
 if(argc==4 && std::string(argv[1])=="--inspect"){
  const auto p=loadExplorationPackage(argv[2]);const auto s=loadSnapshot(argv[3],p.assetVersion);const auto& g=s.gameplay;
  std::cout<<"{\"runId\":"<<s.runId<<",\"life\":\""<<lifeName(g.life)<<"\",\"day\":"<<g.day<<",\"hearts\":"<<g.hearts
   <<",\"floor\":"<<s.floorIndex<<",\"position\":"<<s.position
   <<",\"inventory\":"<<s.inventory.size()<<",\"completed\":"<<(s.completed?"true":"false")<<",\"meals\":"<<g.meals
   <<",\"returnCount\":"<<g.onceKeys.count("return:"+std::to_string(s.runId))
   <<",\"manualCount\":"<<g.onceKeys.count("manual-reset:"+std::to_string(s.runId))
   <<",\"autoCount\":"<<g.onceKeys.count("navigation-reset:"+std::to_string(s.runId))
   <<",\"movement\":"<<g.movement.actionId<<",\"history\":"<<g.navigationHistory.size()<<"}\n";return 0;
 }
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
 Explorer stuck(saved);check(stuck.advanceGameplay(1000).event==GameplayEvent::Move,"recurring joint state did not move within the dungeon");
 check(stuck.state().position!=observed.state().position && stuck.state().phase==Phase::Explore &&
       stuck.state().floorIndex==saved.floorIndex && stuck.state().runId==saved.runId &&
       stuck.state().gameplay.hearts==2 && stuck.state().inventory.empty() && !stuck.state().completed &&
       !stuck.state().gameplay.onceKeys.count("return:900"),"detour repeated the old direction, returned home or changed committed progress");
 std::cout<<"persisted repeat: from="<<saved.position<<" ordinary="<<observed.state().position<<" detour="<<stuck.state().position
  <<" run="<<stuck.state().runId<<" floor="<<stuck.state().floorIndex<<" (no return)\n";
 recurring.visited[75]=1;Explorer progress(recurring);
 check(!progress.advanceGameplay(1000).navigationDetour,"new exploration progress was mistaken for a cycle");
 auto signatureFor=[](ExplorationState s){
  s.gameplay.navigationHistory.clear();Explorer sample(s);sample.advanceGameplay(1000);
  check(!sample.state().gameplay.navigationHistory.empty(),"decision state was not observed");
  return sample.state().gameplay.navigationHistory.front();
 };
 // Revisit the same decision with persisted history to exercise full rotations
 // at both health levels, including the wraparound of the bounded history.
 for(unsigned hp:{2U,5U}) {
  auto s=cycleFixture();s.population.entities.clear();s.gameplay.hostile.clear();s.gameplay.hearts=hp;
  s.gameplay.movement={1,600,400,directionBetween(78,77),78,77};
  const auto signature=signatureFor(s);s.gameplay.navigationHistory={signature};
  std::set<int> alternatives;int last=-1;
  auto withoutHistory=s;withoutHistory.gameplay.navigationHistory.clear();Explorer ordinary(withoutHistory);ordinary.advanceGameplay(1000);
  for(unsigned visit=0;visit<16;++visit) {
   Explorer restored(decodeSnapshot(encodeSnapshot(s),"oscillation"));const auto event=restored.advanceGameplay(1000);
   const auto& after=restored.state();
   check(event.event==GameplayEvent::Move && event.navigationDetour && after.position!=ordinary.state().position && after.position!=78 &&
    after.position!=last && after.phase==Phase::Explore && after.floorIndex==s.floorIndex && after.runId==s.runId &&
    after.gameplay.hearts==hp && after.gameplay.actionRound==s.gameplay.actionRound+1 &&
    after.gameplay.playerActionRound==after.gameplay.actionRound && after.gameplay.movement.durationMs==400 &&
    !after.gameplay.onceKeys.count("return:900"),"persisted detour repeated direction, violated turn/movement or returned home");
   check(canMove(s.world,s.position,after.position,s.blocked),"detour crossed a wall/corner/blocker");
   alternatives.insert(after.position);last=after.position;
   s.gameplay.navigationHistory=after.gameplay.navigationHistory;
  }
  check(alternatives.size()>=4,"repeated decisions did not rotate alternate directions");
 }
 // Only one exit: use it. No exit: wait for the next actor turn, never return.
 for(int exit:{-1,78}) {
  auto s=cycleFixture();s.population.entities.clear();s.gameplay.hostile.clear();
  s.target=s.world.stairs;s.goalKind=GoalKind::Stairs;
  s.gameplay.movement={1,600,400,directionBetween(78,77),78,77};
  for(auto [dx,dy]:kDirections){int tile=s.position+dy*24+dx;if(tile!=exit)s.blocked[tile]=1;}
  s.gameplay.navigationHistory={signatureFor(s)};
  Explorer enclosed(decodeSnapshot(encodeSnapshot(s),"oscillation"));const auto event=enclosed.advanceGameplay(1000);
  check(event.navigationDetour && event.event==(exit<0?GameplayEvent::Wait:GameplayEvent::Move) &&
   enclosed.state().position==(exit<0?s.position:exit) && enclosed.state().phase==Phase::Explore &&
   !enclosed.state().gameplay.onceKeys.count("return:900"),"blocked detour teleported, hung or returned home");
  if(exit<0) for(unsigned tick=1;tick<8;++tick) {
   enclosed=Explorer(decodeSnapshot(encodeSnapshot(enclosed.state()),"oscillation"));
   check(enclosed.advanceGameplay(1000+tick*400).event==GameplayEvent::Wait && enclosed.state().phase==Phase::Explore,
    "blocked restored detour fell back to automatic return");
  }
 }
 auto corner=cycleFixture();corner.population.entities.clear();corner.gameplay.hostile.clear();
 corner.world.terrain[78]=0;corner.world.room[78]=-1;corner.discovered[78]=0;
 corner.gameplay.navigationHistory={signatureFor(corner)};Explorer wall(corner);
 check(wall.advanceGameplay(1000).event==GameplayEvent::Move &&
  canMove(corner.world,corner.position,wall.state().position) && wall.state().position!=54 && wall.state().position!=102,
  "detour cut a blocked diagonal corner");
 auto attacked=cycleFixture();attacked.population.entities[0].tile=78;
 attacked.gameplay.navigationHistory={signatureFor(attacked)};Explorer attackFirst(attacked);
 check(attackFirst.advanceGameplay(1000).event==GameplayEvent::HostileEncounter &&
  attackFirst.state().gameplay.life==LifePhase::Combat && attackFirst.state().position==attacked.position,
  "detour skipped an adjacent enemy's attack right");
 // MDES5 migration has no history; MDES1..4 retain their existing fixtures/checks.
 auto old=encodeSnapshot(cycleFixture());removeCompanionBlock(old);old.erase(old.end()-8,old.end()-4);old[4]=5;
 uint32_t crc=0xffffffff;
 for(size_t i=0;i<old.size()-4;++i){crc^=old[i];for(unsigned bit=0;bit<8;++bit)crc=(crc>>1)^((crc&1)?0xedb88320U:0);}
 crc=~crc;for(unsigned b=0;b<4;++b)old[old.size()-4+b]=uint8_t(crc>>(8*b));
 check(decodeSnapshot(old,"oscillation").gameplay.navigationHistory.empty(),"MDES5 did not initialize empty watch");
 for(auto phase:{LifePhase::Explore,LifePhase::Combat,LifePhase::Dialogue}){
  auto s=cycleFixture();s.population.entities.push_back({2,10,EntityKind::Item,s.position,true});
  const std::string pickup="pickup:900:0:2";s.onceKeys.insert(pickup);s.inventory.push_back({pickup,10});
  s.gameplay.life=phase;s.gameplay.enemyInstance=1;
  if(phase==LifePhase::Combat){s.gameplay.combat=CombatPhase::EnemyAttack;s.gameplay.damage=1;s.gameplay.hit=true;
   s.gameplay.onceKeys.insert("pending-hit:900:0:1:1");}
  Explorer manual(s);check(manual.canResetExploration(),"active dungeon reset unavailable");
  check(manual.resetExploration(1000).event==GameplayEvent::ReturnHome,"manual reset did not return home");
  const auto& after=manual.state();
  check(after.inventory.size()==1 && after.onceKeys.count(pickup) && !after.completed && after.gameplay.hearts==2 &&
   after.gameplay.onceKeys.count("manual-reset:900") && after.gameplay.onceKeys.count("return:900") &&
   !after.gameplay.onceKeys.count("pending-hit:900:0:1:1") && after.gameplay.movement.actionId==0,"reset lost committed progress or replayed pending damage");
  manual=Explorer(decodeSnapshot(encodeSnapshot(after),"oscillation"));const auto durable=encodeSnapshot(manual.state());
  check(!manual.canResetExploration() && manual.resetExploration(1001).event==GameplayEvent::None && encodeSnapshot(manual.state())==durable,"restored reset executed twice");
  for(unsigned tick=0;tick<200 && manual.state().gameplay.day==1;++tick)manual.advanceGameplay(1000+tick*400,true);
  check(manual.state().gameplay.day==2 && manual.state().inventory.size()==1 && manual.state().gameplay.meals==1,"reset failed to reach next day exactly once");
 }
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
  if(!finished || sample.state().gameplay.onceKeys.count("return:900")) {
   const auto& stuck=sample.state();
   std::cerr<<"low HP geometry run="<<run<<" initialP="<<s.position<<" initialE="<<enemy.tile
    <<" currentP="<<stuck.position<<" currentE="<<stuck.population.entities[0].tile
    <<" target="<<stuck.target<<" stairsFailed="<<unsigned(stuck.failedTargets[stuck.world.stairs])
    <<" steps="<<stuck.floorSteps<<" life="<<lifeName(stuck.gameplay.life)<<'\n';
  }
  check(finished,"low HP chase/retreat exceeded bounded liveness sweep");
  recovered+=sample.state().gameplay.onceKeys.count("return:900")!=0;
 }
 check(recovered==0,"low HP sweep ended exploration through automatic return");
 std::cout<<"200 restored low-HP geometries escaped without automatic return\n";
 std::cout<<"restored detour rotations at HP2/HP5, dead-end waits, single exit, corner and enemy-turn checks pass\n";
 std::cout<<"navigation recovery regression pass\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
