#include "core/character_selection.hpp"
#include "core/exploration_io.hpp"
#include <algorithm>
#include <fstream>
#include <set>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace pet {
DialogueActor CharacterCatalogue::select(PortraitRole role,std::uint32_t id) const {
    const auto bit=role==PortraitRole::Player?1U:role==PortraitRole::Partner?2U:0U;
    const auto found=std::find_if(characters.begin(),characters.end(),[&](const auto& c){return c.id==id && (c.roleMask&bit);});
    if(found==characters.end())throw std::runtime_error("character is not an allowed starter for this role");
    auto actor=found->actor;actor.role=role;return actor;
}
std::vector<std::uint32_t> CharacterCatalogue::menu(PortraitRole role) const {
    std::vector<std::uint32_t> ids;std::set<std::uint32_t> species;
    const unsigned bit=role==PortraitRole::Player?1:role==PortraitRole::Partner?2:0;
    for(const auto& c:characters)if((c.roleMask&bit) && species.insert(c.speciesId).second)ids.push_back(c.id);
    return ids;
}
bool CharacterCatalogue::sameSpecies(std::uint32_t first,std::uint32_t second) const {
    auto species=[&](std::uint32_t id) {
        const auto found=std::find_if(characters.begin(),characters.end(),[&](const auto& c){return c.id==id;});
        if(found==characters.end())throw std::runtime_error("unknown starter character ID");
        return found->speciesId;
    };
    return species(first)==species(second);
}
void CharacterCatalogue::validate(CharacterSelection selection) const {
    (void)select(PortraitRole::Player,selection.player);(void)select(PortraitRole::Partner,selection.partner);
    if(sameSpecies(selection.player,selection.partner))throw std::runtime_error("player and partner must be different species");
}
CharacterCatalogue loadCharacterCatalogue(const std::filesystem::path& path) {
    const auto bytes=readFile(path,65536);detail::Reader r(bytes);CharacterCatalogue catalogue;
    if(r.u32()!=0x4353444D || r.u32()!=2)throw std::runtime_error("invalid starter catalogue");
    catalogue.romHash=r.str();if(catalogue.romHash.size()!=64)throw std::runtime_error("invalid starter ROM hash");
    const auto count=r.count(128);std::set<unsigned> ids;
    for(unsigned i=0;i<count;++i) {
        StarterCharacter c;c.id=r.count(65535);c.roleMask=r.count(3);c.speciesId=r.count(493);auto& a=c.actor;a.portraitClip=c.id;
        a.characterId=r.str();a.bodyAssetKey=r.str();a.bodyAtlasSha256=r.str();a.bodyManifest=r.str();a.displayName=r.str();
        const auto file=std::filesystem::u8path(a.bodyManifest);
        if(!c.id || !c.roleMask || !c.speciesId || !ids.insert(c.id).second || a.displayName.empty() || a.bodyAssetKey.empty() || a.bodyAtlasSha256.size()!=64 ||
           a.characterId!="rom:"+catalogue.romHash+":EoS_NA:md:"+std::to_string(c.id) ||
           file.empty() || file.is_absolute() || a.bodyManifest.find_first_of("\\:")!=std::string::npos)
            throw std::runtime_error("invalid starter character identity/path");
        for(const auto& part:file)if(part==".." || part==".")throw std::runtime_error("unsafe starter path");
        catalogue.characters.push_back(std::move(c));
    }
    r.end();(void)catalogue.select(PortraitRole::Player,1);(void)catalogue.select(PortraitRole::Partner,4);return catalogue;
}
CharacterSelection loadCharacterSelection(const std::filesystem::path& path,const CharacterCatalogue& catalogue) {
    const auto bytes=readFile(path,512);detail::Reader r(bytes);
    if(r.u32()!=0x5043444D || r.u32()!=1 || r.str()!=catalogue.romHash)throw std::runtime_error("invalid character profile/ROM");
    CharacterSelection selection;selection.player=r.u32();selection.partner=r.u32();r.end();
    (void)catalogue.select(PortraitRole::Player,selection.player);(void)catalogue.select(PortraitRole::Partner,selection.partner);
    // Old MDCP1 profiles allowed duplicates. Keep the player, repair only the partner,
    // and persist atomically so subsequent starts use the same distinct pair.
    if(catalogue.sameSpecies(selection.player,selection.partner)) {
        const auto candidates=catalogue.menu(PortraitRole::Partner);
        const auto replacement=std::find_if(candidates.begin(),candidates.end(),[&](auto id){return !catalogue.sameSpecies(selection.player,id);});
        if(replacement==candidates.end())throw std::runtime_error("no distinct starter partner");
        selection.partner=*replacement;saveCharacterSelection(path,catalogue,selection);
    }
    return selection;
}
void saveCharacterSelection(const std::filesystem::path& path,const CharacterCatalogue& catalogue,CharacterSelection selection) {
    catalogue.validate(selection);
    detail::Writer w;w.u32(0x5043444D);w.u32(1);w.str(catalogue.romHash);w.u32(selection.player);w.u32(selection.partner);
    auto temp=path;temp+=".tmp";
    if(!path.parent_path().empty())std::filesystem::create_directories(path.parent_path());
    {std::ofstream output(temp,std::ios::binary|std::ios::trunc);output.write(reinterpret_cast<const char*>(w.bytes.data()),std::streamsize(w.bytes.size()));output.flush();
     if(!output)throw std::runtime_error("cannot save character profile");}
#ifdef _WIN32
    if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("cannot replace character profile");
#else
    std::filesystem::rename(temp,path);
#endif
}
DialogueCast selectedDialogueCast(DialogueCast cast,const CharacterCatalogue& catalogue,const std::filesystem::path& profile) {
    if(!std::filesystem::exists(profile))return cast;
    try {
        const auto selection=loadCharacterSelection(profile,catalogue);
        for(auto& actor:cast.actors)if(actor.role==PortraitRole::Player || actor.role==PortraitRole::Partner)
            actor=catalogue.select(actor.role,actor.role==PortraitRole::Player?selection.player:selection.partner);
    } catch(const std::runtime_error&) { /* Invalid persisted selections retain the verified default cast. */ }
    return cast;
}
}
