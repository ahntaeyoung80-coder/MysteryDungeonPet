#include "core/event_runtime.hpp"
#include "event_test_fixture.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace pet;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){try {
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path()/"assets/original";
    const auto p=loadExplorationPackage(root/"exploration/dungeon-15/exploration.mdpe");
    const auto extra=loadExplorationPackage(root/"exploration/dungeon-18/exploration.mdpe");
    const auto c=loadDialogueCatalogue(root/"gameplay/dialogue/catalogue.mdlc");const auto maps=loadEventRegistry(root/"gameplay/events/contexts.mder");
    for(auto type:{VarietyEvent::LostPokemon,VarietyEvent::LostProperty,VarietyEvent::Reunion}) {
        auto s=eventFixture(type==VarietyEvent::Reunion?extra:p);const auto rng=s.rng.state;const auto inventory=s.inventory.size();
        const auto beforePlayer=s.position,beforePartner=s.gameplay.companion.tile;
        check(startVarietyEvent(s,type,1000,c,1,&maps),"movement event could not start");
        if(type==VarietyEvent::Reunion)check((s.position!=beforePlayer)!=(s.gameplay.companion.tile!=beforePartner),"warp moved both/no actors");
        const auto eventId=eventKey(s.variety.event.id);unsigned steps=0;bool propertySeen=false,ownerTalk=false;
        while(eventActive(s) && steps++<150) {
            s=decodeSnapshot(encodeSnapshot(s),s.assetVersion);const auto old=s;const auto at=nextGameplayDeadline(s);
            Explorer e(s);e.advanceGameplay(at,ActorCompletion{true,true,true},c,&maps);s=e.state();
            check(s.floorIndex==old.floorIndex && s.runId==old.runId && !s.completed && s.gameplay.hearts==old.gameplay.hearts,"event move triggered floor/reward/damage");
            check(s.position!=s.gameplay.companion.tile,"event party overlapped");
            if(s.position!=old.position)check(canMove(old.world,old.position,s.position,old.blocked),"route crossed a wall/corner");
            propertySeen|=s.variety.event.carrying;
            ownerTalk|=s.variety.dialogue.context=="event.E06.owner";
            if(type==VarietyEvent::LostProperty && s.variety.event.carrying)check(s.inventory.size()==inventory && stockQuantity(s,0,109)==0,"event property became spendable inventory");
        }
        if(s.variety.event.progress!=EventProgress::Finished)std::cerr<<"type="<<unsigned(type)<<" steps="<<steps<<" progress="<<unsigned(s.variety.event.progress)<<" player="<<s.position<<" partner="<<s.gameplay.companion.tile<<" target="<<s.variety.event.targetTile<<"\n";
        check(steps<150 && s.variety.event.progress==EventProgress::Finished,"movement event abandoned/stuck");
        s=decodeSnapshot(encodeSnapshot(s),s.assetVersion);
        check(s.rng.state==rng && s.inventory.size()==inventory && s.variety.committed.size()==1 && s.variety.committed.begin()->rfind(eventId+":",0)==0,"movement RNG/inventory/result once mismatch");
        if(type==VarietyEvent::LostPokemon)check(std::any_of(s.population.entities.begin(),s.population.entities.end(),[&](const auto& a){return a.instanceId==1 && !a.visible && canAttack(s.world,a.tile,s.world.stairs);}),"escort dismissed before stairs arrival");
        if(type==VarietyEvent::LostProperty)check(propertySeen && ownerTalk && !s.variety.event.carrying,"property skipped pickup/owner/return");
        if(type==VarietyEvent::Reunion)check(canAttack(s.world,s.position,s.gameplay.companion.tile),"reunion dialogue before actual adjacency");
    }
    auto s=eventFixture(p);check(startVarietyEvent(s,VarietyEvent::Reunion,1000,c,0,&maps),"reunion excluded by original zero warp weight");
    s=eventFixture(p);check(startVarietyEvent(s,VarietyEvent::LostProperty,1000,c,1,&maps),"property cancellation fixture");
    s.variety.event.carrying=true;s.variety.event.objectTile=-1;cancelVariety(s);
    check(!s.variety.event.carrying && !s.variety.event.item && s.inventory.empty() && s.variety.event.actors.empty(),"cancel converted lost property to inventory");
    s=eventFixture(p);s.population.entities.push_back({2,237,EntityKind::Friend,126,false});check(startVarietyEvent(s,VarietyEvent::LostProperty,1000,c,1,&maps),"identity fixture");
    s.population.entities[0].visible=false;(void)advanceVarietyEvent(s,1400,c,&maps);
    check(s.variety.event.progress==EventProgress::Abandoned && s.variety.committed.empty(),"same-species replacement accepted as original owner");
    std::cout<<"E02/E06/E08: physical finite routes, snapshots at every boundary, exact owner, unspendable property, shared map-independent warp/single target and actual reunion pass\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
