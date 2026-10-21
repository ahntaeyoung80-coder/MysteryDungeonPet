#include "legacy_snapshot.hpp"
#include "core/character_selection.hpp"
#include "core/exploration_package.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <fstream>
#include <stdexcept>
using namespace pet;
void check(bool b,const char* message){if(!b)throw std::runtime_error(message);}
ExplorationState social(const ExplorationPackage& p,std::uint64_t now) {
    Explorer e(992,42,p.assetVersion,p.floors,{0,0,0,0});auto s=e.state();
    s.world.terrain.fill(0);s.world.room.fill(-1);
    for(int y=2;y<=9;++y)for(int x=2;x<=9;++x){s.world.terrain[y*24+x]=1;s.world.room[y*24+x]=0;}
    for(int y=2;y<=9;++y)for(int x=12;x<=19;++x){s.world.terrain[y*24+x]=1;s.world.room[y*24+x]=1;}
    for(int x=9;x<=12;++x)s.world.terrain[4*24+x]=1;
    s.world.centers={4*24+4,4*24+15};s.world.start=s.world.centers[0];s.world.stairs=s.world.centers[1];
    s.position=4*24+4;s.visited.fill(0);s.visited[s.position]=1;s.discovered=s.world.terrain;s.knownStairs=s.world.stairs;
    s.floorSteps=96;s.target=6*24+6;s.goalKind=GoalKind::Frontier;s.population.entities.clear();
    auto& g=s.gameplay;g.enabled=true;g.companion.enabled=true;g.companion.tile=s.position-1;g.independentParty=true;g.phaseStartedMs=g.deadlineMs=now;
    return s;
}
int main(int argc,char** argv){try {
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path()/"assets/original/gameplay";
    const auto catalogue=loadCharacterCatalogue(root/"starters/starters.mdsc");
    if(argc==4 && std::string(argv[1])=="--fixtures") {
        const auto package=loadExplorationPackage(argv[2]);const std::filesystem::path out=argv[3];std::filesystem::create_directories(out);
        const auto now=std::uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
        auto state=social(package,now);saveSnapshot(out/"conversation.mdes",state);
        auto npc=state;npc.floorSteps=0;npc.population.entities={{1,383,EntityKind::Friend,npc.position+1,false}};npc.target=npc.position;npc.targetInstance=1;npc.goalKind=GoalKind::Friend;
        saveSnapshot(out/"npc.mdes",npc);
        for(bool partner:{false,true}) {
            auto dead=state;dead.phase=Phase::Home;dead.target=-1;dead.targetInstance=0;dead.goalKind=GoalKind::None;
            auto& g=dead.gameplay;g.life=LifePhase::Death;g.deathStage=DeathStage::PartnerTalk;g.deadlineMs=now+2400;g.defeated=true;
            if(partner){g.companion.hearts=0;g.companion.hidden=true;}else{g.hearts=0;g.playerHidden=true;}
            saveSnapshot(out/(partner?"partner-death.mdes":"player-death.mdes"),dead);
        }
        auto fade=state;fade.floorSteps=0;fade.gameplay.sceneBeat=SceneBeat::ExploreFadeIn;fade.gameplay.deadlineMs=now+750;
        saveSnapshot(out/"fade.mdes",fade);
        for(auto role:{PortraitRole::Player,PortraitRole::Partner})for(auto id:catalogue.menu(role)) {
            CharacterSelection choice;if(role==PortraitRole::Player)choice.player=id;else choice.partner=id;
            if(catalogue.sameSpecies(choice.player,choice.partner)) {
                if(role==PortraitRole::Player)choice.partner=id==1?4:1;else choice.player=id==1?4:1;
            }
            saveCharacterSelection(out/(std::to_string(unsigned(role))+"-"+std::to_string(id)+".state"),catalogue,choice);
        }
        saveCharacterSelection(out/"female-pikachu.state",catalogue,{625,1});return 0;
    }
    check(catalogue.characters.size()==34 && catalogue.menu(PortraitRole::Player).size()==21 && catalogue.menu(PortraitRole::Partner).size()==21,"starter union must expose 21 species in each role");
    for(unsigned id:{52U,488U})check(catalogue.select(PortraitRole::Player,id).portraitClip==id,"Meowth/Munchlax unavailable as player");
    bool rejected=false;
    rejected=false;try{(void)catalogue.select(PortraitRole::Partner,383);}catch(const std::exception&){rejected=true;}check(rejected,"non-starter NPC selectable");
    const auto portraits=loadPackage(root/"portraits/portraits.mdpa");
    for(const auto& c:catalogue.characters) {
        const auto role=c.roleMask&1?PortraitRole::Player:PortraitRole::Partner;const auto actor=catalogue.select(role,c.id);
        const auto body=loadPackage(root/actor.bodyManifest);validateDialogueActor(actor,body,portraits);
        for(unsigned clip:{0U,1U,5U,6U,7U,13U,14U,15U,17U,22U,27U,33U,34U})for(unsigned dir=0;dir<8;++dir)check(!body.frames(clip,dir).empty(),"starter scene clip missing");
        check(portraits.frames(c.id,0).size()==14,"starter expression missing");
    }
    auto cast=loadDialogueCast(root/"portraits/cast.mddc");
    check(portraits.frames(383,0).front().sourceMetadata.find("\"kaoIndex\": 382")!=std::string::npos,"Kecleon still uses Cradily portrait");
    check(portraits.frames(383,0)[4].sourceMetadata.find("SpriteCollab")!=std::string::npos,"NPC worried portrait supplement missing");
    const auto profile=std::filesystem::temp_directory_path()/"mdpet-character-test.state";
    saveCharacterSelection(profile,catalogue,{625,488});auto selected=loadCharacterSelection(profile,catalogue);
    check(selected.player==625 && selected.partner==488,"gender or partner-only selection lost");
    const auto changed=selectedDialogueCast(cast,catalogue,profile);check(changed.actor(PortraitRole::Player).portraitClip==625 && changed.actor(PortraitRole::Partner).portraitClip==488,"startup profile did not bind both roles");
    const auto saved=readFile(profile,512);
    for(auto duplicate:{CharacterSelection{4,4},CharacterSelection{625,25}}) {
        rejected=false;try{saveCharacterSelection(profile,catalogue,duplicate);}catch(const std::exception&){rejected=true;}
        check(rejected && readFile(profile,512)==saved,"duplicate or gender variant overwrote the valid profile");
    }
    unsigned allowed=0,blocked=0;
    for(auto player:catalogue.menu(PortraitRole::Player))for(auto partner:catalogue.menu(PortraitRole::Partner)) {
        try {catalogue.validate({player,partner});++allowed;}
        catch(const std::exception&) {check(catalogue.sameSpecies(player,partner),"different species rejected");++blocked;}
    }
    check(allowed==420 && blocked==21 && catalogue.sameSpecies(625,25),"starter pair or gender duplicate policy wrong");
    auto damaged=saved;
    auto setId=[&](unsigned offset,unsigned id){for(unsigned b=0;b<4;++b)damaged[offset+b]=std::uint8_t(id>>(b*8));};
    auto writeDamaged=[&](){std::ofstream file(profile,std::ios::binary|std::ios::trunc);file.write(reinterpret_cast<const char*>(damaged.data()),std::streamsize(damaged.size()));};
    for(auto duplicate:{CharacterSelection{4,4},CharacterSelection{625,25}}) {
        setId(76,duplicate.player);setId(80,duplicate.partner);writeDamaged();
        selected=loadCharacterSelection(profile,catalogue);
        check(selected.player==duplicate.player && selected.partner==1,"legacy duplicate did not preserve player and choose distinct partner");
        check(loadCharacterSelection(profile,catalogue).partner==1 && readFile(profile,512)!=damaged,"legacy duplicate repair was not persisted");
    }
    for(unsigned player:{52U,488U}) {
        saveCharacterSelection(profile,catalogue,{player,4});const auto playerCast=selectedDialogueCast(cast,catalogue,profile);
        check(playerCast.actor(PortraitRole::Player).portraitClip==player,"Meowth/Munchlax player profile lost on restart");
    }
    setId(76,383);setId(80,4);writeDamaged();
    const auto fallback=selectedDialogueCast(cast,catalogue,profile);
    check(fallback.actor(PortraitRole::Player).portraitClip==1 && fallback.actor(PortraitRole::Partner).portraitClip==4,"invalid profile partially changed the cast");
    std::filesystem::remove(profile);
    Surface atlas(portraits.width,portraits.height);for(unsigned y=0;y<atlas.height;++y)for(unsigned x=0;x<atlas.width;++x){const auto i=(y*atlas.width+x)*4;atlas.pixels[i]=x/40+1;atlas.pixels[i+1]=y/40+1;atlas.pixels[i+3]=255;}
    Surface frame(56,56),screen(200,200);
    for(unsigned emotion=0;emotion<14;++emotion){screen.clear();dialoguePortraitOverlay(screen,atlas,portraits,{PortraitRole::Player,0,PortraitEmotion(emotion)},cast,frame);check(screen.pixels[(96*200+16)*4+1]==emotion+1,"expression resolver always drew normal face");}
    const auto package=loadExplorationPackage(root.parent_path()/"exploration/dungeon-0/exploration.mdpe");
    auto s=social(package,1000);Explorer e(s);e.advanceGameplay(1000);check(dialogueDirection(e.state(),0)==6 && dialogueDirection(e.state(),kCompanionActorId)==2,"companion not facing player");
    const int offsets[]{24,25,1,-23,-24,-25,-1,23};
    for(bool party:{false,true})for(bool partnerChat:{false,true})for(unsigned direction=0;direction<8;++direction) {
        auto facing=social(package,1000);facing.gameplay.independentParty=party;const auto target=facing.position+offsets[direction];
        if(partnerChat)facing.gameplay.companion.tile=target;
        else {facing.floorSteps=0;facing.population.entities={{1,383,EntityKind::Friend,target,false}};facing.target=facing.position;facing.targetInstance=1;facing.goalKind=GoalKind::Friend;facing.gameplay.companion.tile=facing.position+offsets[(direction+4)%8];}
        Explorer talk(facing);talk.advanceGameplay(1000);
        for(unsigned stage=0;stage<2;++stage) {
            talk=Explorer(decodeSnapshot(encodeSnapshot(talk.state()),facing.assetVersion));
            check(talk.state().gameplay.life==LifePhase::Dialogue && dialogueDirection(talk.state(),0)==direction &&
                  dialogueDirection(talk.state(),partnerChat?kCompanionActorId:1)==(direction+4)%8,"rendered dialogue direction or restore wrong");
            if(!stage)talk.advanceGameplay(talk.state().gameplay.deadlineMs,ActorCompletion{true,true,true});
        }
    }
    auto legacy=encodeSnapshot(s);removeVarietyBlock(legacy);legacy[4]=10;
    std::uint32_t crc=0xffffffff;
    for(std::size_t i=0;i<legacy.size()-4;++i){crc^=legacy[i];for(unsigned b=0;b<8;++b)crc=(crc>>1)^((crc&1)?0xedb88320U:0);}
    crc=~crc;for(unsigned b=0;b<4;++b)legacy[legacy.size()-4+b]=std::uint8_t(crc>>(b*8));
    check(decodeSnapshot(legacy,s.assetVersion).gameplay.life==s.gameplay.life,"MDES10 migration failed");
    for(bool partner:{false,true}) {
        auto state=s;state.phase=Phase::Home;state.target=-1;state.goalKind=GoalKind::None;
        auto& g=state.gameplay;g.life=LifePhase::Death;g.deathStage=DeathStage::PartnerTalk;g.deadlineMs=3400;
        g.hearts=partner?5:0;g.companion.hearts=partner?0:5;
        Explorer dead(state);const auto rng=state.rng.state;const auto line=gameplayDialogue(g);
        check(line.id && line.speaker==PortraitRole::Partner,"defeat speaker missing");
        dead=Explorer(decodeSnapshot(encodeSnapshot(dead.state()),state.assetVersion));dead.advanceGameplay(3399,{true,true,true});
        check(dead.state().gameplay.deathStage==DeathStage::PartnerTalk && gameplayDialogue(dead.state().gameplay).id==line.id,"restored death line shortened or rerolled");
        dead.advanceGameplay(3400);check(dead.state().gameplay.deathStage==DeathStage::FadeOut,"defeat line stuck");dead.advanceGameplay(4150);check(dead.state().gameplay.deathStage==DeathStage::Blackout,"death fade missing");
        dead.advanceGameplay(5150);check(dead.state().gameplay.life==LifePhase::ReturnHome && dead.state().rng.state==rng,"death dialogue consumed combat RNG or did not return");
    }
    for(auto beat:{SceneBeat::FloorFadeOut,SceneBeat::FloorFadeIn,SceneBeat::CompletionFadeIn,SceneBeat::CompletionFadeOut}) {
        GameplayState g;g.sceneBeat=beat;g.phaseStartedMs=1000;Surface image(200,200);std::fill(image.pixels.begin(),image.pixels.end(),255);
        const auto opacity=gameplayFadeOpacity(g,1375);applyGameplayFade(image.pixels.data(),image.pixels.size(),g,1375);
        check(image.pixels[(10*200+185)*4]==255-opacity && image.pixels[(70*200+5)*4]==255-opacity,"HUD and background use different fade");
    }
    std::cout<<"34 character IDs / 21 options per role, 420 distinct pairs / 21 duplicates blocked, gender duplicate migration, 14 moods, exact Kecleon mapping, profile replace/restore, defeat dialogue replay and final fade passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
