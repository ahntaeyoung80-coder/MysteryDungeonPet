#include "core/dialogue_catalogue.hpp"
#include "core/exploration_package.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace pet;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
ExplorationState scene(const ExplorationPackage& package,const std::string& context) {
    Explorer e(1200,42,package.assetVersion,package.floors,{0,0,0,0});auto s=e.state();
    s.world.terrain.fill(0);s.world.room.fill(-1);
    for(int y=2;y<10;++y)for(int x=2;x<10;++x){s.world.terrain[y*24+x]=1;s.world.room[y*24+x]=0;}
    for(int y=2;y<10;++y)for(int x=12;x<20;++x){s.world.terrain[y*24+x]=1;s.world.room[y*24+x]=1;}
    for(int x=9;x<=12;++x)s.world.terrain[4*24+x]=1;
    s.world.centers={100,111};s.world.start=100;s.world.stairs=111;s.position=100;
    s.visited.fill(0);s.visited[s.position]=1;s.discovered=s.world.terrain;s.knownStairs=111;
    s.population.entities={{1,126,EntityKind::Friend,101,false}};
    s.target=-1;s.targetInstance=0;s.goalKind=GoalKind::None;
    auto& g=s.gameplay;g.enabled=true;g.companion.enabled=true;g.companion.tile=99;
    g.phaseStartedMs=1000;g.deadlineMs=2600;g.life=LifePhase::Dialogue;g.enemyInstance=kCompanionActorId;
    if(context=="npc.greet")g.enemyInstance=1;
    if(context=="sleep.greet"){s.phase=Phase::Home;g.homeScene=true;g.life=LifePhase::Sleep;g.sceneBeat=SceneBeat::BedFace;}
    if(context=="morning.depart"){s.phase=Phase::Home;g.homeScene=true;g.life=LifePhase::Ready;g.sceneBeat=SceneBeat::MorningFace;}
    if(context=="wake.alarm"){s.phase=Phase::Home;g.homeScene=true;g.life=LifePhase::Wake;g.sceneBeat=SceneBeat::WakeDelay;}
    if(context=="defeat.player" || context=="defeat.partner") {
        s.phase=Phase::Home;g.life=LifePhase::Death;g.deathStage=DeathStage::PartnerTalk;
        if(context=="defeat.player"){g.hearts=0;g.playerHidden=true;}else{g.companion.hearts=0;g.companion.hidden=true;}
    }
    if(g.life!=LifePhase::Dialogue)g.enemyInstance=0;
    if(g.homeScene){g.onceKeys.insert("return:1200");g.onceKeys.insert("meal:1200");g.meals=1;}
    return s;
}
int main(int argc,char** argv){try {
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path()/"assets/original";
    const auto c=loadDialogueCatalogue(root/"gameplay/dialogue/catalogue.mdlc");
    const auto package=loadExplorationPackage(root/"exploration/dungeon-15/exploration.mdpe");
    const std::vector<std::string> contexts{"companion.checkin","npc.greet","sleep.greet","wake.alarm","morning.depart","defeat.player","defeat.partner"};
    check(std::count_if(c.bundles.begin(),c.bundles.end(),[](const auto& b){return b.context.rfind("event.",0)!=0 && b.context.rfind("boss.",0)!=0;})==56,"catalogue selection count");
    for(const auto& context:contexts) {
        auto s=scene(package,context);const auto mapRng=s.rng.state,eventRng=s.variety.eventRng;
        Explorer e(s);e.advanceGameplay(1000,ActorCompletion{true,true,true},c);
        check(e.state().variety.dialogue.active,"context did not start a session");
        const auto selected=e.state().variety.dialogue.bundle;
        const auto draw=e.state().variety.dialogueRng;
        for(unsigned i=0;i<10;++i)e.advanceGameplay(1000,ActorCompletion{true,true,true},c);
        check(e.state().variety.dialogue.bundle==selected && e.state().variety.dialogueRng==draw,"frame repeat rerolled bundle");
        check(e.state().rng.state==mapRng && e.state().variety.eventRng==eventRng,"dialogue consumed gameplay/event RNG");
        auto saved=e.state();checkpointDialogue(saved,c,1100);
        auto restored=decodeSnapshot(encodeSnapshot(saved),package.assetVersion);Explorer resumed(restored);resumed.restoreDialogue(c,50000);
        check(resumed.state().variety.dialogue.bundle==selected && resumed.state().variety.dialogue.lineId==saved.variety.dialogue.lineId && resumed.state().variety.dialogueRng==draw,"restore substituted selected line");
        check(revealedCatalogueText(resumed.state(),c,50000)==revealedCatalogueText(e.state(),c,1100),"restore lost reveal progress");
        check(resumed.state().variety.dialogue.deadlineMs>50000,"restore lost remaining read time");
        const auto* line=selectedDialogue(e.state(),c);check(line && cataloguePortraits(e.state(),c).speaker==line->display.speaker,"portrait speaker mismatch");
        const auto revealEnd=1000+(line->display.text.size()-1)*25;
        check(revealedCatalogueText(e.state(),c,revealEnd)==line->display.text && e.state().variety.dialogue.deadlineMs>=revealEnd+line->holdMs,"read time starts before full text");
        unsigned steps=0;
        while(resumed.state().variety.dialogue.active && steps++<40) {
            const auto before=resumed.state().variety.dialogue.line;
            const auto at=resumed.state().variety.dialogue.deadlineMs;
            resumed.advanceGameplay(at,ActorCompletion{true,true,true},c);
            if(resumed.state().variety.dialogue.active)check(resumed.state().variety.dialogue.line==before+1 && resumed.state().variety.dialogue.bundle==selected,"page sequence changed bundle");
            (void)encodeSnapshot(resumed.state());
        }
        check(steps<40 && !resumed.state().variety.dialogue.active,"dialogue did not terminate");
        s=scene(package,context);s.variety=e.state().variety;s.variety.dialogue={};Explorer next(s);next.advanceGameplay(1000,ActorCompletion{true,true,true},c);
        check(next.state().variety.dialogue.bundle!=selected,"recent bundle repeated despite alternatives");
        if(argc==3 && std::string(argv[1])=="--fixtures") {
            std::filesystem::create_directories(argv[2]);saveSnapshot(std::filesystem::path(argv[2])/(context+".mdes"),e.state());
        }
    }
    auto alarm=scene(package,"wake.alarm");alarm.gameplay.companion.enabled=false;
    Explorer wake(alarm);wake.advanceGameplay(1000,ActorCompletion{},c);
    check(cataloguePortraits(wake.state(),c).speaker==PortraitRole::Alarm,"Loudred wake-up call missing without companion");
    auto s=scene(package,"defeat.player");s.gameplay.companion.enabled=false;
    Explorer solo(s);solo.advanceGameplay(1000,ActorCompletion{true,true,true},c);
    check(!solo.state().variety.dialogue.active && catalogueText(solo.state(),c).empty() && cataloguePortraits(solo.state(),c).speaker==PortraitRole::None,"absent solo partner spoke");
    s=scene(package,"companion.checkin");s.gameplay.hearts=1;check(!dialogueEligible(s,"companion.checkin"),"unsafe checkin eligible");
    s=scene(package,"npc.greet");s.gameplay.hostile={1};check(!dialogueEligible(s,"npc.greet"),"hostile NPC eligible for greeting");
    Explorer original(scene(package,"companion.checkin"));original.advanceGameplay(1000,ActorCompletion{true,true,true},c);
    const auto draw=original.state().variety.dialogueRng;auto newer=c;++newer.version;original.restoreDialogue(newer,2000);
    check(!original.state().variety.dialogue.active && original.state().variety.dialogueRng==draw && original.state().gameplay.life==LifePhase::Explore,"catalogue mismatch rerolled or remained stuck");
    auto one=c;one.bundles.erase(std::remove_if(one.bundles.begin(),one.bundles.end(),[](const auto& b){return b.id!="companion.checkin.01";}),one.bundles.end());
    s=scene(package,"companion.checkin");s.variety.recent={{"companion.checkin","companion.checkin.01",0}};
    Explorer single(s);single.advanceGameplay(1000,ActorCompletion{true,true,true},one);check(single.state().variety.dialogue.active,"single candidate rejected forever");
    auto none=c;none.bundles.clear();Explorer empty(scene(package,"companion.checkin"));const auto emptyDraw=empty.state().variety.dialogueRng;
    empty.advanceGameplay(1000,ActorCompletion{true,true,true},none);check(!empty.state().variety.dialogue.active && empty.state().variety.dialogueRng==emptyDraw,"empty candidates drew RNG");
    auto system=scene(package,"companion.checkin");system.gameplay.enemyInstance=0;system.gameplay.life=LifePhase::Pose;system.gameplay.sceneBeat=SceneBeat::CompletionText;
    check(catalogueText(system,c)==L"탐험을 성공적으로 마쳤다" && cataloguePortraits(system,c).speaker==PortraitRole::None,"mandatory system completion changed");
    std::cout<<"56 bundles / seven contexts: quiet-player dialogue, dedicated RNG, repeat avoidance, reveal/read, restore, eligibility and finite cleanup pass\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
