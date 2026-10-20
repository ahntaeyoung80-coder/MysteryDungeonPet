#include "core/exploration.hpp"
#include "core/gameplay_scene.hpp"
#include "core/animator.hpp"
#include "core/package.hpp"
#include "core/exploration_package.hpp"
#include <chrono>
#include <fstream>
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace pet;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
ExplorationState socialFixture(){
 Explorer initial(920,42,"entry-social",fixtureProfiles(3),{0,0,0,0});auto s=initial.state();
 s.world.terrain.fill(0);s.world.room.fill(-1);
 for(int y=2;y<=7;++y)for(int x=2;x<=7;++x){s.world.terrain[y*24+x]=1;s.world.room[y*24+x]=0;}
 for(int y=2;y<=7;++y)for(int x=10;x<=15;++x){s.world.terrain[y*24+x]=1;s.world.room[y*24+x]=1;}
 for(int x=7;x<=10;++x)s.world.terrain[4*24+x]=1;
 s.world.centers={4*24+4,4*24+12};s.world.start=s.world.centers[0];s.world.stairs=s.world.centers[1];
 s.position=3*24+3;s.visited.fill(0);s.visited[s.position]=1;s.discovered=s.world.terrain;s.knownStairs=s.world.stairs;s.gameplay.enabled=true;
 return s;
}
Explorer restore(const Explorer& e){return Explorer(decodeSnapshot(encodeSnapshot(e.state()),e.state().assetVersion));}
void floors(){
 for(uint64_t seed=1;seed<=1000;++seed){
  Explorer e(seed,seed,"entry-social",fixtureProfiles(3));
  std::vector<Mask> layouts{e.state().world.terrain};
  for(unsigned floor=1;floor<3;++floor){
   auto s=e.state();const auto old=s.world.terrain;const int oldFeet=s.world.stairs;
   s.position=oldFeet;s.visited[oldFeet]=1;e=Explorer(s);e.advance();e=restore(e);
   const auto& n=e.state();
   check(n.floorIndex==floor && n.position==n.world.start && n.position!=n.world.stairs,"new floor spawned stairs at entry");
   check(n.world.stairs!=oldFeet,"new floor put stairs under previous player feet");
   for(const auto& a:n.population.entities)check(a.tile!=n.position && a.tile!=oldFeet,"new floor put an entity under entry/previous feet");
   check(n.world.terrain!=old && std::find(layouts.begin(),layouts.end(),n.world.terrain)==layouts.end(),"new floor reused an earlier layout");
   layouts.push_back(n.world.terrain);validateWorld(n.world);
   struct Bounds{int x0=24,y0=24,x1=-1,y1=-1;};std::vector<Bounds> bounds(n.world.centers.size());
   for(int tile=0;tile<kTiles;++tile)if(n.world.room[tile]>=0){auto& b=bounds[n.world.room[tile]];b.x0=std::min(b.x0,tile%24);b.x1=std::max(b.x1,tile%24);b.y0=std::min(b.y0,tile/24);b.y1=std::max(b.y1,tile/24);}
   for(unsigned i=0;i<bounds.size();++i)for(unsigned j=0;j<i;++j){const auto a=bounds[i],b=bounds[j];check(a.x1<b.x0||b.x1<a.x0||a.y1<b.y0||b.y1<a.y0,"room rectangles overlap");}
  }
 }
 // Use an unmodified generated world to force the same candidate from saved RNG.
 Explorer first(921,196,"entry-social",fixtureProfiles(3));auto s=first.state();s.rng.state=s.mapSeed;
 const auto old=s.world.terrain;s.position=s.world.stairs;s.visited[s.position]=1;Explorer replay(s);replay.advance();
 check(replay.state().world.terrain!=old,"replayed map seed bypassed layout deduplication");
 s=replay.state();s.position=s.world.stairs;s.visited[s.position]=1;s.rng.state=first.state().mapSeed;
 replay=restore(Explorer(s));replay.advance();check(replay.state().world.terrain!=old,"restored third floor reused first floor layout");
 std::cout<<"floor entry/layout regression pass\n";
}
void npc(){
 auto s=socialFixture();s.population.entities={{1,4,EntityKind::Friend,6*24+6,false}};
 Explorer e(s);e.advanceGameplay(1000);check(e.goal()==GoalKind::Friend,"off-path friendly NPC was not a regular priority");
 uint64_t now=1400;unsigned greetings=0;
 for(unsigned tick=0;tick<80 && !e.state().population.entities[0].done;++tick,now+=400){
  e=restore(e);if(e.advanceGameplay(now,true).event==GameplayEvent::DialoguePlayer)++greetings;
 }
 check(greetings==1 && e.state().population.entities[0].done && e.state().onceKeys.count("greet:920:0:1"),"NPC did not greet/respond exactly once");
 e=restore(e);for(unsigned tick=0;tick<6;++tick,now+=400)check(e.advanceGameplay(now,true).event!=GameplayEvent::DialoguePlayer,"completed NPC repeated greeting");
 s.population.entities.push_back({2,10,EntityKind::Item,3*24+5,false});Explorer items(s);items.advanceGameplay(1000);check(items.goal()==GoalKind::Item,"NPC displaced an unfinished item");
 auto heading=s;heading.population.entities.pop_back();heading.target=heading.world.stairs;heading.goalKind=GoalKind::Stairs;
 Explorer reconsider(heading);reconsider.advanceGameplay(1000);check(reconsider.goal()==GoalKind::Friend,"known stairs ignored a newly available friendly NPC");
 auto pending=reconsider.state();pending.population.entities.push_back({2,10,EntityKind::Item,3*24+5,false});
 reconsider=restore(Explorer(pending));reconsider.advanceGameplay(1400);check(reconsider.goal()==GoalKind::Friend && reconsider.state().targetInstance==1,"new target replaced an unfinished friendly goal on restore");
 s.gameplay.hearts=2;Explorer survival(s);survival.advanceGameplay(1000);check(survival.goal()==GoalKind::Stairs,"NPC displaced low-HP survival");
 s.gameplay.hearts=5;s.gameplay.hostile={1};s.population.entities.pop_back();Explorer hostile(s);hostile.advanceGameplay(1000);check(hostile.goal()!=GoalKind::Friend,"hostile selected for greeting");
 std::cout<<"friendly priority/dialogue regression pass\n";
}
void scenes(){
 auto s=socialFixture();s.phase=Phase::Home;s.gameplay.homeScene=true;s.gameplay.life=LifePhase::Wake;
 s.gameplay.sceneBeat=SceneBeat::BedFadeIn;s.gameplay.phaseStartedMs=1000;s.gameplay.deadlineMs=1750;s.gameplay.meals=1;s.gameplay.onceKeys={"return:920","meal:920"};
 check(gameplayClip(s.gameplay)==13 && !gameplayLoop(s.gameplay),"morning fade-in stands before Wake14");
 check(holdGameplayPose(s.gameplay),"morning fade replays sleep onset instead of holding its last pose");
 const auto root=std::filesystem::path(__FILE__).parent_path().parent_path();
 for(const auto actor:{"asset-0","asset-1"}){
  const auto p=loadPackage(root/"assets/original/gameplay/actors"/actor/"asset.mdpa");
  Animator pose;pose.start(p,13,2,false,0);const auto held=pose.advance(60000000);
  check(held.finished && held.frame+1==p.frames(13,2).size(),"original sleep clip does not hold final frame");
 }
 const auto apple=loadPackage(root/"assets/original/gameplay/effects/apple.mdpa");
 check(apple.frames(624,0).size()==13 && apple.tickHz==60,"original apple sequence absent");
 check(loopFrameAt(apple,624,0)==0 && loopFrameAt(apple,624,66)==0 && loopFrameAt(apple,624,67)==1 && loopFrameAt(apple,624,867)==0,"original apple ticks changed");
 const auto ui=loadPackage(root/"assets/original/gameplay/ui/ui.mdpa");Surface atlas(144,8);
 for(unsigned y=0;y<8;++y)for(unsigned x=0;x<144;++x){auto i=(y*144+x)*4;atlas.pixels[i]=x/8;atlas.pixels[i+3]=255;}
 const auto panel=originalDialoguePanel(atlas,ui);
 for(const auto v:std::vector<std::array<unsigned,3>>{{0,0,2},{8,0,3},{176,0,4},{0,8,5},{8,8,1},{176,8,6},{0,40,7},{8,40,8},{176,40,9}})
  check(panel.pixels[(v[1]*184+v[0])*4]==v[2],"dialogue border tile position changed");
 auto text=s.gameplay;text.sceneBeat=SceneBeat::WakeDelay;
 check(revealedGameplayText(text,1000).size()==1 && revealedGameplayText(text,1050).size()==3 && revealedGameplayText(text,3000)==gameplayText(text),"text reveal timing changed");
 Explorer e(s);e=restore(e);e.advanceGameplay(1750);
 check(e.state().gameplay.sceneBeat==SceneBeat::AlarmEnter,"Loudred entrance absent");
 for(unsigned ms=0;ms<=800;ms+=100) {
  const auto a=loudredAlarm({112,180},e.state().gameplay,1750+ms);
  check(a.position.x==64+int(48*ms/800) && a.position.y==180 && a.direction==2,"Loudred entrance teleported");
 }
 e=restore(e);e.advanceGameplay(2550);
 check(e.state().gameplay.sceneBeat==SceneBeat::WakeDelay && gameplayClip(e.state().gameplay)==13 && holdGameplayPose(e.state().gameplay),"alarm did not hold sleeping bodies");
 e=restore(e);e.advanceGameplay(2550+kWakeDelayMs);
 check(e.state().gameplay.sceneBeat==SceneBeat::AlarmLeave && holdGameplayPose(e.state().gameplay),"Loudred exit did not hold sleepers");
 for(unsigned ms=0;ms<=800;ms+=100) {
  const auto a=loudredAlarm({112,180},e.state().gameplay,4550+ms);
  check(a.position.x==112-int(48*ms/800) && a.position.y==180 && a.direction==6,"Loudred exit teleported");
 }
 e=restore(e);e.advanceGameplay(5350);check(gameplayClip(e.state().gameplay)==14,"Wake14 missing after Loudred left");
 s=e.state();s.gameplay.life=LifePhase::Ready;s.gameplay.sceneBeat=SceneBeat::RoomExit;s.gameplay.phaseStartedMs=2000;s.gameplay.deadlineMs=2000+kRoomExitMs;
 e=Explorer(s);e.advanceGameplay(s.gameplay.deadlineMs,true);
 check(e.state().gameplay.sceneBeat==SceneBeat::MorningFadeOut && e.state().gameplay.day==1 && e.state().gameplay.homeScene,"room exit skipped fade-out");
 const auto exit=bedroomExit({204,184},e.state().gameplay,e.state().gameplay.phaseStartedMs);
 check(exit.finished && exit.position.x==80 && exit.position.y==184 && gameplayClip(e.state().gameplay)==7,"fade-out teleported to bed or replayed greeting");
 const auto start=e.state().gameplay.phaseStartedMs;e=restore(e);e.advanceGameplay(start+749,true);check(e.state().gameplay.day==1,"next day committed before blackout");
 e.advanceGameplay(start+750,true);check(e.state().gameplay.sceneBeat==SceneBeat::ExploreFadeIn && e.state().gameplay.day==2 && e.state().gameplay.hearts==kMaxHp,"black frame did not commit next day/fade-in");
 auto bytes=encodeSnapshot(e.state());e=restore(e);e.advanceGameplay(start+1499,true);check(bytes==encodeSnapshot(e.state()),"exploration moved during fade-in");
 e.advanceGameplay(start+1500,true);check(e.state().gameplay.sceneBeat==SceneBeat::None && e.state().gameplay.day==2,"fade-in duplicated next day");
 std::cout<<"sleep pose/exit fade/restore regression pass\n";
}
int main(int argc,char** argv){unsigned failed=0;
 if(argc==4 && (std::string(argv[1])=="--fixtures" || std::string(argv[1])=="--wake-fixture"))try{
  const auto p=loadExplorationPackage(argv[2]);const std::filesystem::path out=argv[3];std::filesystem::create_directories(out);
  if(std::string(argv[1])=="--fixtures"){
   Explorer floor(930,196,p.assetVersion,p.floors);auto s=floor.state();s.position=s.world.stairs;s.visited[s.position]=1;
   floor=Explorer(s);floor.advance();s=floor.state();s.position=s.world.stairs;s.visited[s.position]=1;
   floor=Explorer(s);floor.enableGameplay(0);saveSnapshot(out/"entry.mdes",floor.state());
   s=socialFixture();s.assetVersion=p.assetVersion;s.floors=p.floors;s.population.entities={{1,4,EntityKind::Friend,6*24+6,false}};
   saveSnapshot(out/"npc.mdes",s);
   const auto actors=std::filesystem::path(__FILE__).parent_path().parent_path()/"assets/original/gameplay/actors";
   const auto player=loadPackage(actors/"asset-0/asset.mdpa"),partner=loadPackage(actors/"asset-1/asset.mdpa");
   std::ofstream(out/"poses.json")<<"{\"playerLast\":"<<player.frames(13,2).size()-1<<",\"partnerLast\":"<<partner.frames(13,6).size()-1<<"}\n";
  } else {
   auto s=socialFixture();s.assetVersion=p.assetVersion;s.floors=p.floors;s.phase=Phase::Home;
   auto& g=s.gameplay;g.homeScene=true;g.life=LifePhase::Wake;g.sceneBeat=SceneBeat::BedFadeIn;g.hearts=3;g.meals=1;
   g.phaseStartedMs=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();g.deadlineMs=g.phaseStartedMs+750;
   g.onceKeys={"return:920","meal:920"};saveSnapshot(out/"wake.mdes",s);
   g.life=LifePhase::Ready;g.sceneBeat=SceneBeat::RoomExit;g.phaseStartedMs-=800;g.deadlineMs=g.phaseStartedMs+kRoomExitMs;
   saveSnapshot(out/"overlap.mdes",s);
  }
  return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
 if(argc==4 && std::string(argv[1])=="--inspect")try{
  const auto p=loadExplorationPackage(argv[2]);const auto s=loadSnapshot(argv[3],p.assetVersion);
  const auto greetings=std::count_if(s.onceKeys.begin(),s.onceKeys.end(),[](const auto& key){return key.rfind("greet:",0)==0;});
  std::cout<<"{\"runId\":"<<s.runId<<",\"floor\":"<<s.floorIndex<<",\"entry\":"<<s.world.start<<",\"stairs\":"<<s.world.stairs
   <<",\"player\":"<<s.position<<",\"day\":"<<s.gameplay.day<<",\"hearts\":"<<s.gameplay.hearts<<",\"greetings\":"<<greetings
   <<",\"completed\":"<<(s.completed?"true":"false")<<",\"life\":\""<<lifeName(s.gameplay.life)<<"\"}\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
 for(const auto& entry:{std::make_pair("floor",floors),std::make_pair("npc",npc),std::make_pair("scene",scenes)}){
  if(argc>1 && std::string(argv[1])!=entry.first)continue;
  try{entry.second();}catch(const std::exception& e){++failed;std::cerr<<entry.first<<": "<<e.what()<<'\n';}
 }
 return failed?1:0;
}
