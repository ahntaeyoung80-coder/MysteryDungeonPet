#pragma once
#include "core/exploration.hpp"
#include <array>
namespace pet {
struct Tileset {
    std::uint32_t id = 0, chunkCount = 0;
    std::string packageFile;
    std::array<std::uint32_t, 3 * 256 * 3> rules{}; // DmaType, neighbors S..SW, variation
};
struct ExplorationPackage {
    std::string assetVersion, romHash;
    std::vector<FloorProfile> floors;
    std::vector<Tileset> tilesets;
    ExpeditionState expedition;
    const Tileset& tileset(std::uint32_t id) const;
};
ExplorationPackage loadExplorationPackage(const std::filesystem::path&);
std::array<std::uint32_t, kTiles> chunkGrid(const World&, const Tileset&, unsigned variation = 0);
} // namespace pet
