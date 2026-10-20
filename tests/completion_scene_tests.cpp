#include "core/completion_scene.hpp"
#include "core/exploration_package.hpp"
#include "core/gameplay_scene.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>
using namespace pet;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
Explorer lastStairs(const ExplorationPackage& package,std::uint64_t now,bool party) {
    Explorer e(980,42,package.assetVersion,package.floors,{0,0,0,0});
    while(e.state().floorIndex+1<e.state().floors.size()) {
        auto s=e.state();s.position=s.world.stairs;s.visited[s.position]=1;e=Explorer(s);e.advance();
    }
    auto s=e.state();s.position=s.world.stairs;s.visited[s.position]=1;s.population.entities.clear();
    e=Explorer(s);e.enableGameplay(now);if(party)e.enableCompanion();return e;
}
void step(Explorer& e,std::uint64_t now,ActorCompletion done={}) {
    Explorer copy(decodeSnapshot(encodeSnapshot(e.state()),e.state().assetVersion));
    const auto a=e.advanceGameplay(now,done),b=copy.advanceGameplay(now,done);
    check(a.event==b.event && a.onceKey==b.onceKey && encodeSnapshot(e.state())==encodeSnapshot(copy.state()),"completion replay changed state/journal/RNG");
    validateState(e.state());
}
int main(int argc,char** argv){try {
    if(argc==5 && std::string(argv[1])=="--fixtures") {
        const auto package=loadExplorationPackage(argv[2]);const std::filesystem::path out=argv[3];
        const auto now=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        auto e=lastStairs(package,now,true);std::filesystem::create_directories(out);
        saveSnapshot(out/argv[4],e.state());return 0;
    }
    check(argc==4,"usage: completion registry dungeon15 dungeon0");
    const auto registry=loadCompletionRegistry(argv[1]);
    const auto* end=registry.destination(15);check(end && end->terminalId==16,"Steam Cave Peak identity lost");
    check(end->playerEntry.x==344 && end->playerEntry.y==364 && end->playerEnd.y==228 && end->partnerEnd.x==304,"original scene actor coordinates lost");
    check(!registry.destination(0),"Test Dungeon gained invented ending");
    check(registry.mapLabel(15,0,false)=="[뜨거운 동굴 상층] - 1F" && registry.mapLabel(15,6,true)=="[뜨거운 동굴 정상] - 7F","HUD map/floor format incorrect");
    for(const auto* path:{argv[2],argv[3]})for(bool party:{false,true}) {
        const auto package=loadExplorationPackage(path);check(package.romHash==registry.romHash,"ending source differs from dungeon ROM");
        auto e=lastStairs(package,1000,party);const auto inventory=e.state().inventory.size();
        step(e,1000);check(e.state().gameplay.sceneBeat==SceneBeat::FloorFadeOut,"last stairs failed to fade out");
        step(e,1999);check(!e.state().completed,"dungeon completed before transition");
        step(e,2000);check(e.state().completed && e.state().gameplay.sceneBeat==SceneBeat::CompletionFadeIn,"last floor skipped destination");
        const auto clear=e.state().onceKeys;const auto rng=e.state().rng.state;const auto feet=e.state().position;
        for(const auto expected:{SceneBeat::CompletionFadeIn,SceneBeat::CompletionWalk,SceneBeat::CompletionPose,SceneBeat::CompletionText,SceneBeat::CompletionFadeOut}) {
            const auto& g=e.state().gameplay;check(g.sceneBeat==expected,"completion beat skipped");
            check(dialoguePortraits(g).speaker==PortraitRole::None,"completion narration gained speaker portrait");
            if(expected==SceneBeat::CompletionText)check(gameplayText(g)==L"탐험을 성공적으로 마쳤다","success wording changed");
            else check(gameplayText(g).empty(),"success text appeared early or leaked into fade");
            if(expected==SceneBeat::CompletionWalk) {
                const auto start=completionPosition(*end,g,g.phaseStartedMs),mid=completionPosition(*end,g,g.phaseStartedMs+1000),finish=completionPosition(*end,g,g.deadlineMs);
                check(start.y==364 && mid.y==296 && finish.y==228 && gameplayClip(g)==0,"original entry walking path wrong");
            }
            if(expected==SceneBeat::CompletionPose) {
                step(e,g.phaseStartedMs+1,{true,false,false});
                if(party)check(e.state().gameplay.sceneBeat==expected,"completion pose ignored partner");
                if(!party) { // Single-actor pose may finish before its timeout.
                    check(e.state().gameplay.sceneBeat==SceneBeat::CompletionText,"solo pose did not finish");
                    continue;
                }
            }
            else {
                step(e,g.deadlineMs-1,{true,true,true});
                check(e.state().gameplay.sceneBeat==expected,"finite clip compressed completion timeline");
            }
            const auto deadline=e.state().gameplay.deadlineMs;step(e,deadline,{true,true,true});
        }
        check(e.state().gameplay.life==LifePhase::ReturnHome && e.state().gameplay.homeScene && e.state().gameplay.fadeInOnly,"success did not fade directly home");
        check(e.state().onceKeys==clear && e.state().rng.state==rng && e.state().position==feet && e.state().inventory.size()==inventory,"ending changed logical progress or duplicated rewards");
        step(e,e.state().gameplay.deadlineMs);check(e.state().gameplay.life==LifePhase::Eat,"completion return did not start meal");
        const auto meal=e.state().gameplay.phaseStartedMs;const auto meals=e.state().gameplay.meals;
        auto oldMeal=e.state();oldMeal.gameplay.deadlineMs=oldMeal.gameplay.phaseStartedMs+7000;
        const auto migrated=decodeSnapshot(encodeSnapshot(oldMeal),oldMeal.assetVersion);
        check(migrated.gameplay.deadlineMs==meal+6000,"old seven-second meal snapshot retained its tail");
        step(e,meal+5999,{true,true,true});check(e.state().gameplay.life==LifePhase::Eat,"meal compressed/skipped by animation completion");
        step(e,meal+6000);check(e.state().gameplay.sceneBeat==SceneBeat::DiningExit && e.state().gameplay.meals==meals,"meal did not cut tail at six seconds");
        Explorer middle(981,42,package.assetVersion,package.floors,{0,0,0,0});auto s=middle.state();s.position=s.world.stairs;s.visited[s.position]=1;s.population.entities.clear();
        middle=Explorer(s);middle.enableGameplay(1000);step(middle,1000);step(middle,2000);
        check(!completionScene(middle.state().gameplay) && middle.state().floorIndex==1 && gameplayText(middle.state().gameplay).empty(),"intermediate stairs played success");
        middle.resetExploration(2001);step(middle,2751);check(!completionScene(middle.state().gameplay) && gameplayText(middle.state().gameplay).empty(),"manual return played success");
        auto legacy=lastStairs(package,1000,false);step(legacy,1000);step(legacy,2000);s=legacy.state();s.gameplay.sceneBeat=SceneBeat::None;
        legacy=Explorer(s);step(legacy,2001,{true,true,true});check(legacy.state().gameplay.sceneBeat==SceneBeat::CompletionFadeIn,"legacy clear save skipped ending");
    }
    std::cout<<"completion destinations, HUD identities, solo/party timeline, snapshot replay, tail-trimmed meals and non-clear returns passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
