#pragma once
#include "core/gameplay_scene.hpp"
namespace pet {
struct StarterCharacter {std::uint32_t id=0,roleMask=0;DialogueActor actor;};
struct CharacterSelection {std::uint32_t player=1,partner=4;};
struct CharacterCatalogue {
    std::string romHash;
    std::vector<StarterCharacter> characters;
    DialogueActor select(PortraitRole,std::uint32_t id) const;
    std::vector<std::uint32_t> menu(PortraitRole) const;
};
CharacterCatalogue loadCharacterCatalogue(const std::filesystem::path&);
CharacterSelection loadCharacterSelection(const std::filesystem::path&,const CharacterCatalogue&);
void saveCharacterSelection(const std::filesystem::path&,const CharacterCatalogue&,CharacterSelection);
DialogueCast selectedDialogueCast(DialogueCast,const CharacterCatalogue&,const std::filesystem::path&);
}
