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
std::wstring explorationTitle(const ExplorationState&);
unsigned explorationTitleOpacity(const ExplorationState&,std::uint64_t utcMs);
std::wstring revealedGameplayText(const GameplayState&,std::uint64_t utcMs);
std::size_t loopFrameAt(const Package&,std::uint32_t clip,std::uint64_t elapsedMs);
Surface originalDialoguePanel(const Surface&,const Package&);
Surface originalWindowPanel(const Surface&,const Package&,unsigned width,unsigned height);
enum class PortraitRole : std::uint32_t { None=0, Player=1, Partner=2, Friendly=3, Alarm=4 };
enum class PortraitEmotion : std::uint32_t { Normal, Happy, Pain, Angry, Worried, Sad, Crying, Shouting, Teary, Determined, Joyous, Inspired, Surprised, Dizzy };
// A role/instance selects the speaker. Character identity and portrait IDs come from the cast.
struct DialoguePortraits { PortraitRole speaker=PortraitRole::None;std::uint32_t actorId=0;PortraitEmotion emotion=PortraitEmotion::Normal; };
enum class DesktopState { Unknown, Active, Idle, Locked, DisplayOff, Suspended };
DialoguePortraits idlePortrait(const GameplayState&);
std::wstring desktopStatusText(const GameplayState&,DesktopState);
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
// Stable line/context IDs prepare selection from multiple lines without consuming combat RNG.
struct DialogueLine {std::uint32_t id=0;PortraitRole speaker=PortraitRole::None;PortraitEmotion emotion=PortraitEmotion::Normal;std::wstring text;};
DialogueLine gameplayDialogue(const GameplayState&);
unsigned dialogueDirection(const ExplorationState&,std::uint32_t actorId);
unsigned gameplayFadeOpacity(const GameplayState&,std::uint64_t utcMs);
void applyGameplayFade(std::uint8_t* bgra,std::size_t bytes,const GameplayState&,std::uint64_t utcMs);
DialogueCast loadDialogueCast(const std::filesystem::path&);
void validateDialogueActor(const DialogueActor&,const Package& body,const Package& portraits);
DialoguePortraits dialoguePortraits(const GameplayState&);
// Portraits remain 40x40 screen pixels, independent of the dungeon camera/scale.
void dialoguePortraitOverlay(Surface&,const Surface& atlas,const Package&,DialoguePortraits,
    const DialogueCast&,const Surface& frame,const DialogueActor* speaker=nullptr);
void gameplayOverlay(Surface&, const GameplayState&, std::uint64_t utcMs,const Surface* dialoguePanel=nullptr,bool includeFade=true,bool includeHud=true,bool hasDialogue=false);
}
