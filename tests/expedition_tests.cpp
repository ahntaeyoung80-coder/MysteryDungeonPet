#include "core/exploration_package.hpp"
#include "core/exploration_io.hpp"
#include "core/npc_characters.hpp"
#include "core/event_registry.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>
using namespace pet;
namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
Explorer fixture() {
    FloorProfile first;first.edition="test-fixture";first.dungeonId=14;
    auto second=first;second.dungeonId=15;
    auto last=first;last.dungeonId=16;
    ExpeditionState route;route.routeId="RUN-D014";route.name="fixture expedition";
    ExpeditionStop stop;stop.groundId="GND-L027-D10P21A";stop.assetFile="fixture.mdpa";
    stop.width=stop.height=200;stop.playerX=stop.partnerX=100;stop.playerY=100;stop.partnerY=124;
    ExpeditionFloor a;a.dungeonId=14;a.exitLink="LNK-D014-D015";a.stops={stop};
    ExpeditionFloor b;b.dungeonId=15;b.exitLink="LNK-D015-D016";
    ExpeditionFloor c;c.dungeonId=16;c.fixedFloorId=3;c.fixedSpawns={{415,0,0,8,2,6}};
    route.floors={a,b,c};
    Explorer e(9200,31,"expedition-fixture",{first,second,last},{0,0,0,0},{},route);
    e.enableGameplay(1000);return e;
}
void stairs(Explorer& e) {
    auto s=e.state();s.position=floorExitTile(s);s.visited[s.position]=1;s.discovered=s.world.terrain;
    s.target=s.position;s.goalKind=GoalKind::Stairs;s.targetInstance=0;
    s.gameplay.sceneBeat=SceneBeat::None;s.gameplay.deadlineMs=s.gameplay.phaseStartedMs;s.gameplay.movement={};e=Explorer(std::move(s));
}
void replay(Explorer& e,std::uint64_t now) {
    Explorer restored(decodeSnapshot(encodeSnapshot(e.state()),e.state().assetVersion));
    const auto a=e.advanceGameplay(now,true),b=restored.advanceGameplay(now,true);
    check(a.event==b.event && a.onceKey==b.onceKey && encodeSnapshot(e.state())==encodeSnapshot(restored.state()),"expedition resume differs from uninterrupted play");
}
void quick() {
    auto e=fixture();stairs(e);replay(e,1000);replay(e,2000);
    check(expeditionScene(e.state()) && e.state().floorIndex==0 && !e.state().completed,"segment returned home before shelter");
    check(e.state().gameplay.life==LifePhase::Explore && e.state().gameplay.meals==0,"shelter executed guild lifecycle");
    const auto position=e.state().position;const auto rng=e.state().rng.state;
    replay(e,2750);check(e.state().gameplay.sceneBeat==SceneBeat::ExpeditionRest,"shelter failed to enter rest");
    replay(e,6749);check(e.state().position==position && e.state().rng.state==rng,"shelter advanced exploration");
    replay(e,6750);replay(e,7500);
    check(e.state().floorIndex==1 && e.state().floors[1].dungeonId==15 && !e.state().completed,"shelter did not advance to next segment");
    replay(e,8500);stairs(e);replay(e,8501);replay(e,9501);
    check(e.state().floorIndex==2 && e.state().population.entities.size()==1 && e.state().population.entities[0].originalId==415 && fixedBattlePending(e.state()),"terminal fixed boss was replaced by random profile");
    replay(e,10501);stairs(e);replay(e,10502);
    check(!e.state().completed && e.state().gameplay.sceneBeat!=SceneBeat::FloorFadeOut,"boss could be skipped by stairs");
    auto s=e.state();s.target=-1;s.goalKind=GoalKind::None;s.population.entities[0].done=true;
    s.onceKeys.insert("greet:9200:2:1");e=Explorer(std::move(s));stairs(e);
    replay(e,11000);replay(e,12000);
    check(e.state().completed && e.state().expedition.outcome==ExpeditionOutcome::Success,"final segment did not complete expedition");
    for(unsigned beat=0;beat<8 && e.state().gameplay.life==LifePhase::Pose;++beat)replay(e,e.state().gameplay.deadlineMs);
    check(e.state().gameplay.life==LifePhase::ReturnHome && e.state().variety.completedExplorations==1,"terminal success did not return exactly once");
    replay(e,e.state().gameplay.deadlineMs);check(e.state().gameplay.life==LifePhase::Eat && e.state().gameplay.meals==1,"final return did not start one meal");
    auto voluntary=fixture();voluntary.resetExploration(1001);
    check(voluntary.state().expedition.outcome==ExpeditionOutcome::VoluntaryReturn && !voluntary.state().completed && !voluntary.state().variety.completedExplorations,"voluntary return counted as success");
    auto broken=fixture().state();broken.expedition.floors[1].dungeonId=18;
    bool rejected=false;try{Explorer invalid(broken);}catch(const std::exception&){rejected=true;}
    check(rejected,"broken expedition binding accepted");
}
const std::vector<unsigned> roots{14,17,1,4,21,35,41,8};
ExplorationPackage packageAt(const std::filesystem::path& root,unsigned id) {
    return loadExplorationPackage(root/"assets/original/exploration"/("dungeon-"+std::to_string(id))/"exploration.mdpe");
}
Explorer original(const ExplorationPackage& p) {
    Explorer e(9300,41,p.assetVersion,p.floors,{0,0,0,0},{},p.expedition);e.enableGameplay(1000);if(e.state().expedition.entryPending)e.advanceGameplay(1000);return e;
}
void settle(Explorer& e) {
    unsigned limit=0;
    while(e.state().gameplay.sceneBeat!=SceneBeat::None && e.state().gameplay.life==LifePhase::Explore) {
        check(++limit<64,"ground/transition did not terminate");replay(e,e.state().gameplay.deadlineMs);
    }
}
void finishFixedForTransitionTest(Explorer& e) {
    auto s=e.state();
    for(auto& actor:s.population.entities)if(!actor.done) {
        check(actor.kind==EntityKind::Friend,"unexpected fixed item in selected transition fixture");
        actor.done=true;actor.remainingHits=0;s.onceKeys.insert("greet:"+std::to_string(s.runId)+":"+std::to_string(s.floorIndex)+":"+std::to_string(actor.instanceId));
    }
    s.target=-1;s.goalKind=GoalKind::None;s.targetInstance=0;s.gameplay.movement={};s.gameplay.partyActions.clear();s.gameplay.partyPlaybacks.clear();
    e=Explorer(std::move(s));
}
void terminalWalk(const std::filesystem::path& root,const std::filesystem::path& out={}) {
    for(auto id:roots)for(bool party:{false,true}) {
        auto e=original(packageAt(root,id));
        while(e.state().floorIndex+1<e.state().floors.size()) {
            settle(e);finishFixedForTransitionTest(e);stairs(e);
            replay(e,e.state().gameplay.deadlineMs+1);replay(e,e.state().gameplay.deadlineMs);
        }
        settle(e);finishFixedForTransitionTest(e);
        if(party)e.enableCompanion();
        // Reproduce an existing terminal save still aiming at the old exit,
        // with exhausted target retries and remembered stairs.
        auto stale=e.state();stale.target=stale.world.stairs;stale.goalKind=GoalKind::Stairs;
        stale.discovered=stale.world.terrain;stale.knownStairs=stale.world.stairs;
        stale.failedTargets[stale.world.stairs]=3;
        e=Explorer(decodeSnapshot(encodeSnapshot(stale),stale.assetVersion));
        const int center=floorExitTile(e.state()),start=e.state().position;
        check(center!=start,"terminal objective coincides with entrance");
        if(!out.empty() && id==14 && party) {
            std::filesystem::create_directories(out);
            const auto now=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            const auto then=e.state().gameplay.phaseStartedMs;e.shiftPausedClocks(now>then?now-then:0,then);
            saveSnapshot(out/"terminal-walk-14.mdes",e.state());
        }
        auto now=e.state().gameplay.deadlineMs;
        for(unsigned tick=0;tick<3000 && !e.state().completed;++tick) {
            now+=250;replay(e,now);
        }
        check(e.state().completed && e.state().position==center,"natural terminal walk did not clear at center");
        check(e.state().floorSteps>0,"terminal clear teleported instead of walking");
        for(unsigned beat=0;beat<8 && e.state().gameplay.life==LifePhase::Pose;++beat)replay(e,e.state().gameplay.deadlineMs);
        check(e.state().gameplay.life==LifePhase::ReturnHome && e.state().variety.completedExplorations==1,"center clear did not return exactly once");
        replay(e,e.state().gameplay.deadlineMs);
        check(e.state().gameplay.life==LifePhase::Eat && e.state().gameplay.meals==1,"center return did not start meal");
        std::cout<<"terminal center walk "<<id<<" party="<<party<<" passed\n";
    }
}
void references(const std::filesystem::path& root) {
    const auto npcs=loadNpcCatalogue(root/"assets/original/gameplay/npcs/characters.mdnc");
    const auto maps=loadEventRegistry(root/"assets/original/gameplay/events/contexts.mder");
    check(maps.maps.size()==8,"automatic pool includes unselected/test/branch maps");
    unsigned floors=0;
    for(auto id:roots) {
        const auto p=packageAt(root,id);check(p.expedition.enabled() && p.floors.front().dungeonId==id,"selected route identity mismatch");
        check(p.romHash==maps.romHash && p.romHash==npcs.romHash,"original asset identities differ");validateNpcCoverage(npcs,p.floors);
        for(unsigned f=0;f<p.floors.size();++f) {
            auto s=original(p).state();s.floorIndex=f;
            check(maps.floor(s)!=nullptr,"segment event contexts missing");
            for(const auto* rows:{&p.expedition.floors[f].fixedSpawns,&p.expedition.floors[f].revisitSpawns})for(const auto& spawn:*rows)if(spawn.md)(void)npcs.select(spawn.md);
        }
        floors+=unsigned(p.floors.size());
    }
    for(auto id:{9U,11U,13U})validateNpcCoverage(npcs,packageAt(root,id).floors);
    std::cout<<"8 routes, "<<floors<<" selected logical floors, original NPC/context references and 3 sidepaths passed\n";
}
void routes(const std::filesystem::path& root,unsigned only=0) {
    for(auto id:roots)for(bool revisit:{false,true}) {
        if(only && only!=id)continue;
        const auto p=packageAt(root,id);auto e=original(p);check(e.setExpeditionRevisit(revisit),"cannot select revisit before first fixed room");
        unsigned count=0;
        while(!e.state().completed) {
            settle(e);if(++count>p.floors.size())throw std::runtime_error("route looped id="+std::to_string(id)+" floor="+std::to_string(e.state().floorIndex)+" life="+lifeName(e.state().gameplay.life)+" beat="+std::to_string(unsigned(e.state().gameplay.sceneBeat))+" target="+std::to_string(e.state().target));
            check(e.state().gameplay.life==LifePhase::Explore && !e.state().gameplay.meals && !e.state().variety.completedExplorations,"intermediate segment executed final lifecycle");
            if(e.state().expedition.floors[e.state().floorIndex].fixedFloorId) {
                const auto& f=e.state().expedition.floors[e.state().floorIndex];const auto& source=revisit && f.revisitFixedFloorId?f.revisitSpawns:f.fixedSpawns;
                check(e.state().population.entities.size()==source.size(),"fixed encounter population differs from original placement");
                for(unsigned i=0;i<source.size();++i)check(e.state().population.entities[i].originalId==source[i].md,"fixed/revisit boss MD changed");
                finishFixedForTransitionTest(e);
            }
            stairs(e);replay(e,e.state().gameplay.deadlineMs+1);replay(e,e.state().gameplay.deadlineMs);
        }
        check(count==p.floors.size(),"route skipped an original floor/segment");
        for(unsigned i=0;i<8 && e.state().gameplay.life==LifePhase::Pose;++i)replay(e,e.state().gameplay.deadlineMs);
        replay(e,e.state().gameplay.deadlineMs);
        check(e.state().gameplay.life==LifePhase::Eat && e.state().gameplay.meals==1 && e.state().variety.completedExplorations==1,"terminal lifecycle was omitted/duplicated");
        std::cout<<p.expedition.routeId<<" revisit="<<revisit<<" transition/snapshot lifecycle passed\n";
    }
}
void branches(const std::filesystem::path& root) {
    auto e=original(packageAt(root,8));
    for(unsigned step=0;step<400 && e.state().expedition.branches.size()<3;++step) {
        if(e.state().gameplay.sceneBeat==SceneBeat::ExpeditionRest) {
            for(unsigned id:{9U,11U,13U})if(e.canTakeExpeditionBranch(id)) {
                const auto p=packageAt(root,id);const auto ground=expeditionStop(e.state())->groundId;
                check(e.takeExpeditionBranch(p.floors,p.expedition,e.state().gameplay.phaseStartedMs+1),"sidepath selection failed");
                check(!e.canTakeExpeditionBranch(id) && expeditionStop(e.state())->groundId==ground,"branch duplicated or changed its anchor ground");
                Explorer restored(decodeSnapshot(encodeSnapshot(e.state()),e.state().assetVersion));
                check(restored.state().expedition.branches==e.state().expedition.branches,"sidepath state not restored");
            }
        }
        if(e.state().gameplay.sceneBeat!=SceneBeat::None){replay(e,e.state().gameplay.deadlineMs);continue;}
        stairs(e);replay(e,e.state().gameplay.deadlineMs+1);
    }
    check(e.state().expedition.branches==std::vector<unsigned>({9,11,13}),"sidepaths were offered at the wrong junctions");
    check(e.state().floors.size()==65 && !e.state().completed && e.state().gameplay.meals==0,"sidepath returned to guild or skipped route");
    std::cout<<"all 3 sidepaths return to the same ground and retain forward route/save state\n";
}
void fixtures(const std::filesystem::path& root,const std::filesystem::path& out) {
    std::filesystem::create_directories(out);
    const auto now=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    for(unsigned id:{14U,17U,41U,8U}) {
        auto p=packageAt(root,id);auto e=original(p);
        bool savedStop=false;
        while(!e.state().completed) {
            if(expeditionScene(e.state()) && !savedStop) {
                auto s=e.state();s.gameplay.sceneBeat=SceneBeat::ExpeditionRest;s.gameplay.phaseStartedMs=now;s.gameplay.deadlineMs=now+4000;
                saveSnapshot(out/("stop-"+std::to_string(id)+".mdes"),s);savedStop=true;
            }
            settle(e);
            if(e.state().expedition.floors[e.state().floorIndex].fixedFloorId) {
                auto s=e.state();s.gameplay.phaseStartedMs=s.gameplay.deadlineMs=now;
                saveSnapshot(out/("boss-"+std::to_string(id)+".mdes"),s);finishFixedForTransitionTest(e);
            }
            stairs(e);replay(e,e.state().gameplay.deadlineMs+1);replay(e,e.state().gameplay.deadlineMs);
        }
        auto s=e.state();s.gameplay.phaseStartedMs=now;s.gameplay.deadlineMs=now+750;
        saveSnapshot(out/("ending-"+std::to_string(id)+".mdes"),s);
    }
}
}
int main(int argc,char** argv){try{
    if(argc==6 && std::string(argv[1])=="--rebase") {
        const auto p=packageAt(argv[2],unsigned(std::stoul(argv[3])));Explorer e(loadSnapshot(argv[4],p.assetVersion));
        const auto now=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        const auto then=e.state().gameplay.phaseStartedMs;e.shiftPausedClocks(now>then?now-then:0,then);saveSnapshot(argv[5],e.state());return 0;
    }
    if(argc>=3 && std::string(argv[1])=="--terminal-walk"){terminalWalk(argv[2],argc>3?argv[3]:std::filesystem::path{});return 0;}
    if(argc>=3 && std::string(argv[1])=="--references"){references(argv[2]);return 0;}
    if(argc==4 && std::string(argv[1])=="--route"){routes(argv[2],unsigned(std::stoul(argv[3])));return 0;}
    if(argc>=3 && std::string(argv[1])=="--branches"){branches(argv[2]);return 0;}
    if(argc>=3 && std::string(argv[1])=="--packages"){references(argv[2]);routes(argv[2]);branches(argv[2]);return 0;}
    if(argc==4 && std::string(argv[1])=="--fixtures"){fixtures(argv[2],argv[3]);return 0;}
    quick();std::cout<<"expedition minimal transition, shelter, fixed boss, terminal return and snapshot replay passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
