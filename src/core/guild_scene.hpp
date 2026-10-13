#pragma once
#include "core/package.hpp"
#include <vector>
namespace pet {
enum class GuildActorKind : std::uint32_t { Player, Partner, GuildMember, Object };
struct GuildActor {
    std::uint32_t id = 0, mdIndex = 0, direction = 0, actionClip = 7, effectId = 0;
    GuildActorKind kind = GuildActorKind::Player;
    Point position;
    std::string name, assetFile;
};
struct GuildRoom {
    std::string name, sourceScene;
    Point sourceOrigin;
    std::uint32_t width=200,height=200;
    std::vector<GuildActor> actors;
    const GuildActor& player() const;
};
std::vector<GuildRoom> loadGuildScene(const std::filesystem::path&);
}
