#include "core/event_runtime.hpp"
#include "core/event_registry.hpp"
#include "core/npc_characters.hpp"
#include "core/completion_scene.hpp"
#include "event_test_fixture.hpp"
#include <iostream>
#include <stdexcept>
using namespace pet;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){try {
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path()/"assets/original";
    const auto p=loadExplorationPackage(root/"exploration/dungeon-15/exploration.mdpe");
    const auto c=loadDialogueCatalogue(root/"gameplay/dialogue/catalogue.mdlc");
    const auto maps=loadEventRegistry(root/"gameplay/events/contexts.mder");
    const auto npcs=loadNpcCatalogue(root/"gameplay/npcs/characters.mdnc");
    const auto extra=loadExplorationPackage(root/"exploration/dungeon-18/exploration.mdpe");
    check(maps.maps.size()==2 && maps.floors.size()==16 && extra.floors.size()==9,"selected event map/floor count");
    validateNpcCoverage(npcs,extra.floors);
    bool shop=false,house=false,warp=false;
    for(const auto& f:maps.floors) {
        for(const auto& w:f.monsterHouse)if(w.value)(void)npcs.select(w.originalId);
        if(f.dungeon==15){check(!f.shopChance && !f.monsterHouseChance,"default map special conditions fabricated");
            for(const auto& t:f.traps)if(t.originalId==6)check(!t.value,"default map warp condition fabricated");}
        if(f.dungeon==18){shop|=f.shopChance>0;house|=f.monsterHouseChance>0;
            for(const auto& t:f.traps)if(t.originalId==6 && t.value && f.trapDensity)warp=true;}
    }
    check(shop && house && warp,"new map original special conditions absent");
    const auto ends=loadCompletionRegistry(root/"gameplay/completion/destinations.mdec");
    check(ends.destination(15) && ends.destination(18) && ends.destination(18)->terminalId==19 && ends.destination(18)->assetFile=="amp-clearing.mdpa","new map completion reused wrong ground");
    const auto ground=loadPackage(root/"gameplay/completion/amp-clearing.mdpa");check(ground.frames(0,0).front().rect.w==int(ends.destination(18)->width),"completion dimensions mismatch");
    auto s=eventFixture(p);s.gameplay.oran=3;
    check(changeStock(s,0,109,4) && stockQuantity(s,0,109)==4 && stockQuantity(s,0,70)==3,"separate stock quantities");
    check(startVarietyEvent(s,VarietyEvent::TiredExplorer,1000,c,1) && s.variety.event.variant=="E01.share_a","spare berry did not select sharing");
    check(reservedQuantity(s,0,70)==1 && !changeStock(s,0,70,-3) && stockQuantity(s,0,70)==3,"reserved stock spent twice");
    check(!commitOranShare(s),"berry transferred before actual result");
    unsigned steps=0;while(eventActive(s) && steps++<80){Explorer e(decodeSnapshot(encodeSnapshot(s),p.assetVersion));e.advanceGameplay(nextGameplayDeadline(s),ActorCompletion{true,true,true},c);s=e.state();}
    check(steps<80 && s.gameplay.oran==2 && stockQuantity(s,0,109)==4 && s.variety.committed.size()==1,"sharing double debit or changed unrelated inventory");
    const auto encoded=encodeSnapshot(s);check(encoded[4]==17,"stock schema not versioned");const auto restored=decodeSnapshot(encoded,p.assetVersion);
    check(restored.gameplay.oran==2 && stockQuantity(restored,0,109)==4,"stock restore lost current quantities");
    s=eventFixture(p);s.gameplay.oran=1;check(startVarietyEvent(s,VarietyEvent::TiredExplorer,1000,c,1) && s.variety.event.variant=="E01.encourage" && s.variety.event.reservations.empty(),"last recovery berry offered");
    s=eventFixture(p);s.gameplay.companion.oran=2;check(startVarietyEvent(s,VarietyEvent::TiredExplorer,1000,c,1) && s.variety.event.variant=="E01.share_b" && reservedQuantity(s,kCompanionActorId,70)==1,"wrong stock owner reserved");
    cancelVariety(s);check(s.gameplay.companion.oran==2 && s.variety.event.reservations.empty(),"cancel consumed reserved berry");
    std::cout<<"original 18->19 completion, 16 floor contexts/all MD coverage, current stock/owner/reservation/share once and last-reserve protection pass\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
