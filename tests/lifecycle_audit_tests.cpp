#include "core/event_runtime.hpp"
#include "core/holiday_scene.hpp"
#include "event_test_fixture.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>
using namespace pet;
namespace {
unsigned failures=0,checks=0;
void check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
void scenario(const char* name,void (*test)()) {
    try {test();std::cout<<"PASS "<<name<<'\n';}
    catch(const std::exception& error){++failures;std::cerr<<"FAIL "<<name<<": "<<error.what()<<'\n';}
}
ExplorationState home(LifePhase life,SceneBeat beat,bool partner) {
    Explorer e(4200,42,"lifecycle-audit",fixtureProfiles(1),{0,0,0,0});e.enableGameplay(1000);
    if(partner)e.enableCompanion();
    auto s=e.state();auto& g=s.gameplay;s.phase=Phase::Home;g.homeScene=true;g.life=life;g.sceneBeat=beat;
    g.phaseStartedMs=1000;g.deadlineMs=2000;g.meals=1;
    g.onceKeys.insert("return:4200");g.onceKeys.insert("meal:4200");s.variety.schedule.mealCommitted=true;
    return decodeSnapshot(encodeSnapshot(s),s.assetVersion);
}
Explorer restore(const Explorer& e){return Explorer(decodeSnapshot(encodeSnapshot(e.state()),e.state().assetVersion));}
void soloBodies() {
    for(auto beat:{SceneBeat::BedFace,SceneBeat::BedLay,SceneBeat::Sleeping,SceneBeat::Waking,SceneBeat::MorningFace}) {
        const auto life=beat==SceneBeat::Waking?LifePhase::Wake:beat==SceneBeat::MorningFace?LifePhase::Ready:LifePhase::Sleep;
        Explorer e(home(life,beat,false));e.advanceGameplay(2000,ActorCompletion{true,false,false});
        check(e.state().gameplay.sceneBeat!=beat,"absent partner blocked completed solo body");
        (void)encodeSnapshot(e.state());
    }
}
void missingBodies() {
    for(auto beat:{SceneBeat::BedFace,SceneBeat::BedLay,SceneBeat::Waking,SceneBeat::MorningFace,SceneBeat::RoomExit}) {
        const auto life=beat==SceneBeat::Waking?LifePhase::Wake:(beat==SceneBeat::MorningFace || beat==SceneBeat::RoomExit)?LifePhase::Ready:LifePhase::Sleep;
        Explorer e(home(life,beat,true));
        e.advanceGameplay(1999,ActorCompletion{});
        check(e.state().gameplay.sceneBeat==beat,"missing completion skipped minimum scene duration");
        e=restore(e);e.advanceGameplay(13000,ActorCompletion{});
        check(e.state().gameplay.sceneBeat!=beat,"late restored scene waits forever for body completion");
        (void)encodeSnapshot(e.state());
    }
}
void lostDialogueCompletion() {
    auto s=home(LifePhase::Sleep,SceneBeat::BedFace,true);
    s.phase=Phase::Explore;auto& g=s.gameplay;g.homeScene=false;g.life=LifePhase::Dialogue;g.sceneBeat=SceneBeat::None;
    g.enemyInstance=kCompanionActorId;g.dialogueStage=DialogueStage::Npc;
    // enableCompanion chooses an adjacent tile; this is a valid saved legacy dialogue.
    Explorer e(s);e.advanceGameplay(1999,ActorCompletion{});
    check(e.state().gameplay.life==LifePhase::Dialogue,"dialogue ended before its read interval");
    e=restore(e);e.advanceGameplay(13000,ActorCompletion{});
    check(e.state().gameplay.life==LifePhase::Explore,"fallback dialogue has no finite escape");
}
void fullDay() {
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path()/"assets/original";
    const auto c=loadDialogueCatalogue(root/"gameplay/dialogue/catalogue.mdlc");
    for(bool partner:{false,true}) {
        Explorer e(4200,42,"lifecycle-audit",fixtureProfiles(1),{0,0,0,0});e.enableGameplay(1000);if(partner)e.enableCompanion();
        e.startGuildMorning(1000);std::uint64_t now=1000;
        for(unsigned i=0;i<100 && e.state().phase!=Phase::Explore;++i){
            e=restore(e);now=std::max(now+1,nextGameplayDeadline(e.state()));
            e.advanceGameplay(now,ActorCompletion{true,false,partner},c);
        }
        check(e.state().phase==Phase::Explore && e.state().gameplay.day==1,"fresh morning did not start same-day exploration");
        check(e.resetExploration(now+1).event==GameplayEvent::ReturnHome,"manual return failed");
        const auto position=e.state().position;const auto steps=e.state().floorSteps;
        const auto inventory=e.state().inventory.size();const auto rng=e.state().rng.state;
        for(unsigned i=0;i<160 && e.state().gameplay.day==1;++i){
            e=restore(e);now=std::max(now+1,nextGameplayDeadline(e.state()));
            e.advanceGameplay(now,ActorCompletion{true,false,partner},c);
            if(e.state().gameplay.homeScene || e.state().gameplay.life==LifePhase::ReturnHome) {
                check(e.state().position==position && e.state().floorSteps==steps && e.state().rng.state==rng,"home pipeline ran dungeon navigation");
            }
        }
        check(e.state().gameplay.day==2 && e.state().gameplay.life==LifePhase::Explore,"home pipeline did not reach next day");
        check(e.state().gameplay.meals==1 && e.state().inventory.size()==inventory,"meal duplicated or consumed pickup history");
        const auto run=e.state().runId;e=restore(e);e.advanceGameplay(now+1,ActorCompletion{},c);
        check(e.state().runId==run && e.state().gameplay.day==2,"same tick committed another new day");
    }
}
void lostFaintCompletion() {
    const auto assets=std::filesystem::path(__FILE__).parent_path().parent_path()/"assets/original";
    const auto p=loadExplorationPackage(assets/"exploration/dungeon-15/exploration.mdpe");
    for(bool partner:{false,true}) {
        auto s=eventFixture(p);auto& g=s.gameplay;
        if(!partner)g.companion={};
        g.independentParty=false;g.life=LifePhase::Combat;
        g.combat=partner?CombatPhase::CompanionFaint:CombatPhase::PlayerFaint;
        g.enemyInstance=1;g.enemyHits=2;g.hostile={1};
        if(partner)g.companion.hearts=0;else g.hearts=0;
        g.phaseStartedMs=1000;g.deadlineMs=6000;
        Explorer e(s);e.advanceGameplay(5999,ActorCompletion{});
        check(e.state().gameplay.life==LifePhase::Combat,"faint timeout skipped finite animation interval");
        e=restore(e);const auto stock=e.state().gameplay.oran+e.state().gameplay.companion.oran;
        const auto inventory=e.state().inventory.size();
        e.advanceGameplay(6000,ActorCompletion{});
        check(e.state().gameplay.life==LifePhase::Death && e.state().phase==Phase::Home,"lost faint completion blocked defeat");
        check(e.state().gameplay.oran+e.state().gameplay.companion.oran==stock && e.state().inventory.size()==inventory,"faint fallback changed stock");
        e=restore(e);e.advanceGameplay(6001,ActorCompletion{});
        check(e.state().gameplay.life==LifePhase::Death,"duplicate faint timeout advanced twice");
    }
}
void repeatedCommit() {
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path()/"assets/original";
    const auto p=loadExplorationPackage(root/"exploration/dungeon-15/exploration.mdpe");
    const auto c=loadDialogueCatalogue(root/"gameplay/dialogue/catalogue.mdlc");
    auto s=eventFixture(p);s.gameplay.oran=s.gameplay.companion.oran=1;
    check(startVarietyEvent(s,VarietyEvent::TiredExplorer,1000,c,1),"share setup failed");
    s.variety.event.progress=EventProgress::Result;
    check(commitOranShare(s),"share did not debit");
    s=decodeSnapshot(encodeSnapshot(s),s.assetVersion);
    check(!commitOranShare(s) && s.gameplay.oran+s.gameplay.companion.oran==1,"share replay debited a second berry");
    s=eventFixture(p);s.floors[s.floorIndex].floorItems={{1,1,{{70,1}}}};check(changeStock(s,0,109,6),"apple setup failed");
    check(startVarietyEvent(s,VarietyEvent::Shop,1000,c,0),"shop setup failed");
    check(s.variety.event.task==EventTask::Trade,"shop did not select exchange");
    s.variety.event.progress=EventProgress::Result;
    check(commitShopExchange(s),"exchange did not commit");
    s=decodeSnapshot(encodeSnapshot(s),s.assetVersion);
    check(!commitShopExchange(s) && stockQuantity(s,0,109)==3 && s.gameplay.oran==1,"exchange replay consumed apples");
}
}
int main(int argc,char** argv){
    if(argc==3 && std::string(argv[1])=="--inspect") {
        try {
            const auto root=std::filesystem::path(__FILE__).parent_path().parent_path()/"assets/original";
            const auto p=loadExplorationPackage(root/"exploration/dungeon-15/exploration.mdpe");
            const auto s=loadSnapshot(argv[2],p.assetVersion);
            std::cout<<"{\"day\":"<<s.gameplay.day<<",\"meals\":"<<s.gameplay.meals
                     <<",\"life\":"<<unsigned(s.gameplay.life)<<",\"scene\":"<<unsigned(s.gameplay.sceneBeat)<<"}\n";
            return 0;
        } catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
    }
    scenario("solo body completion",soloBodies);
    scenario("missing/late body completion",missingBodies);
    scenario("fallback dialogue completion",lostDialogueCompletion);
    scenario("restored full morning/evening/next day",fullDay);
    scenario("share/exchange replay",repeatedCommit);
    scenario("lost player/partner faint completion",lostFaintCompletion);
    if(!failures && argc==3 && std::string(argv[1])=="--fixtures") {
        const auto root=std::filesystem::path(__FILE__).parent_path().parent_path()/"assets/original";
        const auto p=loadExplorationPackage(root/"exploration/dungeon-15/exploration.mdpe");
        const auto now=std::uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
        std::filesystem::create_directories(argv[2]);
        for(bool partner:{false,true}) {
            auto s=home(LifePhase::Sleep,SceneBeat::BedLay,partner);
            s.assetVersion=p.assetVersion;s.floors=p.floors;
            s.gameplay.phaseStartedMs=now;s.gameplay.deadlineMs=now+400;
            saveSnapshot(std::filesystem::path(argv[2])/(partner?"pair-night.mdes":"solo-night.mdes"),s);
        }
    }
    std::cout<<checks<<" checks, "<<failures<<" failures\n";
    return failures?1:0;
}
