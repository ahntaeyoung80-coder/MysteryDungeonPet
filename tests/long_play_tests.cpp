#include "core/dialogue_catalogue.hpp"
#include "core/event_registry.hpp"
#include "core/exploration_package.hpp"
#include "core/gameplay_notice.hpp"
#include "core/holiday_scene.hpp"
#include "core/motion.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
using namespace pet;
namespace {
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
struct Counts {
    uint64_t ticks=0,restores=0,pauses=0,days=0,floors=0,berries=0,pickups=0,attacks=0;
    uint64_t bossEntries=0,bossTalks=0,preparations=0,returns=0,holidays=0;
    size_t maxSnapshot=0,maxInventory=0,maxJournal=0;
};
std::string json(const Counts& n) {
    std::ostringstream o;
    o<<"{\"ticks\":"<<n.ticks<<",\"restores\":"<<n.restores<<",\"pauses\":"<<n.pauses
     <<",\"days\":"<<n.days<<",\"floors\":"<<n.floors<<",\"berries\":"<<n.berries
     <<",\"pickups\":"<<n.pickups<<",\"attacks\":"<<n.attacks<<",\"boss_entries\":"<<n.bossEntries
     <<",\"boss_talks\":"<<n.bossTalks<<",\"preparations\":"<<n.preparations<<",\"returns\":"<<n.returns
     <<",\"holidays\":"<<n.holidays<<",\"max_snapshot_bytes\":"<<n.maxSnapshot
     <<",\"max_inventory\":"<<n.maxInventory<<",\"max_journal\":"<<n.maxJournal<<"}";return o.str();
}
ActorCompletion completions(const ExplorationState& s,uint64_t now,bool missing,unsigned tick) {
    ActorCompletion done;done.keyed=true;
    // A second seed drops every finite-animation completion. Timeouts must keep the run finite.
    if(missing || tick%7==0)return done;
    done.player=done.enemy=done.partner=now>=s.gameplay.phaseStartedMs+600;
    for(const auto& v:s.gameplay.partyPlaybacks)if(now>=v.startedMs+600)
        done.actions.push_back({s.runId,v.round,v.id,s.floorIndex,v.actorId});
    // Include stale tokens to ensure that replayed acknowledgements cannot apply a new impact.
    if(!s.gameplay.partyPlaybacks.empty()) {
        const auto& v=s.gameplay.partyPlaybacks.front();
        done.actions.push_back({s.runId-1,v.round,v.id,s.floorIndex,v.actorId});
    }
    return done;
}
std::string location(const ExplorationState& s) {
    std::ostringstream o;o<<"run="<<s.runId<<" day="<<s.gameplay.day<<" floor="<<s.floorIndex
     <<" life="<<unsigned(s.gameplay.life)<<" scene="<<unsigned(s.gameplay.sceneBeat)
     <<" combat="<<unsigned(s.gameplay.combat)<<" pos="<<s.position<<" hp="<<s.gameplay.hearts
     <<"/"<<s.gameplay.companion.hearts<<" steps="<<s.floorSteps<<" event="<<unsigned(s.variety.event.id.type)
     <<" dialogue="<<s.variety.dialogue.bundle;return o.str();
}
}
int main(int argc,char** argv) {
    unsigned days=2,seeds=1;std::vector<unsigned> roots{1,14},modes{0,1,2};std::filesystem::path output;std::set<std::string> skipped;
    try {
        for(int i=1;i<argc;++i) {
            const std::string arg=argv[i];check(i+1<argc,"missing option value");const std::string value=argv[++i];
            if(arg=="--days")days=std::stoul(value);
            else if(arg=="--seeds")seeds=std::stoul(value);
            else if(arg=="--output")output=value;
            else if(arg=="--skip-passed") {
                std::ifstream prior(value);check(bool(prior),"cannot read prior case results");std::string line;
                while(std::getline(prior,line))if(line.find("\"passed\":true")!=std::string::npos) {
                    const auto begin=line.find("\"case\":\"");check(begin!=std::string::npos,"prior case missing identity");
                    const auto start=begin+8,end=line.find('"',start);check(end!=std::string::npos,"invalid prior case identity");
                    skipped.insert(line.substr(start,end-start));
                }
            }
            else if(arg=="--roots" || arg=="--modes") {auto& list=arg=="--roots"?roots:modes;list.clear();std::istringstream in(value);std::string n;while(std::getline(in,n,','))list.push_back(std::stoul(n));}
            else throw std::runtime_error("unknown option");
        }
        check(days>0 && days<=30 && seeds>0 && seeds<=8 && !roots.empty() && !modes.empty() && *std::max_element(modes.begin(),modes.end())<=2,"invalid long play bounds");
        const auto assets=std::filesystem::path(__FILE__).parent_path().parent_path()/"assets/original";
        const auto catalogue=loadDialogueCatalogue(assets/"gameplay/dialogue/catalogue.mdlc");
        const auto maps=loadEventRegistry(assets/"gameplay/events/contexts.mder");
        const auto visits=loadHolidayRegistry(assets/"gameplay/holiday");
        const std::vector<unsigned> ids{1,4,8,14,17,21,35,41};
        std::map<unsigned,ExplorationPackage> packages;
        for(auto id:ids)packages.emplace(id,loadExplorationPackage(assets/"exploration"/("dungeon-"+std::to_string(id))/"exploration.mdpe"));
        std::ofstream report;if(!output.empty()){std::filesystem::create_directories(output);report.open(output/"cases.jsonl");}
        const auto started=std::chrono::steady_clock::now();unsigned cases=0,failures=0;Counts total;
        for(auto id:roots)for(unsigned mode:modes)for(unsigned seed=0;seed<seeds;++seed) {
            const auto name=std::to_string(id)+"-"+std::to_string(mode)+"-"+std::to_string(seed);
            if(skipped.count(name))continue;
            const auto caseStarted=std::chrono::steady_clock::now();
            Counts n;std::string error;std::vector<uint8_t> checkpoint;
            const auto& p=packages.at(id);
            Explorer e(10000+id*100+mode*10+seed,seed?99:41,p.assetVersion,p.floors,{}, {},p.expedition);
            try {
                e.enableGameplay(1000);if(mode)check(e.enableCompanion(),"cannot enable companion");
                e.enableHolidaySchedule();e.enableScreenExploration(200);
                auto scheduled=e.state();scheduled.variety.schedule.destination=DayDestination::Exploration;
                scheduled.variety.schedule.dungeon=id;e=Explorer(scheduled);e.startGuildMorning(1000);
                uint64_t now=1000,configuredRun=0;unsigned upcoming=id,dayTicks=0;
                std::set<uint64_t> returnedRuns,bossRuns,preparedRuns,holidayRuns;
                std::set<std::pair<uint64_t,unsigned>> enteredBoss;
                while(e.state().gameplay.day<=days) {
                    ++n.ticks;++dayTicks;
                    if(n.ticks%5000==0)std::cerr<<"PROGRESS "<<name<<" tick="<<n.ticks<<" "<<location(e.state())<<std::endl;
                    const auto camera=cameraFor(tilePoint(e.state().position),kWidth*24,kHeight*24,200);
                    e.observeViewport(camera.x,camera.y);
                    const auto& phase=e.state().gameplay;
                    if(phase.life==LifePhase::Combat && (phase.combat==CombatPhase::PlayerFaint || phase.combat==CombatPhase::CompanionFaint))
                        check(now<=phase.deadlineMs+60000,"faint completion did not time out");
                    check(dayTicks<100000,"day did not end within 100000 decisions");
                    if(e.state().runId!=configuredRun) {
                        // This is the same external package policy that main.cpp reapplies after restore.
                        upcoming=e.chooseNextDungeon(ids);configuredRun=e.state().runId;
                        if(mode==1 && e.state().gameplay.independentParty) {
                            auto legacy=e.state();legacy.gameplay.independentParty=false;e=Explorer(legacy);
                        }
                    }
                    const auto& next=packages.at(upcoming);e.setNextRunPackage(next.assetVersion,next.floors,next.expedition);
                    if(n.ticks%997==0) {
                        const auto before=e.state();const auto rng=before.rng.state;
                        e.shiftPausedClocks(60000,now);now+=60000;++n.pauses;
                        check(e.state().rng.state==rng && e.state().inventory.size()==before.inventory.size() &&
                              e.state().gameplay.oran==before.gameplay.oran && e.state().gameplay.companion.oran==before.gameplay.companion.oran,
                              "pause granted or consumed inventory/RNG");
                    }
                    const auto before=e.state();
                    const auto done=completions(before,now,seed%2==1,unsigned(n.ticks));
                    const bool restore=n.ticks%31==0 || before.gameplay.life!=LifePhase::Explore ||
                        before.gameplay.sceneBeat!=SceneBeat::None || !before.gameplay.partyPlaybacks.empty();
                    GameplayUpdate update;
                    if(restore) {
                        checkpoint=encodeSnapshot(before);n.maxSnapshot=std::max(n.maxSnapshot,checkpoint.size());
                        Explorer copy(decodeSnapshot(checkpoint,before.assetVersion));
                        copy.setNextRunPackage(next.assetVersion,next.floors,next.expedition);
                        update=e.advanceGameplay(now,done,catalogue,&maps,&visits);
                        const auto replay=copy.advanceGameplay(now,done,catalogue,&maps,&visits);
                        check(update.event==replay.event && update.onceKey==replay.onceKey &&
                              encodeSnapshot(e.state())==encodeSnapshot(copy.state()),"restored next decision diverged");
                        e=std::move(copy);++n.restores;
                    } else update=e.advanceGameplay(now,done,catalogue,&maps,&visits);
                    const auto& after=e.state();const auto& g=after.gameplay;
                    if(g.day==1 && g.life==LifePhase::Explore)check(after.floors.front().dungeonId==id,"first exploration changed the selected root");
                    check(g.oran<=1 && g.companion.oran<=1,"actor exceeded one berry");
                    check(g.hearts<=kMaxHp && g.companion.hearts<=kMaxHp,"health exceeded maximum");
                    check(g.day==before.gameplay.day || g.day==before.gameplay.day+1,"date skipped or decreased");
                    check(g.meals==g.day-1 || (g.meals==g.day && after.variety.schedule.mealCommitted),"meal duplicated or missing for date");
                    check(after.inventory.size()>=before.inventory.size(),"historical pickups disappeared");
                    n.maxInventory=std::max(n.maxInventory,after.inventory.size());
                    n.maxJournal=std::max(n.maxJournal,g.onceKeys.size()+after.onceKeys.size()+after.variety.committed.size());
                    unsigned used=0;
                    if(update.actors.empty()){if(update.event==GameplayEvent::UseOranBerry)++used;}
                    for(const auto& a:update.actors) {
                        if(a.event==GameplayEvent::UseOranBerry)++used;
                        if(a.event==GameplayEvent::EnemyAttack && std::find(before.gameplay.hostile.begin(),before.gameplay.hostile.end(),a.actorId)!=before.gameplay.hostile.end())
                            check(!a.critical,"enemy critical reappeared");
                    }
                    if(update.actors.empty() && update.event==GameplayEvent::EnemyAttack)check(!update.critical,"legacy enemy critical reappeared");
                    n.berries+=used;
                    // Optional exchanges/shares have a separate ledger; direct consumptions must debit exactly once.
                    unsigned collected=0;
                    for(size_t i=before.inventory.size();i<after.inventory.size();++i)if(after.inventory[i].originalId==ITEM_ORAN_BERRY)++collected;
                    if(used && before.runId==after.runId) {
                        check(before.gameplay.oran+before.gameplay.companion.oran+collected==g.oran+g.companion.oran+used,"berry use did not debit exactly one slot");
                    }
                    n.pickups+=after.inventory.size()-before.inventory.size();
                    if(update.event==GameplayEvent::PlayerAttack || update.event==GameplayEvent::EnemyAttack || update.event==GameplayEvent::CompanionAttack)++n.attacks;
                    if(after.floorIndex!=before.floorIndex && after.runId==before.runId)++n.floors;
                    for(const auto& actor:after.population.entities)if(isBossEntity(after,actor)) {
                        if(enteredBoss.insert({after.runId,after.floorIndex}).second)++n.bossEntries;
                        check(actor.remainingHits<=4,"boss HP exceeded four");
                    }
                    if(bossConversation(after) && bossRuns.insert(after.runId).second)++n.bossTalks;
                    if(bossPreparation(after) && preparedRuns.insert(after.runId).second)++n.preparations;
                    if(after.variety.holiday.active && holidayRuns.insert(after.runId).second)++n.holidays;
                    if(g.life==LifePhase::ReturnHome && before.gameplay.life!=LifePhase::ReturnHome) {
                        check(returnedRuns.insert(after.runId).second,"same run returned home twice");++n.returns;
                    }
                    if(g.day!=before.gameplay.day) {
                        check(g.day==n.days+2 && g.meals==g.day-1,"day or meal committed twice");
                        check(g.oran==before.gameplay.oran && g.companion.oran==before.gameplay.companion.oran,"new day refilled berries");
                        check(after.floors.front().dungeonId==upcoming,"new day ignored scheduled package");
                        ++n.days;dayTicks=0;
                    }
                    now+=250;
                }
                check(n.days==days && n.pickups>0 && n.returns>=days,"long play omitted pickups or returns");
            } catch(const std::exception& ex) {
                error=ex.what();++failures;
                std::cerr<<"FAIL "<<name<<": "<<error<<"; "<<location(e.state())<<'\n';
                if(!output.empty()) {
                    try{saveSnapshot(output/(name+"-failure.mdes"),e.state());}
                    catch(...){std::ofstream f(output/(name+"-checkpoint.mdes"),std::ios::binary);f.write(reinterpret_cast<const char*>(checkpoint.data()),std::streamsize(checkpoint.size()));}
                }
            }
            ++cases;
            std::ostringstream row;row<<"{\"case\":\""<<name<<"\",\"root\":"<<id<<",\"mode\":"<<mode<<",\"seed\":"<<(seed?99:41)
                <<",\"missing_completions\":"<<(seed%2?"true":"false")<<",\"passed\":"<<(error.empty()?"true":"false")<<",\"seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-caseStarted).count()<<",\"counts\":"<<json(n)<<"}";
            std::cout<<row.str()<<std::endl;if(report)report<<row.str()<<std::endl;
            for(auto member:{&Counts::ticks,&Counts::restores,&Counts::days,&Counts::berries,&Counts::pauses,&Counts::floors,&Counts::pickups,&Counts::attacks,&Counts::bossEntries,&Counts::bossTalks,&Counts::preparations,&Counts::returns,&Counts::holidays})total.*member+=n.*member;
            total.maxSnapshot=std::max(total.maxSnapshot,n.maxSnapshot);total.maxInventory=std::max(total.maxInventory,n.maxInventory);total.maxJournal=std::max(total.maxJournal,n.maxJournal);
        }
        const auto seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count();
        std::ostringstream summary;summary<<"{\"cases\":"<<cases<<",\"failures\":"<<failures<<",\"seconds\":"<<seconds<<",\"totals\":"<<json(total)<<"}";
        std::cout<<summary.str()<<'\n';if(!output.empty()){std::ofstream f(output/"summary.json");f<<summary.str()<<'\n';}
        return failures?1:0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
