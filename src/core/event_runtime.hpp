#pragma once
#include "core/dialogue_catalogue.hpp"
namespace pet {
bool eventActive(const ExplorationState&);
// Direct scenario entry bypasses frequency only; all safety/actor/space checks still apply.
bool startVarietyEvent(ExplorationState&,VarietyEvent,std::uint64_t,const DialogueCatalogue&,std::uint32_t actor=0);
bool tryAutomaticEvent(ExplorationState&,std::uint64_t,const DialogueCatalogue&);
GameplayUpdate advanceVarietyEvent(ExplorationState&,std::uint64_t,const DialogueCatalogue&);
} // namespace pet
