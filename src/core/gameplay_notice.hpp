#pragma once
#include "core/gameplay_scene.hpp"
namespace pet {
std::wstring itemDisplayName(std::uint32_t item);
void queueGameplayNotice(ExplorationState&,NoticeKind,std::uint32_t item);
void updateGameplayNotices(ExplorationState&,std::uint64_t now);
void restoreGameplayNoticeClock(ExplorationState&,std::uint64_t now);
const GameplayNotice* activeGameplayNotice(const ExplorationState&);
std::wstring gameplayNoticeText(const ExplorationState&);
std::wstring revealedGameplayNotice(const ExplorationState&,std::uint64_t now);
DialoguePortraits gameplayNoticePortraits(const ExplorationState&);
} // namespace pet
