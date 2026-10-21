#pragma once
#include "core/exploration.hpp"
namespace pet {
struct EventFloorFacts {
    std::uint32_t dungeon=0,floor=0,shopChance=0,monsterHouseChance=0,trapDensity=0;
    Weights monsterHouse,traps;
    std::vector<ItemCategory> shopItems,monsterHouseItems;
};
struct EventMapChoice {std::uint32_t dungeon=0;std::string packageFile,assetVersion;};
struct EventRegistry {
    std::string romHash;
    std::vector<EventMapChoice> maps;
    std::vector<EventFloorFacts> floors;
    const EventFloorFacts* floor(const ExplorationState&) const;
};
EventRegistry loadEventRegistry(const std::filesystem::path&);
} // namespace pet
