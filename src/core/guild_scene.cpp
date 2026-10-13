#include "core/guild_scene.hpp"
#include "core/exploration_io.hpp"
#include <algorithm>
#include <set>
namespace pet {
const GuildActor& GuildRoom::player() const {
    const auto it = std::find_if(actors.begin(), actors.end(), [](const auto& a) { return a.kind == GuildActorKind::Player; });
    if (it == actors.end()) throw std::runtime_error("guild room has no player");
    return *it;
}
std::vector<GuildRoom> loadGuildScene(const std::filesystem::path& path) {
    const auto bytes = readFile(path, 32768);
    detail::Reader r(bytes);
    if (r.u32() != 0x5347444d || r.u32() != 1 || r.count(2) != 2) throw std::runtime_error("invalid guild scene header");
    std::vector<GuildRoom> rooms;
    for (unsigned room = 0; room < 2; ++room) {
        GuildRoom scene;
        scene.name = r.str(); scene.sourceScene = r.str();
        scene.sourceOrigin = {static_cast<std::int32_t>(r.count(4096)), static_cast<std::int32_t>(r.count(4096))};
        std::set<unsigned> ids;
        const auto n = r.count(32);
        unsigned players = 0, partners = 0;
        for (unsigned i = 0; i < n; ++i) {
            GuildActor a;
            a.id = r.u32(); a.mdIndex = r.count(65535); a.kind = static_cast<GuildActorKind>(r.count(3));
            a.name = r.str(); a.assetFile = r.str();
            a.position = {static_cast<std::int32_t>(r.count(199)), static_cast<std::int32_t>(r.count(199))};
            a.direction = r.count(7); a.actionClip = r.count(255); a.effectId = r.count(65535);
            if (!ids.insert(a.id).second) throw std::runtime_error("duplicate guild actor");
            players += a.kind == GuildActorKind::Player; partners += a.kind == GuildActorKind::Partner;
            if (a.kind == GuildActorKind::GuildMember || a.kind == GuildActorKind::Object) {
                const auto p = std::filesystem::path(a.assetFile);
                if (a.assetFile.empty() || p.is_absolute() || a.assetFile.find('\\') != std::string::npos || a.assetFile.find(':') != std::string::npos)
                    throw std::runtime_error("invalid guild asset path");
                for (const auto& part : p) if (part == ".." || part == ".") throw std::runtime_error("unsafe guild asset path");
            } else if (!a.assetFile.empty()) throw std::runtime_error("unexpected player/partner guild asset");
            scene.actors.push_back(std::move(a));
        }
        if (players != 1 || partners != 1) throw std::runtime_error("invalid guild player/partner cast");
        (void)scene.player(); rooms.push_back(std::move(scene));
    }
    r.end(); return rooms;
}
}
