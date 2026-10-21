#pragma once
#include "core/exploration.hpp"
namespace pet {
enum class EventRoute { Arrived, Moved, Blocked };
EventRoute advanceEventRoute(ExplorationState&,std::uint32_t leader,int target,unsigned stopDistance,std::uint64_t now);
} // namespace pet
