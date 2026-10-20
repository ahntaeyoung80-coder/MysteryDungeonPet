#include "core/exploration.hpp"
#include "core/exploration_package.hpp"
#include "core/gameplay_scene.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>
using namespace pet;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
ExplorationState fixture(bool party=true) {
    Explorer e(921,42,"dialogue",fixtureProfiles(3),{0,0,0,0});auto s=e.state();
    s.world.terrain.fill(0);s.world.room.fill(-1);
    for(int y=2;y<=9;++y)for(int x=2;x<=9;++x){s.world.terrain[y*24+x]=1;s.world.room[y*24+x]=0;}
    for(int y=2;y<=9;++y)for(int x=12;x<=19;++x){s.world.terrain[y*24+x]=1;s.world.room[y*24+x]=1;}
    for(int x=9;x<=12;++x)s.world.terrain[4*24+x]=1;
    s.world.centers={4*24+4,4*24+15};s.world.start=s.world.centers[0];s.world.stairs=s.world.centers[1];
    s.position=4*24+4;s.visited.fill(0);s.visited[s.position]=1;s.discovered=s.world.terrain;s.knownStairs=s.world.stairs;
    s.floorSteps=28;s.target=6*24+6;s.goalKind=GoalKind::Frontier;
    auto& g=s.gameplay;g.enabled=true;g.companion.enabled=true;g.companion.tile=s.position-1;g.independentParty=party;
    return s;
}
Explorer restore(const Explorer& e){return Explorer(decodeSnapshot(encodeSnapshot(e.state()),e.state().assetVersion));}
void conversation(bool party) {
    const auto initial=fixture(party);Explorer e(initial);
    check(e.advanceGameplay(1000).event==GameplayEvent::DialoguePlayer && companionDialogue(e.state().gameplay),"safe exploration did not begin companion conversation");
    const auto& a=e.state().gameplay;
    check(a.playerDirection==6 && a.companion.direction==2 && gameplayClip(a)==22 && companionClip(a)==7,"party did not turn toward one another");
    check(e.state().rng.state==initial.rng.state && a.actionRound==initial.gameplay.actionRound && a.playerActionRound==0,"chat consumed combat RNG/turn");
    e=restore(e);auto bytes=encodeSnapshot(e.state());
    e.advanceGameplay(1000+kDialogueMs-1,ActorCompletion{true,true,true});
    check(encodeSnapshot(e.state())==bytes,"chat moved actors before reading interval");
    e.advanceGameplay(1000+kDialogueMs,ActorCompletion{false,true,true});
    check(encodeSnapshot(e.state())==bytes,"NPC/partner completion skipped the player's line");
    e.advanceGameplay(1000+kDialogueMs,ActorCompletion{true,false,false});
    check(e.state().gameplay.dialogueStage==DialogueStage::Npc && companionClip(e.state().gameplay)==22 && gameplayClip(e.state().gameplay)==7,"companion reply animation missing");
    e=restore(e);bytes=encodeSnapshot(e.state());
    e.advanceGameplay(1000+2*kDialogueMs,ActorCompletion{true,true,false});
    check(bytes==encodeSnapshot(e.state()),"enemy completion ended companion reply");
    e.advanceGameplay(1000+2*kDialogueMs,ActorCompletion{false,false,true});
    check(e.state().gameplay.life==LifePhase::Explore && e.state().target==initial.target && e.state().position==initial.position,"chat failed to resume same exploration goal");
    check(e.state().onceKeys==initial.onceKeys && e.state().inventory.size()==initial.inventory.size(),"chat created an NPC encounter/pickup");
    e=restore(e);e.advanceGameplay(1000+2*kDialogueMs,true);
    check(!companionDialogue(e.state().gameplay),"same-step chat repeated after restore");
}
void safeBoundaries() {
    for(unsigned reason=0;reason<7;++reason) {
        auto s=fixture();auto& g=s.gameplay;
        if(reason==0)g.hearts=kLowHp;
        if(reason==1)g.companion.hidden=true;
        if(reason==2)g.companion.tile=7*24+7;
        if(reason==3){s.population.entities={{1,384,EntityKind::Friend,4*24+7,false}};g.hostile={1};}
        if(reason==4){g.movement={1,900,400,2,s.position-1,s.position};g.deadlineMs=1300;}
        if(reason==5)s.floorSteps=0;
        if(reason==6){s.population.entities={{1,383,EntityKind::Friend,s.position+1,false}};}
        Explorer e(s);e.advanceGameplay(1000,true);check(!companionDialogue(e.state().gameplay),"unsafe/early companion chat interrupted exploration");
    }
    // Let real party actions settle and start the conversation inside planPlayer.
    auto s=fixture();s.gameplay.partyPhase=PartyPhase::Allies;
    s.gameplay.partyActions={{1,1,600,1000,0,0,0,PartyActionKind::Move,false,false,false},
                            {2,1,600,1000,kCompanionActorId,0,0,PartyActionKind::Move,false,false,false}};
    s.gameplay.nextPartyId=3;s.gameplay.playerActionRound=s.gameplay.companion.actionRound=1;
    s.gameplay.phaseStartedMs=600;s.gameplay.deadlineMs=1000;
    Explorer e(s);e.advanceGameplay(1000,true);
    check(companionDialogue(e.state().gameplay),"continuous shared party movement never reached a chat boundary");
}
void portraits() {
    auto g=fixture().gameplay;g.life=LifePhase::Dialogue;g.enemyInstance=17;
    auto roles=dialoguePortraits(g);check(roles.speaker==PortraitRole::Player && roles.actorId==0,"player speaker identity missing");
    g.dialogueStage=DialogueStage::Npc;roles=dialoguePortraits(g);
    check(roles.speaker==PortraitRole::Friendly && roles.actorId==17,"NPC speaker instance missing");
    g.enemyInstance=kCompanionActorId;roles=dialoguePortraits(g);
    check(roles.speaker==PortraitRole::Partner && roles.actorId==kCompanionActorId,"companion confused with NPC instance");
    for(const auto scene:{SceneBeat::BedFace,SceneBeat::MorningFace}){g.life=scene==SceneBeat::BedFace?LifePhase::Sleep:LifePhase::Ready;g.sceneBeat=scene;check(dialoguePortraits(g).speaker==PortraitRole::Player,"bed/morning speaker absent");}
    g.life=LifePhase::Wake;g.sceneBeat=SceneBeat::WakeDelay;check(dialoguePortraits(g).speaker==PortraitRole::Alarm,"wake alarm speaker absent");
    for(const auto scene:{SceneBeat::Waking,SceneBeat::RoomExit,SceneBeat::FloorFadeOut,SceneBeat::None}){g.sceneBeat=scene;check(dialoguePortraits(g).speaker==PortraitRole::None,"portrait leaked outside dialogue");}
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path();
    const auto folder=root/"assets/original/gameplay";
    const auto p=loadPackage(folder/"portraits/portraits.mdpa");
    auto cast=loadDialogueCast(folder/"portraits/cast.mddc");
    for(const auto& actor:cast.actors)validateDialogueActor(actor,loadPackage(folder/actor.bodyManifest),p);
    check(cast.actor(PortraitRole::Partner).portraitClip==4 && cast.actor(PortraitRole::Friendly).portraitClip==383,"portrait ID is still a role/atlas position");
    check(cast.label(PortraitRole::Player)=="나 · 이상해씨" && cast.label(PortraitRole::Partner)=="동료 · 파이리","speaker role/name distinction missing");
    Surface atlas(p.width,40);for(unsigned y=0;y<40;++y)for(unsigned x=0;x<p.width;++x){const auto i=(y*p.width+x)*4;atlas.pixels[i]=x/40+1;atlas.pixels[i+3]=255;}
    const auto ui=loadPackage(folder/"ui/ui.mdpa");Surface tiles(144,8);
    for(unsigned y=0;y<8;++y)for(unsigned x=0;x<144;++x){const auto i=(y*144+x)*4;tiles.pixels[i]=x/8;tiles.pixels[i+3]=255;}
    const auto frame=originalWindowPanel(tiles,ui,56,56);
    for(const auto role:{PortraitRole::Player,PortraitRole::Partner,PortraitRole::Friendly,PortraitRole::Alarm}) {
        Surface screen(200,200);dialoguePortraitOverlay(screen,atlas,p,{role,0},cast,frame);
        const unsigned left=role==PortraitRole::Player?8:136,other=role==PortraitRole::Player?144:16;
        const auto& rect=p.frames(cast.actor(role).portraitClip,0).front().rect;
        check(screen.pixels[(96*200+left+8)*4]==unsigned(rect.x/40+1),"speaker did not resolve its character portrait");
        check(screen.pixels[(100*200+other)*4+3]==0,"listener portrait remained visible");
        check(screen.pixels[(130*200+100)*4+3]==0,"removed nameplate still occupies screen");
        for(const auto sample:std::vector<std::array<unsigned,3>>{{0,0,2},{8,0,3},{48,0,4},{0,8,5},{48,8,6},{0,48,7},{8,48,8},{48,48,9}})
            check(screen.pixels[((88+sample[1])*200+left+sample[0])*4]==sample[2],"portrait did not use original dialogue corner/edge tiles");
        check(screen.pixels[(144*200+8)*4+3]==0,"portrait overwrote dialogue text");screen.validatePbgra();
    }
    auto swapped=cast;auto player=cast.actor(PortraitRole::Partner),partner=cast.actor(PortraitRole::Player);
    player.role=PortraitRole::Player;partner.role=PortraitRole::Partner;swapped.actors[0]=player;swapped.actors[1]=partner;
    validateDialogueActor(player,loadPackage(folder/player.bodyManifest),p);validateDialogueActor(partner,loadPackage(folder/partner.bodyManifest),p);
    Surface screen(200,200);dialoguePortraitOverlay(screen,atlas,p,{PortraitRole::Player,0},swapped,frame);
    check(screen.pixels[(96*200+16)*4]==2 && swapped.label(PortraitRole::Player)=="나 · 파이리","player replacement retained former portrait/name");
    screen.clear();dialoguePortraitOverlay(screen,atlas,p,{PortraitRole::Partner,kCompanionActorId},swapped,frame);
    check(screen.pixels[(96*200+144)*4]==1 && swapped.label(PortraitRole::Partner)=="동료 · 이상해씨","partner replacement retained former portrait/name");
    bool rejected=false;try{validateDialogueActor(player,loadPackage(folder/partner.bodyManifest),p);}catch(const std::exception&){rejected=true;}
    check(rejected,"mismatched body/portrait binding accepted");
    auto relabelled=loadPackage(folder/partner.bodyManifest);relabelled.assetKey=player.bodyAssetKey;rejected=false;
    try{validateDialogueActor(player,relabelled,p);}catch(const std::exception&){rejected=true;}
    check(rejected,"relabelled body asset bypassed character atlas check");
    auto invalid=player;invalid.characterId=partner.characterId;rejected=false;
    try{validateDialogueActor(invalid,loadPackage(folder/player.bodyManifest),p);}catch(const std::exception&){rejected=true;}
    check(rejected,"mismatched character identity accepted");
    Surface clipped(20,110);dialoguePortraitOverlay(clipped,atlas,p,{PortraitRole::Player,0},cast,frame);clipped.validatePbgra();
}
int main(int argc,char** argv)try {
    if(argc==4 && std::string(argv[1])=="--fixtures") {
        const auto p=loadExplorationPackage(argv[2]);const std::filesystem::path out=argv[3];std::filesystem::create_directories(out);
        auto s=fixture();s.assetVersion=p.assetVersion;s.floors=p.floors;
        const auto now=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        s.gameplay.phaseStartedMs=now;s.gameplay.deadlineMs=now;
        saveSnapshot(out/"companion.mdes",s);
        s.floorSteps=0;s.population.entities={{1,383,EntityKind::Friend,s.position+1,false}};s.target=s.position;s.targetInstance=1;s.goalKind=GoalKind::Friend;
        saveSnapshot(out/"npc.mdes",s);return 0;
    }
    conversation(false);conversation(true);safeBoundaries();portraits();
    std::cout<<"NPC/partner/bed/wake portraits, speaker/layout/clipping, safe party chat, completion ownership, save/restore and exploration resume pass\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
