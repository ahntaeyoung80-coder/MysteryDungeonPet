#include "core/gameplay_scene.hpp"
#include <algorithm>
namespace pet {
void gameplayOverlay(Surface& s, const GameplayState& g, std::uint64_t now) {
    auto pixel = [&](unsigned x, unsigned y, unsigned blue, unsigned green, unsigned red) {
        if (x >= s.width || y >= s.height) return;
        const auto i = (y * s.width + x) * 4;
        s.pixels[i] = static_cast<std::uint8_t>(blue); s.pixels[i+1] = static_cast<std::uint8_t>(green);
        s.pixels[i+2] = static_cast<std::uint8_t>(red); s.pixels[i+3] = 255;
    };
    if (dungeonScene(g)) {
        static const char* heart[]{"0110110", "1111111", "1111111", "0111110", "0011100", "0001000"};
        for (unsigned h = 0; h < 3; ++h) for (unsigned y = 0; y < 6; ++y) for (unsigned x = 0; x < 7; ++x)
            if (heart[y][x] == '1') pixel(8 + 10*h+x, 8+y, 35, 35, h < g.hearts ? 230 : 65);
    }
    if (g.life == LifePhase::Combat && g.combat == CombatPhase::BerryText)
        for (unsigned y = 150; y < std::min(190U, s.height); ++y)
            for (unsigned x = 8; x < std::min(192U, s.width); ++x) pixel(x, y, 40, 30, 20);
    if (g.life == LifePhase::ReturnHome) {
        const auto elapsed = now > g.phaseStartedMs ? now - g.phaseStartedMs : 0;
        const auto opacity = g.homeScene ? (elapsed >= 1500 ? 0 : std::min<std::uint64_t>(255, (1500-elapsed)*255/750)) : std::min<std::uint64_t>(255, elapsed*255/750);
        for (std::size_t i = 0; i < s.pixels.size(); i += 4)
            for (unsigned ch = 0; ch < 3; ++ch) s.pixels[i+ch] = static_cast<std::uint8_t>(s.pixels[i+ch] * (255-opacity) / 255);
    }
}
}
