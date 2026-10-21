#include "core/dialogue_catalogue.hpp"
#include "event_test_fixture.hpp"
#include <iostream>
#include <stdexcept>
using namespace pet;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(){try {
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path()/"assets/original";
    const auto package=loadExplorationPackage(root/"exploration/dungeon-15/exploration.mdpe");
    const auto catalogue=loadDialogueCatalogue(root/"gameplay/dialogue/catalogue.mdlc");
    {const auto items=loadPackage(root/"gameplay/items/items.mdpa");
     for(unsigned dungeon:{0U,15U,18U})for(const auto& floor:loadExplorationPackage(root/"exploration"/("dungeon-"+std::to_string(dungeon))/"exploration.mdpe").floors) {
         for(const auto& category:floor.floorItems)if(category.weight)for(const auto& item:category.items)if(item.value)check(!items.frames(item.originalId,0).empty(),"spawnable item has no original sprite");
         for(auto id:floor.guaranteed)check(!items.frames(id,0).empty(),"guaranteed item has no original sprite");}}
    {auto intro=eventFixture(package);intro.gameplay.sceneBeat=SceneBeat::ExploreFadeIn;intro.gameplay.phaseStartedMs=1000;intro.gameplay.deadlineMs=4000;
     check(!explorationTitle(intro).empty() && explorationTitleOpacity(intro,1000)==0 && explorationTitleOpacity(intro,2200)==255 && explorationTitleOpacity(intro,3800)<255,"map name fade/read interval missing");
     intro.gameplay.sceneBeat=SceneBeat::FloorFadeIn;check(explorationTitle(intro).empty(),"map name repeated every floor");
     intro.gameplay.sceneBeat=SceneBeat::ExploreFadeIn;intro.variety.schedule.destination=DayDestination::Town;check(explorationTitle(intro).empty(),"dungeon name leaked into holiday");}
    auto base=eventFixture(package);base.gameplay.independentParty=true;
    {auto s=base;s.population.entities={{1,237,EntityKind::Friend,159,false},{2,83,EntityKind::Friend,183,false}};s.gameplay.hostile={1};
     Explorer live(s);live.advanceGameplay(1000,ActorCompletion{true,true,true});
     check(live.state().population.entities[0].tile!=159 && live.state().population.entities[1].tile!=183,"unseen enemy or friendly failed to patrol");
     check(live.state().world.room[live.state().position]==0 && live.state().population.entities[0].tracking==Tracking::Idle,"patrol required entering enemy room");
     auto snapshot=decodeSnapshot(encodeSnapshot(live.state()),package.assetVersion);Explorer restored(snapshot);
     for(unsigned n=0;n<12;++n){const auto now=nextGameplayDeadline(live.state());live.advanceGameplay(now,ActorCompletion{true,true,true});restored.advanceGameplay(now,ActorCompletion{true,true,true});
       check(encodeSnapshot(live.state())==encodeSnapshot(restored.state()),"patrol changed across exact restore");}}
    for(unsigned hp:{1U,5U})for(bool bothAdjacent:{false,true}) {
        auto s=base;s.position=bothAdjacent?76:102;s.gameplay.companion.tile=101;s.gameplay.hearts=hp;
        s.visited[s.position]=1;s.population.entities={{1,237,EntityKind::Friend,100,false}};s.population.entities[0].direction=2;s.gameplay.hostile={1};
        auto& g=s.gameplay;g.partyPhase=PartyPhase::OrderedEnemies;g.playerActionRound=g.companion.actionRound=g.actionRound;
        Explorer live(s);live.advanceGameplay(1000,ActorCompletion{true,true,true});
        const auto& actions=live.state().gameplay.partyActions;
        check(!actions.empty() && actions.front().actorId==1 && actions.front().kind==PartyActionKind::Attack && actions.front().targetId==kCompanionActorId,"enemy ignored companion in front or preferred low HP leader");
        (void)encodeSnapshot(live.state());
    }
    {auto s=base;s.position=103;s.visited[s.position]=1;s.gameplay.companion.tile=104;s.population.entities={{1,237,EntityKind::Friend,100,false}};s.gameplay.hostile={1};
     auto& g=s.gameplay;g.partyPhase=PartyPhase::OrderedEnemies;g.playerActionRound=g.companion.actionRound=g.actionRound;
     Explorer live(s);live.advanceGameplay(1000,ActorCompletion{true,true,true});
     check(live.state().population.entities[0].tile!=100 && live.state().gameplay.partyPlaybacks.empty(),"enemy attacked an absent adjacent target instead of moving");}
    for(unsigned hp=1;hp+2<=kMaxHp;++hp) {
        auto s=base;s.population.entities.clear();s.gameplay.hearts=s.gameplay.companion.hearts=hp;s.gameplay.oran=s.gameplay.companion.oran=1;
        Explorer live(s);live.advanceGameplay(1000,ActorCompletion{true,true,true});
        check(live.state().gameplay.hearts==kMaxHp && live.state().gameplay.companion.hearts==kMaxHp && !live.state().gameplay.oran && !live.state().gameplay.companion.oran,"Oran did not fully heal both actors");
        auto saved=decodeSnapshot(encodeSnapshot(live.state()),package.assetVersion);Explorer restored(saved);restored.advanceGameplay(1000,ActorCompletion{});
        check(!restored.state().gameplay.oran && !restored.state().gameplay.companion.oran,"restored healing consumed twice");
    }
    for(bool morning:{false,true}) {
        auto s=base;s.phase=Phase::Home;auto& g=s.gameplay;g.homeScene=true;g.hearts=0;
        g.life=morning?LifePhase::Ready:LifePhase::Sleep;g.sceneBeat=morning?SceneBeat::MorningFace:SceneBeat::BedFace;
        g.onceKeys={"return:1300","meal:1300"};g.meals=1;
        Explorer live(s);live.advanceGameplay(1000,ActorCompletion{},catalogue);
        check(live.state().variety.dialogue.active && !catalogueText(live.state(),catalogue).empty(),"home greeting lost after defeat");
        (void)encodeSnapshot(live.state());
    }
    for(unsigned hp:{1U,2U}) {
        auto s=base;auto& g=s.gameplay;g.life=LifePhase::Dialogue;g.enemyInstance=1;g.hearts=g.companion.hearts=hp;
        Explorer live(s);live.advanceGameplay(1000,ActorCompletion{},catalogue);
        check(live.state().variety.dialogue.active && !catalogueText(live.state(),catalogue).empty(),"wounded friendly greeting rendered an empty panel");
    }
    {auto s=base;s.population.entities.clear();s.floorSteps=96;Explorer live(s);live.advanceGameplay(1000,ActorCompletion{true,true,true},catalogue);
     check(live.state().gameplay.life==LifePhase::Dialogue,"ordinary floor never offered a companion chat");
     for(unsigned n=0;live.state().gameplay.life==LifePhase::Dialogue && n<20;++n)live.advanceGameplay(nextGameplayDeadline(live.state()),ActorCompletion{true,true,true},catalogue);
     auto after=live.state();after.gameplay.partyActions.clear();after.gameplay.partyPlaybacks.clear();after.gameplay.partyPhase=PartyPhase::Idle;after.gameplay.phaseStartedMs=after.gameplay.deadlineMs=10000;after.floorSteps=120;
     Explorer shortWalk(after);shortWalk.advanceGameplay(10000,ActorCompletion{true,true,true});check(shortWalk.state().gameplay.life!=LifePhase::Dialogue,"companion chat repeated after a short walk");
     after.floorSteps=256;Explorer longWalk(after);longWalk.advanceGameplay(10000,ActorCompletion{true,true,true});check(longWalk.state().gameplay.life==LifePhase::Dialogue,"long floor was capped to one chat");}
    std::cout<<"unseen/friendly patrol+restore; front adjacent targeting/HP independence; move to contact; full Oran; defeated home/wounded NPC text; sparse but repeatable long-floor chat pass\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
