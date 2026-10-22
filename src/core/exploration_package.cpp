#include "core/exploration_package.hpp"
#include "core/exploration_io.hpp"
#include "core/package.hpp"
#include <algorithm>
#include <set>
namespace pet {
const Tileset& ExplorationPackage::tileset(std::uint32_t id) const {
    for (const auto& t : tilesets) if (t.id == id) return t;
    throw std::runtime_error("missing tileset reference");
}
ExplorationPackage loadExplorationPackage(const std::filesystem::path& path) {
    const auto bytes = readFile(path, detail::kFileLimit);
    detail::Reader r(bytes);
    if (r.u32() != 0x4550444d) throw std::runtime_error("unknown exploration package schema");
    const auto schema=r.count(2);
    if(schema<1)throw std::runtime_error("unknown exploration package schema");
    ExplorationPackage p;
    p.assetVersion = r.str(); p.romHash = r.str(); p.floors = detail::readProfiles(r);
    const auto n = r.count(170);
    std::set<std::uint32_t> ids;
    for (std::uint32_t i = 0; i < n; ++i) {
        Tileset t;
        t.id = r.count(169); t.chunkCount = r.count(4096); t.packageFile = r.str();
        const std::filesystem::path name(t.packageFile);
        if (!ids.insert(t.id).second || !t.chunkCount || t.packageFile.empty() ||
            name.has_parent_path() || t.packageFile.find('\\') != std::string::npos ||
            name.extension() != ".mdpa") throw std::runtime_error("invalid tileset header");
        for (auto& mapping : t.rules) {
            mapping = r.u32();
            if (mapping >= t.chunkCount) throw std::runtime_error("DMA mapping outside chunks");
        }
        p.tilesets.push_back(std::move(t));
    }
    if(schema>=2)p.expedition=detail::readExpedition(r);
    r.end();
    // Validate profiles with the same runtime state invariants used by restoration.
    Explorer checked(1, 1, p.assetVersion, p.floors,{}, {},p.expedition);
    (void)checked;
    if (p.romHash.size() != 64 || p.assetVersion.size() != 64)
        throw std::runtime_error("invalid exploration provenance hash");
    for (const auto& f : p.floors) (void)p.tileset(f.tilesetId);
    return p;
}
std::array<std::uint32_t, kTiles> chunkGrid(const World& world, const Tileset& tileset, unsigned variation) {
    if (variation >= 3) throw std::runtime_error("invalid DMA variation");
    std::array<std::uint32_t, kTiles> chunks{};
    const std::array<std::pair<int, int>, 8> neighbors{{{0, 1}, {1, 1}, {1, 0}, {1, -1},
                                                     {0, -1}, {-1, -1}, {-1, 0}, {-1, 1}}};
    for (int tile = 0; tile < kTiles; ++tile) {
        const bool floor = world.walkable(tile);
        unsigned mask = 0;
        for (unsigned bit = 0; bit < neighbors.size(); ++bit) {
            const int x = tile % kWidth + neighbors[bit].first;
            const int y = tile / kWidth + neighbors[bit].second;
            // Outside is WALL, matching the offline treat_outside_as_wall=True contract.
            const bool neighborFloor = x >= 0 && x < kWidth && y >= 0 && y < kHeight &&
                                       world.walkable(y * kWidth + x);
            if (neighborFloor == floor) mask |= 1U << bit;
        }
        chunks[tile] = tileset.rules[((floor ? 2U : 0U) * 256 + mask) * 3 + variation];
        if (chunks[tile] >= tileset.chunkCount) throw std::runtime_error("chunk outside tileset");
    }
    return chunks;
}
} // namespace pet
