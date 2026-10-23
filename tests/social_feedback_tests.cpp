#include "core/dialogue_catalogue.hpp"
#include "core/gameplay_notice.hpp"
#include "event_test_fixture.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <set>
#include <stdexcept>
using namespace pet;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
void step(Explorer& e,uint64_t now,const DialogueCatalogue& c) {
    Explorer restored(decodeSnapshot(encodeSnapshot(e.state()),e.state().assetVersion));
    e.advanceGameplay(now,ActorCompletion{true,true,true},c);
    restored.advanceGameplay(now,ActorCompletion{true,true,true},c);
    check(encodeSnapshot(e.state())==encodeSnapshot(restored.state()),"social scene diverges after snapshot restore");
}
int main(int argc,char** argv){try {
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path();
    const auto assets=root/"assets/original";
    const auto c=loadDialogueCatalogue(assets/"gameplay/dialogue/catalogue.mdlc");
    const auto p=loadExplorationPackage(assets/"exploration/dungeon-15/exploration.mdpe");
    auto base=eventFixture(p);base.population.entities.clear();
    // Both actors may use the other's held berry in either runtime path.
    for(bool independent:{false,true})for(bool receiverPartner:{false,true}) {
        auto s=base;auto& g=s.gameplay;g.independentParty=independent;
        g.oran=receiverPartner?1:0;g.companion.oran=receiverPartner?0:1;
        g.hearts=receiverPartner?5:1;g.companion.hearts=receiverPartner?1:5;
        Explorer e(s);for(unsigned tick=0;tick<12;++tick)step(e,1000+tick*250,c);
        check(e.state().gameplay.hearts==5 && e.state().gameplay.companion.hearts==5 &&
            !e.state().gameplay.oran && !e.state().gameplay.companion.oran,"team berry did not heal empty-handed recipient exactly once");
    }
    // Every item has three distinct, stable reactions and avoids immediate repetition.
    for(const auto& clip:loadPackage(assets/"gameplay/items/items.mdpa").clips) {
        std::set<std::wstring> choices;auto s=base;unsigned previous=3;
        for(unsigned i=0;i<24;++i) {
            s.variety.notices.clear();queueGameplayNotice(s,NoticeKind::ItemComment,clip.id);updateGameplayNotices(s,1000);
            const auto n=s.variety.notices.front();check(n.variant!=previous,"item comment immediately repeated");previous=n.variant;
            choices.insert(gameplayNoticeText(s));const auto before=gameplayNoticeText(s);
            auto restored=decodeSnapshot(encodeSnapshot(s),s.assetVersion);restoreGameplayNoticeClock(restored,50000);
            check(gameplayNoticeText(restored)==before,"restore changed chosen item reaction");s=std::move(restored);
        }
        check(choices.size()==3,"selected item lacks three reachable reactions");
    }
    {auto s=base;queueGameplayNotice(s,NoticeKind::Victory,0);check(s.variety.notices.empty(),"victory queue still produces text");
     s.variety.notices={{NoticeKind::Victory,0,1000,2000}};updateGameplayNotices(s,1100);check(s.variety.notices.empty(),"old victory notice not removed");}
    for(unsigned id:{1U,4U,8U,14U,17U,21U,35U,41U}) {
        auto package=loadExplorationPackage(assets/"exploration"/("dungeon-"+std::to_string(id))/"exploration.mdpe");
        Explorer e(9300,41,package.assetVersion,package.floors,{0,0,0,0},{},package.expedition);e.enableGameplay(1000);
        if(e.state().expedition.entryPending)e.advanceGameplay(1000);
        for(unsigned f=0;e.state().floorIndex+1<package.floors.size() && f<256;++f) {
            for(unsigned n=0;e.state().gameplay.sceneBeat!=SceneBeat::None && n<40;++n)e.advanceGameplay(e.state().gameplay.deadlineMs,true);
            if(e.state().floorIndex+1==package.floors.size())break;
            auto s=e.state();s.position=s.world.stairs;s.visited[s.position]=1;s.discovered=s.world.terrain;
            s.target=s.world.stairs;s.goalKind=GoalKind::Stairs;s.targetInstance=0;s.gameplay.movement={};
            e=Explorer(s);e.advanceGameplay(s.gameplay.deadlineMs+1,true);e.advanceGameplay(e.state().gameplay.deadlineMs,true);
        }
        for(unsigned n=0;e.state().gameplay.sceneBeat!=SceneBeat::None && n<40;++n)e.advanceGameplay(e.state().gameplay.deadlineMs,true);
        e.enableCompanion();auto s=e.state();auto& g=s.gameplay;
        check(!s.population.entities.empty(),"selected terminal has no boss");
        unsigned leader=0;
        for(auto& actor:s.population.entities)if(actor.kind==EntityKind::Friend && std::find(g.hostile.begin(),g.hostile.end(),actor.instanceId)!=g.hostile.end()) {
            actor.remainingHits=0;actor.done=true;s.onceKeys.insert("greet:"+std::to_string(s.runId)+":"+std::to_string(s.floorIndex)+":"+std::to_string(actor.instanceId));
            if(!leader)leader=actor.instanceId;
        }
        check(leader!=0,"selected terminal has no hostile boss");
        auto& boss=*std::find_if(s.population.entities.begin(),s.population.entities.end(),[&](const auto& a){return a.instanceId==leader;});
        boss.done=false;s.onceKeys.erase("greet:"+std::to_string(s.runId)+":"+std::to_string(s.floorIndex)+":"+std::to_string(leader));
        g.phaseStartedMs=g.deadlineMs=1000;g.life=LifePhase::Explore;g.sceneBeat=SceneBeat::None;g.partyPhase=PartyPhase::Player;
        g.partyActions.clear();g.partyPlaybacks={{g.nextPartyId++,g.actionRound,1000,1500,leader,33,4}};
        e=Explorer(s);step(e,1500,c);
        check(bossConversation(e.state()) && !e.state().completed && dialogueContext(e.state())=="boss.befriend" &&
            cataloguePortraits(e.state(),c).speaker==PortraitRole::Friendly,"final boss did not speak after faint");
        if(argc==3 && std::string(argv[1])=="--fixtures" && id==14) {
            std::filesystem::create_directories(argv[2]);auto saved=e.state();
            const auto now=uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
            e.shiftPausedClocks(now-1500,1500);saveSnapshot(std::filesystem::path(argv[2])/"boss-talk.mdes",e.state());
            e=Explorer(saved);
        }
        for(unsigned n=0;bossConversation(e.state()) && n<32;++n)step(e,e.state().gameplay.deadlineMs,c);
        check(bossFriendship(e.state()) && e.state().gameplay.hostile.empty(),"boss friendship not committed");
        auto now=e.state().gameplay.deadlineMs;
        for(unsigned n=0;!e.state().completed && n<800;++n){now+=250;step(e,now,c);}
        check(e.state().completed && bossFriendship(e.state()),"friendship did not lead to terminal clear");
        for(unsigned n=0;e.state().gameplay.life==LifePhase::Pose && n<8;++n)step(e,e.state().gameplay.deadlineMs,c);
        check(e.state().gameplay.life==LifePhase::ReturnHome && e.state().variety.completedExplorations==1,"boss dialogue omitted or duplicated return");
        const auto friendKey="boss-friend:"+std::to_string(boss.originalId);
        check(e.state().gameplay.onceKeys.count(friendKey),"boss relationship was not recorded");
        for(unsigned n=0;e.state().runId==9300 && n<32;++n)step(e,e.state().gameplay.deadlineMs,c);
        check(e.state().runId==9301 && e.state().gameplay.onceKeys.count(friendKey),"next day lost boss relationship");
        std::cout<<"boss friendship -> center clear -> return "<<id<<" passed\n";
    }
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
