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
Surface originalWindowPanel(const Surface&,const Package&,unsigned width,unsigned height);
enum class PortraitRole : std::uint32_t { None=0, Player=1, Partner=2, Friendly=3, Alarm=4 };
// A role/instance selects the speaker. Character identity and portrait IDs come from the cast.
struct DialoguePortraits { PortraitRole speaker=PortraitRole::None;std::uint32_t actorId=0; };
struct DialogueActor {
    PortraitRole role=PortraitRole::None;
    std::uint32_t portraitClip=0;
    std::string characterId,bodyAssetKey,bodyAtlasSha256,bodyManifest,displayName;
};
struct DialogueCast {
    std::vector<DialogueActor> actors;
    const DialogueActor& actor(PortraitRole) const;
    std::string label(PortraitRole) const;
};
DialogueCast loadDialogueCast(const std::filesystem::path&);
void validateDialogueActor(const DialogueActor&,const Package& body,const Package& portraits);
DialoguePortraits dialoguePortraits(const GameplayState&);
// Portraits remain 40x40 screen pixels, independent of the dungeon camera/scale.
void dialoguePortraitOverlay(Surface&,const Surface& atlas,const Package&,DialoguePortraits,
    const DialogueCast&,const Surface& frame);
void gameplayOverlay(Surface&, const GameplayState&, std::uint64_t utcMs,const Surface* dialoguePanel=nullptr);
}
