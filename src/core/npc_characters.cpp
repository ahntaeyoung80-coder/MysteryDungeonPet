#include "core/npc_characters.hpp"
#include "core/exploration_io.hpp"
#include <algorithm>
#include <set>
namespace pet {
namespace {
bool hash(const std::string& value) {
    return value.size()==64 && std::all_of(value.begin(),value.end(),[](char c) {
        return (c>='0' && c<='9') || (c>='a' && c<='f');
    });
}
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
}
const NpcCharacter& NpcCatalogue::select(std::uint32_t id) const {
    const auto it=std::find_if(characters.begin(),characters.end(),[&](const auto& c){return c.id==id;});
    if(it==characters.end())throw std::runtime_error("unsupported NPC MD="+std::to_string(id));
    return *it;
}
const NpcCharacter& NpcCatalogue::forActor(const ExplorationState& s,std::uint32_t id) const {
    require(id && id!=kCompanionActorId,"player/companion is not an NPC instance");
    const auto it=std::find_if(s.population.entities.begin(),s.population.entities.end(),[&](const auto& e) {
        return e.instanceId==id && e.kind==EntityKind::Friend;
    });
    require(it!=s.population.entities.end(),"NPC instance is absent or is an item");
    return select(it->originalId);
}
NpcCatalogue loadNpcCatalogue(const std::filesystem::path& path) {
    const auto bytes=readFile(path,128*1024);detail::Reader r(bytes);NpcCatalogue result;
    require(r.u32()==0x434e444d && r.u32()==1,"unknown NPC catalogue schema");
    result.romHash=r.str();require(hash(result.romHash),"invalid NPC ROM hash");
    const auto count=r.count(128);require(count>0,"empty NPC catalogue");
    std::set<unsigned> ids,visitors;
    for(unsigned i=0;i<count;++i) {
        NpcCharacter c;c.id=r.count(65535);c.speciesId=r.count(493);c.spriteIndex=r.count(65535);
        c.kaoIndex=r.count(4095);c.emotionMask=r.count((1U<<14)-1);c.talkGroup=r.count(255);c.visitorActorId=r.count(65535);
        auto& a=c.actor;a.role=PortraitRole::Friendly;a.portraitClip=c.id;
        a.characterId=r.str();a.bodyAssetKey=r.str();a.bodyAtlasSha256=r.str();a.bodyManifest=r.str();a.displayName=r.str();
        require(c.id && c.speciesId && ids.insert(c.id).second && (!c.visitorActorId || visitors.insert(c.visitorActorId).second),"invalid NPC identity/visitor");
        require(a.characterId=="rom:"+result.romHash+":EoS_NA:md:"+std::to_string(c.id) &&
            hash(a.bodyAtlasSha256) && !a.bodyAssetKey.empty() && !a.displayName.empty(),"invalid NPC body/portrait binding");
        const auto file=std::filesystem::u8path(a.bodyManifest);
        require(!file.empty() && !file.is_absolute() && a.bodyManifest.find_first_of("\\:")==std::string::npos,"invalid NPC body path");
        for(const auto& part:file)require(part!="." && part!="..","unsafe NPC body path");
        result.characters.push_back(std::move(c));
    }
    r.end();return result;
}
void validateNpcCoverage(const NpcCatalogue& catalogue,const std::vector<FloorProfile>& floors) {
    for(const auto& floor:floors)for(const auto& weight:floor.normalFriends)
        if(weight.value)(void)catalogue.select(weight.originalId);
}
void validateNpcCharacter(const NpcCharacter& c,const Package& body,const Package& portraits) {
    require(c.actor.bodyAssetKey==body.assetKey && c.actor.bodyAtlasSha256==body.pngSha256,"NPC body asset mismatch");
    for(unsigned clip:{0U,1U,6U,7U}) {
        require(body.resolve(clip).rows.size()==8,"NPC required clip must retain eight original directions");
        for(unsigned dir=0;dir<8;++dir)require(!body.frames(clip,dir).empty(),"NPC required body clip/direction absent");
    }
    for(unsigned clip:{22U,33U}) {
        const auto it=std::find_if(body.clips.begin(),body.clips.end(),[&](const auto& a){return a.id==clip;});
        if(it!=body.clips.end())for(unsigned dir=0;dir<8;++dir)require(!body.frames(clip,dir).empty(),"NPC optional direction absent");
    }
    if(c.emotionMask&1)validateDialogueActor(c.actor,body,portraits);
    if(c.visitorActorId)require(!body.frames(0,0).empty(),"guild visitor has no Walk");
}
PortraitEmotion npcEmotion(const NpcCharacter& c,PortraitEmotion requested) {
    require(c.emotionMask&1,"NPC lacks a Normal portrait; dialogue role unavailable");
    const auto index=static_cast<unsigned>(requested);
    return index<14 && (c.emotionMask&(1U<<index))?requested:PortraitEmotion::Normal;
}
} // namespace pet
