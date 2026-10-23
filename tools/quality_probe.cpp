#include "core/exploration_package.hpp"
#include "core/dialogue_catalogue.hpp"
#include "core/event_registry.hpp"
#include "core/holiday_scene.hpp"
#include "core/motion.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
using namespace pet;
namespace {
const std::vector<unsigned> roots{1,4,8,14,17,21,35,41};
const auto assets=std::filesystem::path(__FILE__).parent_path().parent_path()/"assets/original";
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
std::string snapshot(const ExplorationState& s) {
 std::ostringstream o;o<<"{\"run\":"<<s.runId<<",\"day\":"<<s.gameplay.day<<",\"meals\":"<<s.gameplay.meals
 <<",\"root\":"<<s.floors.front().dungeonId<<",\"floor\":"<<s.floorIndex<<",\"life\":"<<unsigned(s.gameplay.life)
 <<",\"scene\":"<<unsigned(s.gameplay.sceneBeat)<<",\"position\":"<<s.position<<",\"hp\":"<<s.gameplay.hearts
 <<",\"partner_hp\":"<<s.gameplay.companion.hearts<<",\"oran\":"<<s.gameplay.oran<<",\"partner_oran\":"<<s.gameplay.companion.oran
 <<",\"inventory\":"<<s.inventory.size()<<",\"round\":"<<s.gameplay.actionRound<<",\"outcome\":"<<unsigned(s.expedition.outcome)
 <<",\"completed\":"<<(s.completed?"true":"false")<<",\"event\":"<<unsigned(s.variety.event.id.type)<<"}";return o.str();
}
struct Floor {
 uint64_t ms=0,combatMs=0,dialogueMs=0,moves=0,revisits=0,berries=0,pickups=0,unproductive=0,maxUnproductive=0;
 bool boss=false;unsigned entryHp=0,entryPartnerHp=0,exitHp=0,exitPartnerHp=0;
};
std::string floorJson(unsigned index,const Floor& f) {
 std::ostringstream o;o<<"{\"floor\":"<<index<<",\"duration_ms\":"<<f.ms<<",\"combat_ms\":"<<f.combatMs
 <<",\"dialogue_ms\":"<<f.dialogueMs<<",\"moves\":"<<f.moves<<",\"revisited_moves\":"<<f.revisits
 <<",\"max_moves_without_progress\":"<<f.maxUnproductive<<",\"berries_used\":"<<f.berries<<",\"pickups\":"<<f.pickups
 <<",\"boss\":"<<(f.boss?"true":"false")<<",\"entry_hp\":"<<f.entryHp<<",\"entry_partner_hp\":"<<f.entryPartnerHp
 <<",\"exit_hp\":"<<f.exitHp<<",\"exit_partner_hp\":"<<f.exitPartnerHp<<"}";return o.str();
}
}
int main(int argc,char** argv){try {
 if(argc==3 && std::string(argv[1])=="--inspect") {
  for(auto id:roots) {const auto p=loadExplorationPackage(assets/"exploration"/("dungeon-"+std::to_string(id))/"exploration.mdpe");
   try{std::cout<<snapshot(loadSnapshot(argv[2],p.assetVersion))<<'\n';return 0;}
   catch(const std::runtime_error& e){if(std::string(e.what())!="snapshot asset version mismatch")throw;}
  }
  throw std::runtime_error("snapshot not from a selected quality-check expedition");
 }
 std::filesystem::path out;std::vector<unsigned> selected=roots;unsigned helperHp=4;
 for(int i=1;i<argc;++i){require(i+1<argc,"missing option value");const std::string arg=argv[i],value=argv[++i];
  if(arg=="--output")out=value;
  else if(arg=="--helper-hp"){helperHp=std::stoul(value);require(helperHp>=1 && helperHp<=4,"invalid helper HP");}
  else if(arg=="--roots"){selected.clear();std::istringstream values(value);std::string token;while(std::getline(values,token,','))selected.push_back(std::stoul(token));}
  else throw std::runtime_error("unknown quality probe option");
 }
 require(!out.empty(),"provide --output DIR");std::filesystem::create_directories(out);std::ofstream cases(out/"cases.jsonl");
 const auto catalogue=loadDialogueCatalogue(assets/"gameplay/dialogue/catalogue.mdlc");const auto maps=loadEventRegistry(assets/"gameplay/events/contexts.mder");
 const auto visits=loadHolidayRegistry(assets/"gameplay/holiday");
 const auto started=std::chrono::steady_clock::now();unsigned count=0;
 for(auto root:selected)for(bool solo:{false,true})for(unsigned seedIndex=0;seedIndex<(solo?1:4);++seedIndex) {
  const unsigned seed=std::vector<unsigned>{41,7,42,99}[seedIndex];
  const auto p=loadExplorationPackage(assets/"exploration"/("dungeon-"+std::to_string(root))/"exploration.mdpe");
  Explorer e(60000+root*100+seedIndex*2+solo,seed,p.assetVersion,p.floors,{}, {},p.expedition);
  e.enableGameplay(1000);if(!solo)require(e.enableCompanion(),"companion spawn failed");e.enableScreenExploration(200);e.enableHolidaySchedule();
  auto scheduled=e.state();scheduled.variety.schedule.destination=DayDestination::Exploration;scheduled.variety.schedule.dungeon=root;e=Explorer(scheduled);e.startGuildMorning(1000);
  std::set<unsigned> adjustedFloors;
  std::map<unsigned,Floor> floors;uint64_t now=1000,ticks=0,idle=0,maxIdle=0;unsigned warnings=0,talks=0;bool prepared=false,befriended=false;
  while(e.state().gameplay.life!=LifePhase::ReturnHome && ++ticks<100000) {
   const auto camera=cameraFor(tilePoint(e.state().position),kWidth*24,kHeight*24,200);e.observeViewport(camera.x,camera.y);
   if(helperHp<4 && (root==17 || root==35) && e.state().expedition.floors.at(e.state().floorIndex).fixedFloorId && adjustedFloors.insert(e.state().floorIndex).second) {
    auto adjusted=e.state();for(auto& actor:adjusted.population.entities)if(isBossEntity(adjusted,actor) && actor.originalId==(root==17?337U:139U))actor.remainingHits=helperHp;
    e=Explorer(adjusted);
   }
   const auto before=e.state();const auto& g=before.gameplay;
   ActorCompletion done;done.keyed=true;done.player=done.enemy=done.partner=now>=g.phaseStartedMs+600;
   for(const auto& v:g.partyPlaybacks)if(now>=v.startedMs+600)done.actions.push_back({before.runId,v.round,v.id,before.floorIndex,v.actorId});
   const auto update=e.advanceGameplay(now,done,catalogue,&maps,&visits);const auto& after=e.state();const auto& ag=after.gameplay;
   require(ag.oran<=1 && ag.companion.oran<=1,"berry capacity exceeded");
   if(ag.life!=g.life || ag.sceneBeat!=g.sceneBeat || after.floorIndex!=before.floorIndex)(void)encodeSnapshot(after);
   if(!g.homeScene) {
    auto [it,inserted]=floors.emplace(before.floorIndex,Floor{});auto& f=it->second;
    if(inserted){f.entryHp=g.hearts;f.entryPartnerHp=g.companion.hearts;}
    f.ms+=250;if(g.life==LifePhase::Combat)f.combatMs+=250;if(g.life==LifePhase::Dialogue || before.variety.dialogue.active)f.dialogueMs+=250;
    f.exitHp=ag.hearts;f.exitPartnerHp=ag.companion.hearts;
    for(const auto& a:before.population.entities)f.boss|=isBossEntity(before,a);
    unsigned used=0;if(update.actors.empty() && update.event==GameplayEvent::UseOranBerry)++used;
    for(const auto& a:update.actors)used+=a.event==GameplayEvent::UseOranBerry;
    f.berries+=used;
    f.pickups+=after.inventory.size()-before.inventory.size();
    const bool sameFloor=after.floorIndex==before.floorIndex;
    const bool moved=sameFloor && after.position!=before.position;
    const bool progress=!sameFloor || after.inventory.size()>before.inventory.size() || ag.hearts!=g.hearts || ag.companion.hearts!=g.companion.hearts ||
       after.variety.event.progress!=before.variety.event.progress ||
       std::count(after.visited.begin(),after.visited.end(),1)>std::count(before.visited.begin(),before.visited.end(),1) ||
       std::count(after.screenSeen.begin(),after.screenSeen.end(),1)>std::count(before.screenSeen.begin(),before.screenSeen.end(),1) ||
       std::any_of(after.population.entities.begin(),after.population.entities.end(),[&](const auto& a){const auto old=std::find_if(before.population.entities.begin(),before.population.entities.end(),[&](const auto& b){return b.instanceId==a.instanceId;});return old!=before.population.entities.end() && (a.done!=old->done || a.remainingHits!=old->remainingHits);});
    if(progress){f.unproductive=0;idle=0;}
    if(moved){++f.moves;f.revisits+=before.visited[after.position]!=0;if(!progress)++f.unproductive;f.maxUnproductive=std::max(f.maxUnproductive,f.unproductive);}
    if(ag.life==LifePhase::Explore && ag.sceneBeat==SceneBeat::None && !varietyBusy(after.variety) && !moved && update.event==GameplayEvent::None)idle+=250;else idle=0;
    maxIdle=std::max(maxIdle,idle);
   }
   if(bossPreparation(after) && !prepared){++warnings;prepared=true;}
   if(bossConversation(after) && !befriended){++talks;befriended=true;}
   now+=250;
  }
  if(ticks>=100000){saveSnapshot(out/(std::to_string(root)+"-stalled.mdes"),e.state());throw std::runtime_error("quality route exceeded decision limit");}
  uint64_t moves=0,revisits=0,berries=0,maxNoProgress=0;for(auto& [i,f]:floors){moves+=f.moves;revisits+=f.revisits;berries+=f.berries;maxNoProgress=std::max(maxNoProgress,f.maxUnproductive);}
  std::ostringstream row;row<<"{\"root\":"<<root<<",\"solo\":"<<(solo?"true":"false")<<",\"seed\":"<<seed
   <<",\"helper_hp_hypothesis\":"<<helperHp<<",\"approximate_virtual_ms\":"<<now-1000<<",\"ticks\":"<<ticks<<",\"outcome\":"<<unsigned(e.state().expedition.outcome)
   <<",\"completed\":"<<(e.state().completed?"true":"false")<<",\"total_floors\":"<<p.floors.size()<<",\"floors_visited\":"<<floors.size()
   <<",\"moves\":"<<moves<<",\"revisited_moves\":"<<revisits<<",\"berries_used\":"<<berries<<",\"max_moves_without_progress\":"<<maxNoProgress
   <<",\"max_unexplained_idle_ms\":"<<maxIdle<<",\"boss_preparations\":"<<warnings<<",\"boss_talks\":"<<talks<<",\"floors\":[";
  bool first=true;for(auto& [i,f]:floors){if(!first)row<<',';first=false;row<<floorJson(i,f);}row<<"]}";
  cases<<row.str()<<std::endl;std::cout<<"root="<<root<<" solo="<<solo<<" seed="<<seed<<" outcome="<<unsigned(e.state().expedition.outcome)<<" virtual_s="<<(now-1000)/1000<<"\n"<<std::flush;++count;
 }
 const auto seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count();std::ofstream summary(out/"summary.json");summary<<"{\"cases\":"<<count<<",\"seconds\":"<<seconds<<",\"clock\":\"virtual 250ms; acknowledgements after 600ms, not measured animation wall time\"}\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
