#pragma once
#include "core/gameplay_scene.hpp"
namespace pet {
struct NpcCharacter {
    std::uint32_t id=0,speciesId=0,spriteIndex=0,kaoIndex=0,emotionMask=0,talkGroup=0,visitorActorId=0;
    DialogueActor actor;
};
struct NpcCatalogue {
    std::string romHash;
    std::vector<NpcCharacter> characters;
    const NpcCharacter& select(std::uint32_t md) const;
    const NpcCharacter& forActor(const ExplorationState&,std::uint32_t instance) const;
};
NpcCatalogue loadNpcCatalogue(const std::filesystem::path&);
void validateNpcCoverage(const NpcCatalogue&,const std::vector<FloorProfile>&);
void validateNpcCharacter(const NpcCharacter&,const Package& body,const Package& portraits);
PortraitEmotion npcEmotion(const NpcCharacter&,PortraitEmotion requested);
} // namespace pet
