#pragma once
#include "core/exploration.hpp"
#include "core/surface.hpp"
namespace pet {
// Hearts, berry text-box background and fade-to-black share the production/test compositor.
void gameplayOverlay(Surface&, const GameplayState&, std::uint64_t utcMs);
}
