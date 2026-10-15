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
int main(){try{
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
