#pragma once
#include "core/gameplay_scene.hpp"
namespace pet {
struct CatalogueLine {
    std::string id;
    DialogueLine display;
    PortraitRole listener=PortraitRole::None;
    std::uint32_t holdMs=1000;
};
struct DialogueBundle {
    std::string id,context;
    std::uint32_t weight=1;
    std::vector<CatalogueLine> lines;
};
struct DialogueCatalogue {
    std::uint32_t version=0;
    std::vector<DialogueBundle> bundles;
    const DialogueBundle* find(const std::string&) const;
};
DialogueCatalogue loadDialogueCatalogue(const std::filesystem::path&);
std::string dialogueContext(const ExplorationState&);
bool dialogueEligible(const ExplorationState&,const std::string& context);
const CatalogueLine* selectedDialogue(const ExplorationState&,const DialogueCatalogue&);
std::wstring catalogueText(const ExplorationState&,const DialogueCatalogue&);
std::wstring revealedCatalogueText(const ExplorationState&,const DialogueCatalogue&,std::uint64_t now);
DialoguePortraits cataloguePortraits(const ExplorationState&,const DialogueCatalogue&);
// Snapshot-only projection; no draw, line change, or mutation of the running Explorer.
void checkpointDialogue(ExplorationState&,const DialogueCatalogue&,std::uint64_t now);
} // namespace pet
