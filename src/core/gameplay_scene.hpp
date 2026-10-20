#pragma once
#include "core/exploration.hpp"
#include "core/surface.hpp"
namespace pet {
// Hearts, berry text-box background and fade-to-black share the production/test compositor.
struct BedroomExitSample {Point position;unsigned direction;bool finished;bool moving=false;};
bool guildAppleVisible(const GameplayState&);
unsigned bedroomDirection(const GameplayState&,unsigned fallback,bool partner=false);
unsigned bedroomPlaybackScale(const GameplayState&);
Point bedroomExitCamera(const GameplayState&,std::uint64_t utcMs);
BedroomExitSample bedroomExit(Point start,const GameplayState&,std::uint64_t utcMs,bool partner=false);
BedroomExitSample loudredAlarm(Point original,const GameplayState&,std::uint64_t utcMs);
std::wstring gameplayText(const GameplayState&);
std::wstring revealedGameplayText(const GameplayState&,std::uint64_t utcMs);
std::size_t loopFrameAt(const Package&,std::uint32_t clip,std::uint64_t elapsedMs);
Surface originalDialoguePanel(const Surface&,const Package&);
void gameplayOverlay(Surface&, const GameplayState&, std::uint64_t utcMs,const Surface* dialoguePanel=nullptr);
}
