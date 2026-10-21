#include "core/npc_characters.hpp"
#include "core/animator.hpp"
#include "core/exploration_package.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace pet;
namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
template<class F> void rejects(F f,const char* message) {
    bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}check(rejected,message);
}
void put(std::vector<std::uint8_t>& b,std::size_t at,unsigned value){for(unsigned i=0;i<4;++i)b.at(at+i)=std::uint8_t(value>>(i*8));}
ExplorationState scene(const ExplorationPackage& p,std::uint32_t md) {
    Explorer e(1100+md,42,p.assetVersion,p.floors,{0,0,0,0});auto s=e.state();
    s.world.terrain.fill(0);s.world.room.fill(-1);
    for(int y=2;y<10;++y)for(int x=2;x<10;++x){s.world.terrain[y*24+x]=1;s.world.room[y*24+x]=0;}
    for(int y=2;y<10;++y)for(int x=12;x<20;++x){s.world.terrain[y*24+x]=1;s.world.room[y*24+x]=1;}
    for(int x=9;x<=12;++x)s.world.terrain[4*24+x]=1;
    s.world.centers={100,111};s.world.start=100;s.world.stairs=111;s.position=100;
    s.visited.fill(0);s.visited[s.position]=1;s.discovered=s.world.terrain;s.knownStairs=111;
    s.target=s.position;s.targetInstance=1;s.goalKind=GoalKind::Friend;
    s.population.entities={{1,md,EntityKind::Friend,101,false},{2,md,EntityKind::Friend,125,false}};
    auto& g=s.gameplay;g.enabled=true;g.companion.enabled=true;g.companion.tile=99;g.independentParty=false;
    g.phaseStartedMs=g.deadlineMs=std::uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    return s;
}
}
int main(int argc,char** argv) {try {
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path()/"assets/original/gameplay";
    const auto catalogue=loadNpcCatalogue(root/"npcs/characters.mdnc");
    const auto faces=loadPackage(root/"portraits/portraits.mdpa");
    const auto package=loadExplorationPackage(root.parent_path()/"exploration/dungeon-15/exploration.mdpe");
    if(argc==4 && std::string(argv[1])=="--validate-fixtures") {
        const auto input=loadExplorationPackage(argv[2]);unsigned files=0,actors=0;
        for(const auto& entry:std::filesystem::recursive_directory_iterator(argv[3])) {
            if(entry.path().extension()!=".mdes")continue;
            const auto s=loadSnapshot(entry.path(),input.assetVersion);++files;
            for(const auto& e:s.population.entities)if(e.kind==EntityKind::Friend) {
                const auto& c=catalogue.forActor(s,e.instanceId);
                validateNpcCharacter(c,loadPackage(root/c.actor.bodyManifest),faces);++actors;
            }
        }
        check(files>0 && actors>0,"no GUI fixture actors validated");
        std::cout<<"GUI fixture catalogue coverage: "<<files<<" snapshots, "<<actors<<" actors pass\n";
        return 0;
    }
    if(argc==3 && std::string(argv[1])=="--fixtures") {
        const std::filesystem::path output=argv[2];std::filesystem::create_directories(output);
        for(const auto& c:catalogue.characters)for(bool hostile:{false,true}) {
            auto s=scene(package,c.id);if(hostile){s.gameplay.hostile={1,2};s.target=-1;s.targetInstance=0;s.goalKind=GoalKind::None;}
            saveSnapshot(output/(std::to_string(c.id)+(hostile?"-hostile.mdes":"-friendly.mdes")),s);
        }
        for(unsigned floor=0;floor+1<package.floors.size();++floor) {
            Explorer run(2000+floor,41,package.assetVersion,package.floors,{3,4,8,4});
            for(unsigned step=0;run.state().floorIndex<floor && step<50000;++step)run.advance();
            auto s=run.state();check(s.floorIndex==floor,"floor fixture failed to reach original floor");
            s.position=s.world.stairs;s.visited[s.position]=1;s.discovered=s.world.terrain;s.knownStairs=s.world.stairs;
            s.target=-1;s.targetInstance=0;s.goalKind=GoalKind::None;
            auto& g=s.gameplay;g.enabled=true;g.life=LifePhase::Explore;g.sceneBeat=SceneBeat::FloorFadeOut;
            g.phaseStartedMs=std::uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
            g.deadlineMs=g.phaseStartedMs+1000;
            saveSnapshot(output/("floor-"+std::to_string(floor)+".mdes"),s);
        }
        return 0;
    }
    check(catalogue.characters.size()==30,"NPC selection count changed without coverage review");
    validateNpcCoverage(catalogue,package.floors);
    validateNpcCoverage(catalogue,loadExplorationPackage(root.parent_path()/"exploration/dungeon-18/exploration.mdpe").floors);
    validateNpcCoverage(catalogue,loadExplorationPackage(root.parent_path()/"exploration/dungeon-0/exploration.mdpe").floors);
    auto unsupported=package.floors;unsupported[0].normalFriends.push_back({999,1});
    rejects([&]{validateNpcCoverage(catalogue,unsupported);},"unsupported species silently replaced");
    check(catalogue.select(237).speciesId==210 && catalogue.select(237).spriteIndex==257 && catalogue.select(237).kaoIndex==236,"MD/species/sprite/KAO conflated");
    check(catalogue.select(434).visitorActorId==124 && catalogue.select(369).visitorActorId==115,"guild actor ID confused with MD");
    for(const auto& c:catalogue.characters) {
        const auto body=loadPackage(root/c.actor.bodyManifest);validateNpcCharacter(c,body,faces);
        Animator animator;animator.start(body,7,0,true,1000);
        check(!animator.tryChangeDirection(body,0,2),"Idle reused as Walk direction change");
        const auto held=animator.advance(1000);check(held.frame==0,"rejected clip change mutated playback");
        animator.start(body,0,0,true,2000);
        for(unsigned dir=0;dir<8;++dir) {
            if(!animator.tryChangeDirection(body,0,dir))animator.start(body,0,dir,true,2000);
            check(animator.advance(2000).frame==0,"direction fallback lost valid Walk");
        }
        animator.start(body,7,0,false,3000);
        check(!animator.tryChangeDirection(body,7,2),"finite Idle response accepted phase-preserving turn");
        check(npcEmotion(c,PortraitEmotion::Normal)==PortraitEmotion::Normal,"Normal unavailable");
        for(unsigned emotion=0;emotion<14;++emotion)
            check(unsigned(npcEmotion(c,PortraitEmotion(emotion)))==((c.emotionMask&(1U<<emotion))?emotion:0),"missing expression changed character");
        auto s=scene(package,c.id);const auto* a=&catalogue.forActor(s,1);s.gameplay.hostile={1};
        check(a==&catalogue.forActor(s,1) && a==&catalogue.forActor(s,2),"relationship or instance changed body species");
        s=decodeSnapshot(encodeSnapshot(s),s.assetVersion);
        check(catalogue.forActor(s,1).actor.portraitClip==c.id,"restored NPC retained another species portrait");
        s.population.entities[0].kind=EntityKind::Item;
        rejects([&]{catalogue.forActor(s,1);},"item treated as portrait actor");
    }
    auto noFace=catalogue.select(126);noFace.emotionMask=0;
    rejects([&]{npcEmotion(noFace,PortraitEmotion::Happy);},"unverified Normal used in a dialogue role");
    validateNpcCharacter(noFace,loadPackage(root/noFace.actor.bodyManifest),faces); // Enemy body support is independent.
    rejects([&]{catalogue.forActor(scene(package,126),0);},"player sentinel treated as NPC");
    rejects([&]{catalogue.forActor(scene(package,126),kCompanionActorId);},"companion sentinel treated as NPC");
    auto bad=catalogue.select(237);
    rejects([&]{validateNpcCharacter(bad,loadPackage(root/catalogue.select(126).actor.bodyManifest),faces);},"wrong body accepted");
    auto missing=loadPackage(root/bad.actor.bodyManifest);missing.clips.erase(std::remove_if(missing.clips.begin(),missing.clips.end(),[](const auto& c){return c.id==1;}),missing.clips.end());
    rejects([&]{validateNpcCharacter(bad,missing,faces);},"missing Attack hidden by Idle substitution");
    missing=loadPackage(root/bad.actor.bodyManifest);for(auto& c:missing.clips)if(c.id==0)c.rows.resize(1);
    rejects([&]{validateNpcCharacter(bad,missing,faces);},"single Walk row claimed as eight original directions");
    auto cast=loadDialogueCast(root/"portraits/cast.mddc");Surface atlas(faces.width,faces.height);
    for(unsigned y=0;y<atlas.height;++y)for(unsigned x=0;x<atlas.width;++x){const auto i=(y*atlas.width+x)*4;atlas.pixels[i]=x/40+1;atlas.pixels[i+1]=y/40+1;atlas.pixels[i+3]=255;}
    Surface frame(56,56),screen(200,200);
    for(const auto& c:catalogue.characters) {
        screen.clear();dialoguePortraitOverlay(screen,atlas,faces,{PortraitRole::Friendly,1,npcEmotion(c,PortraitEmotion::Happy)},cast,frame,&c.actor);
        const auto& rect=faces.frames(c.id,0)[unsigned(npcEmotion(c,PortraitEmotion::Happy))].rect;
        check(screen.pixels[(96*200+144)*4]==rect.x/40+1,"actual NPC instance drew fixed Friendly cast");
        check(screen.pixels[(96*200+16)*4+3]==0 && screen.pixels[(130*200+100)*4+3]==0,"listener/nameplate leaked");
    }
    auto bytes=readFile(root/"npcs/characters.mdnc",128*1024);
    const auto file=std::filesystem::temp_directory_path()/"mdpet-invalid-npc.mdnc";
    auto rejectBytes=[&](const std::vector<std::uint8_t>& b) {
        {std::ofstream f(file,std::ios::binary);f.write(reinterpret_cast<const char*>(b.data()),std::streamsize(b.size()));}
        rejects([&]{loadNpcCatalogue(file);},"corrupt NPC catalogue accepted");
    };
    auto broken=bytes;put(broken,4,2);rejectBytes(broken);
    broken=bytes;put(broken,76,129);rejectBytes(broken);
    broken=bytes;broken.resize(broken.size()-1);rejectBytes(broken);
    broken=bytes;broken.push_back(0);rejectBytes(broken);std::filesystem::remove(file);
    std::cout<<"17 NPC bindings; complete dungeon 0/15 coverage; exact MD/species/sprite/KAO; roles, Normal fallback, eight directions, missing body/ID/schema rejected; single-speaker overlay pass\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
