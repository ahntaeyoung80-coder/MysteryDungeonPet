#include "core/event_runtime.hpp"
#include "event_test_fixture.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace pet;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){try {
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path()/"assets/original";
    const auto p=loadExplorationPackage(root/"exploration/dungeon-18/exploration.mdpe");
    const auto base=loadExplorationPackage(root/"exploration/dungeon-15/exploration.mdpe");
    const auto c=loadDialogueCatalogue(root/"gameplay/dialogue/catalogue.mdlc");const auto maps=loadEventRegistry(root/"gameplay/events/contexts.mder");
    auto s=eventFixture(base);check(startVarietyEvent(s,VarietyEvent::Shop,1000,c,0,&maps),"shop excluded by original zero chance");
    s=eventFixture(base);check(startVarietyEvent(s,VarietyEvent::MonsterHouse,1000,c,0,&maps),"monster house excluded by original zero chance");
    bool traded=false;
    for(unsigned seed=0;seed<64 && !traded;++seed) {
        s=eventFixture(p);s.variety.eventRng=seed;check(changeStock(s,0,109,3),"trade fixture stock");
        check(startVarietyEvent(s,VarietyEvent::Shop,1000,c,0,&maps),"shop could not start");
        if(s.variety.event.task!=EventTask::Trade)continue;
        check(!commitShopExchange(s),"trade committed during approach");
        s.variety.event.progress=EventProgress::Result;check(commitShopExchange(s),"offered original stock did not transfer");
        const auto saved=decodeSnapshot(encodeSnapshot(s),p.assetVersion);
        check(stockQuantity(saved,0,109)==0 && saved.gameplay.oran==1 && !saved.variety.event.goods.empty(),"actual exchange quantities differ");
        s=saved;check(!commitShopExchange(s) && stockQuantity(s,0,109)==0 && s.gameplay.oran==1,"restored trade charged/credited twice");traded=true;
    }
    check(traded,"no original shop sample contained supported offer");
    finishVarietyEvent(s);s.gameplay.deadlineMs=s.gameplay.phaseStartedMs;
    s=decodeSnapshot(encodeSnapshot(s),s.assetVersion);
    for(auto type:{VarietyEvent::MonsterHouse,VarietyEvent::JointBattle}) {
        s=eventFixture(p);s.rng.state=42;s.gameplay.oran=s.gameplay.companion.oran=5;
        check(startVarietyEvent(s,type,1000,c,1,&maps),"battle event could not start");
        const auto opponents=s.variety.event.opponents;const auto key=eventKey(s.variety.event.id);const auto helper=type==VarietyEvent::JointBattle?1U:0U;
        unsigned steps=0,helperAttacks=0,allyAttacks=0;std::uint64_t now=1000;
        while(eventActive(s) && steps++<250) {
            s=decodeSnapshot(encodeSnapshot(s),p.assetVersion);ActorCompletion done;done.keyed=true;
            for(const auto& v:s.gameplay.partyPlaybacks)done.actions.push_back({s.runId,v.round,v.id,s.floorIndex,v.actorId});
            now=s.gameplay.partyPlaybacks.empty()?std::max(now,nextGameplayDeadline(s)):now+100;
            Explorer e(s);const auto update=e.advanceGameplay(now,done,c,&maps);s=e.state();
            for(const auto& a:update.actors)if(a.event==GameplayEvent::PlayerAttack || a.event==GameplayEvent::CompanionAttack || a.event==GameplayEvent::EnemyAttack) {
                const auto ally=[&](unsigned id){return id==0 || id==kCompanionActorId || (helper && id==helper);};
                check(ally(a.actorId)!=ally(a.targetId),"event battle friendly fire");
                helperAttacks+=helper && a.actorId==helper;allyAttacks+=a.actorId==0 || a.actorId==kCompanionActorId;
            }
            check(std::count_if(s.gameplay.partyPlaybacks.begin(),s.gameplay.partyPlaybacks.end(),[](const auto& v){return v.clip==1;})<=1,"event attacks overlapped");
            if(s.variety.dialogue.context=="event.E04.victory" || s.variety.dialogue.context=="event.E07.victory")
                for(auto id:opponents)check(std::any_of(s.population.entities.begin(),s.population.entities.end(),[&](const auto& a){return a.instanceId==id && a.done;}),"victory line before enemy removal");
        }
        if(s.variety.event.progress!=EventProgress::Finished)std::cerr<<"battle="<<unsigned(type)<<" steps="<<steps<<" progress="<<unsigned(s.variety.event.progress)<<" hp="<<s.gameplay.hearts<<','<<s.gameplay.companion.hearts<<" helperAttacks="<<helperAttacks<<'\n';
        check(steps<250 && s.variety.event.progress==EventProgress::Finished && s.variety.committed.count(key+":victory"),"battle did not commit actual victory");
        check(allyAttacks>0 && (!helper || helperAttacks>0),"joint helper did not use independent action");
        check(s.phase==Phase::Explore && s.gameplay.companion.enabled && s.inventory.empty(),"event fight replaced permanent party or returned home");
    }
    std::cout<<"E03 real original stock/3 apples -> 1 Oran/restore once; E04/E07 ordered attacks, actual defeat, independent helper and no friendly fire pass\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
