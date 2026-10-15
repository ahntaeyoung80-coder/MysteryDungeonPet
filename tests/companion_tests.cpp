#include "core/exploration.hpp"
#include "legacy_snapshot.hpp"
#include <iostream>
#include <stdexcept>
using namespace pet;
void check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
ExplorationState companionFixture(){
 Explorer first(940,42,"companion",fixtureProfiles(3),{0,0,0,0});auto s=first.state();
 s.world.terrain.fill(0);s.world.room.fill(-1);
 for(int y=2;y<=10;++y)for(int x=2;x<=10;++x){s.world.terrain[y*24+x]=1;s.world.room[y*24+x]=0;}
 for(int y=2;y<=10;++y)for(int x=14;x<=20;++x){s.world.terrain[y*24+x]=1;s.world.room[y*24+x]=1;}
 for(int x=10;x<=14;++x)s.world.terrain[6*24+x]=1;
 s.world.centers={75,161};s.world.start=75;s.world.stairs=161;s.position=125;
 s.visited.fill(0);s.visited[s.position]=1;s.discovered=s.world.terrain;s.knownStairs=s.world.stairs;
 s.gameplay.enabled=true;return s;
}
void crc(std::vector<uint8_t>& bytes){uint32_t value=0xffffffff;
 for(size_t i=0;i<bytes.size()-4;++i){value^=bytes[i];for(int bit=0;bit<8;++bit)value=(value>>1)^((value&1)?0xedb88320U:0);}
 value=~value;for(unsigned bit=0;bit<4;++bit)bytes[bytes.size()-4+bit]=uint8_t(value>>(8*bit));
}
Explorer partyEncounter(int enemy=123) {
 auto s=companionFixture();auto& g=s.gameplay;
 g.actionRound=1;g.playerActionRound=1;g.companion.enabled=true;g.companion.tile=124;g.companion.actionRound=1;
 s.population.entities={{1,4,EntityKind::Friend,enemy,false}};g.hostile={1};return Explorer(s);
}
void forceHit(Explorer& e,bool player) {
 auto s=e.state();for(uint64_t seed=0;seed<10000;++seed){Rng rng{seed};auto roll=rollBasicAttack(rng,player);
 if(roll.hit && !roll.critical){s.rng.state=seed;e=Explorer(s);return;}}throw std::runtime_error("no hit seed");
}
GameplayUpdate restoredStep(Explorer& e,uint64_t now,ActorCompletion done={true,true,true}) {
 auto copy=Explorer(decodeSnapshot(encodeSnapshot(e.state()),"companion"));auto expected=e.advanceGameplay(now,done),actual=copy.advanceGameplay(now,done);
 check(expected.event==actual.event && encodeSnapshot(e.state())==encodeSnapshot(copy.state()),"party combat replay differs");return expected;
}
void combatCases(){
 {auto e=partyEncounter();restoredStep(e,1000);check(e.state().gameplay.combat==CombatPhase::EnemyTurn && e.state().gameplay.enemyTargetsCompanion,"enemy ignored closer companion");
  forceHit(e,false);check(restoredStep(e,1350).event==GameplayEvent::EnemyAttack,"enemy attack absent");
  restoredStep(e,1351,{true,false,true});check(e.state().gameplay.companion.hearts==5,"non-owner completed enemy impact");
  restoredStep(e,1352);check(e.state().gameplay.companion.hearts==4 && e.state().gameplay.hearts==5 && e.state().gameplay.combat==CombatPhase::CompanionHurt,"enemy impact applied to wrong target");
  restoredStep(e,1353,{true,true,false});check(e.state().gameplay.combat==CombatPhase::CompanionHurt && e.state().gameplay.companion.hearts==4,"partner hurt ownership/duplicate damage failed");
 }
 {auto e=partyEncounter(126);auto s=e.state();s.gameplay.companion.tile=150;e=Explorer(s);restoredStep(e,1000);check(!e.state().gameplay.enemyTargetsCompanion,"equal distance did not select leader");}
 for(bool corridor:{false,true}) {
  auto e=partyEncounter(126);auto s=e.state();auto& g=s.gameplay;g.life=LifePhase::Combat;g.combat=CombatPhase::CompanionTurn;g.enemyInstance=1;g.enemyHits=2;g.companion.actionRound=0;
  // Keep valid room metadata; only the occupied corridor tiles need corridor IDs.
  if(corridor){s.world.room=companionFixture().world.room;s.world.room[124]=s.world.room[125]=-1;}
  e=Explorer(s);auto u=restoredStep(e,1000);
  check(e.state().gameplay.companion.actionRound==1,"support wait/move did not spend action");
  if(corridor)check(e.state().gameplay.companion.tile==124 && u.event!=GameplayEvent::CompanionMove,"companion repositioned in corridor combat");
  else {check(u.event==GameplayEvent::CompanionMove && canAttack(e.state().world,e.state().gameplay.companion.tile,126),"room support step absent");
   check(e.state().gameplay.enemyHits==2 && e.state().gameplay.deadlineMs==1400,"support step included bonus attack");restoredStep(e,1400);}
 }
 {auto e=partyEncounter(126);auto s=e.state();auto& g=s.gameplay;g.companion.tile=150;g.life=LifePhase::Combat;g.combat=CombatPhase::CompanionTurn;g.enemyInstance=1;g.enemyHits=2;g.companion.actionRound=0;e=Explorer(s);forceHit(e,true);
  check(restoredStep(e,1000).event==GameplayEvent::CompanionAttack && e.state().gameplay.enemyHits==1,"companion attack stats differ from player");
  restoredStep(e,1001,{true,true,false});check(e.state().gameplay.combat==CombatPhase::CompanionAttack,"player finished companion attack");restoredStep(e,1002);check(e.state().gameplay.combat==CombatPhase::EnemyHurt,"companion hit lacks enemy hurt");
 }
 {auto e=partyEncounter();auto s=e.state();s.gameplay.companion.hearts=3;s.gameplay.companion.oran=1;s.gameplay.companion.actionRound=0;e=Explorer(s);
  auto u=restoredStep(e,1000);check(u.event==GameplayEvent::UseOranBerry && u.actorId==kCompanionActorId && e.state().gameplay.companion.hearts==5 && e.state().gameplay.hearts==5,"companion berry differs from player");}
 {auto e=partyEncounter();auto s=e.state();s.gameplay.companion.hearts=1;e=Explorer(s);restoredStep(e,1000);forceHit(e,false);restoredStep(e,1350);restoredStep(e,1351);restoredStep(e,1352);
  check(e.state().gameplay.combat==CombatPhase::CompanionFaint,"companion faint missing");restoredStep(e,1353,{true,true,false});check(e.state().gameplay.life==LifePhase::Combat,"faint skipped companion animation");
  restoredStep(e,1354);check(e.state().gameplay.life==LifePhase::Death && e.state().gameplay.companion.hidden,"party defeat absent");
  for(unsigned i=0;i<50 && e.state().gameplay.day==1;++i)restoredStep(e,e.state().gameplay.deadlineMs);
  check(e.state().gameplay.day==2 && e.state().gameplay.hearts==5 && e.state().gameplay.companion.hearts==5 && !e.state().gameplay.companion.hidden,"party next day recovery failed");
 }
}
int main(){try{
 combatCases();
 for(unsigned dir=0;dir<8;++dir)for(bool reverse:{false,true}){
  auto s=companionFixture();auto [dx,dy]=kDirections[dir];const int next=s.position+dy*24+dx;
  s.gameplay.companion.enabled=true;s.gameplay.companion.tile=reverse?next:s.position-dy*24-dx;
  s.population.entities={{1,10,EntityKind::Item,next,false}};Explorer e(s);
  check(e.advanceGameplay(1000).event==GameplayEvent::Move,"companion blocked a legal leader move");
  const auto& after=e.state();const auto& p=after.gameplay.companion;
  check(after.position==next && p.tile==s.position && canAttack(after.world,after.position,p.tile),"companion did not follow/swap into previous leader tile");
  check(p.movement.from==s.gameplay.companion.tile && p.movement.to==p.tile && p.movement.durationMs==400 && p.movement.startedMs==1000,"companion movement clocks differ from leader");
  auto bytes=encodeSnapshot(after);Explorer restored(decodeSnapshot(bytes,"companion"));restored.advanceGameplay(1199,true);
  check(encodeSnapshot(restored.state())==bytes,"restore repeated partial companion motion");
  restored.shiftPausedClocks(500,1200);check(restored.state().gameplay.companion.movement.startedMs==1500,"pause lost companion motion clock");
 }
 for(bool greeted:{false,true}) {
  auto s=companionFixture();s.gameplay.companion.enabled=true;s.gameplay.companion.tile=124;
  s.population.entities={{1,4,EntityKind::Friend,126,greeted},{2,10,EntityKind::Item,128,false}};
  if(greeted)s.onceKeys.insert("greet:940:0:1");
  for(int tile=0;tile<kTiles;++tile)if(s.world.walkable(tile)&&tile/24!=5)s.blocked[tile]=1;
  Explorer e(s);for(unsigned tick=0;tick<20 && e.state().inventory.empty();++tick){e=Explorer(decodeSnapshot(encodeSnapshot(e.state()),"companion"));e.advanceGameplay(1000+tick*400,true);}
  check(e.state().inventory.size()==1 && canAttack(e.state().world,e.state().position,e.state().gameplay.companion.tile),"friendly corridor trapped/separated the party");
 }
 for(uint64_t seed=1;seed<=30;++seed){
  Explorer e(seed,seed,"companion",fixtureProfiles(3));e.enableGameplay(1000);check(e.enableCompanion(),"populated entry companion failed");
  for(unsigned tick=0;tick<2500 && e.state().gameplay.day==1;++tick){
   e=Explorer(decodeSnapshot(encodeSnapshot(e.state()),"companion"));e.advanceGameplay(1000+tick*1000,true);
   const auto& s=e.state();if(s.phase!=Phase::Home)check(canAttack(s.world,s.position,s.gameplay.companion.tile),"combat separated party");
  }
  check(e.state().gameplay.day==2,"populated party failed finite daily loop");
 }
 unsigned actions=0;
 for(uint64_t seed=1;seed<=100;++seed){
  Explorer e(seed,seed,"companion",fixtureProfiles(3),{0,0,0,0});e.enableGameplay(1000);check(e.enableCompanion(),"companion failed floor entry");
  for(unsigned tick=0;tick<1600 && e.state().phase!=Phase::Home;++tick){
   auto bytes=encodeSnapshot(e.state());e=Explorer(decodeSnapshot(bytes,"companion"));
   e.advanceGameplay(1000+tick*400,true);const auto& s=e.state();
   check(s.gameplay.companion.enabled && s.position!=s.gameplay.companion.tile,"companion disappeared/overlapped");
   if(s.phase!=Phase::Home)check(canAttack(s.world,s.position,s.gameplay.companion.tile),"companion left the one-tile radius");
   ++actions;
  }
  if(!e.state().completed){const auto& s=e.state();std::cerr<<"seed="<<seed<<" floor="<<s.floorIndex<<" phase="<<int(s.phase)<<" life="<<lifeName(s.gameplay.life)<<" player="<<s.position<<" partner="<<s.gameplay.companion.tile<<" target="<<s.target<<" steps="<<s.floorSteps<<" round="<<s.gameplay.actionRound<<" partnerRound="<<s.gameplay.companion.actionRound<<'\n';}
  check(e.state().completed,"following prevented dungeon completion");
 }
 auto old=encodeSnapshot(companionFixture());removeCompanionBlock(old);old[4]=6;crc(old);
 Explorer migrated(decodeSnapshot(old,"companion"));check(!migrated.state().gameplay.companion.enabled && migrated.enableCompanion(),"MDES6 did not safely enable a companion");
 auto invalid=migrated.state();invalid.gameplay.companion.tile=invalid.position;bool rejected=false;
 try{encodeSnapshot(invalid);}catch(const std::exception&){rejected=true;}check(rejected,"overlapping companion accepted");
 std::cout<<"16 directional/reversal cases, pause/partial restore, MDES6 migration and "<<actions<<" restored party actions pass\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
