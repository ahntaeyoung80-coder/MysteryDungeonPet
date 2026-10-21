#pragma once
#include "core/dialogue_catalogue.hpp"
#include "core/event_registry.hpp"
namespace pet {
bool eventActive(const ExplorationState&);
// Direct scenario entry bypasses frequency only; all safety/actor/space checks still apply.
bool startVarietyEvent(ExplorationState&,VarietyEvent,std::uint64_t,const DialogueCatalogue&,std::uint32_t actor=0,const EventRegistry* maps=nullptr);
bool tryAutomaticEvent(ExplorationState&,std::uint64_t,const DialogueCatalogue&,const EventRegistry* maps=nullptr);
GameplayUpdate advanceVarietyEvent(ExplorationState&,std::uint64_t,const DialogueCatalogue&,const EventRegistry* maps=nullptr);
bool commitOranShare(ExplorationState&);
void finishVarietyEvent(ExplorationState&);
bool movementEvent(VarietyEvent);
bool startMovementEvent(ExplorationState&,VarietyEvent,std::uint64_t,const DialogueCatalogue&,std::uint32_t,const EventRegistry*);
GameplayUpdate advanceMovementEvent(ExplorationState&,std::uint64_t,const DialogueCatalogue&);
bool combatEvent(VarietyEvent);
bool startCombatEvent(ExplorationState&,VarietyEvent,std::uint64_t,const DialogueCatalogue&,std::uint32_t,const EventRegistry*);
GameplayUpdate advanceCombatEvent(ExplorationState&,std::uint64_t,const DialogueCatalogue&);
bool commitShopExchange(ExplorationState&);
} // namespace pet
