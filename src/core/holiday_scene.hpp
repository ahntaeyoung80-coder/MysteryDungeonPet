#pragma once
#include "core/dialogue_catalogue.hpp"
#include <map>
namespace pet {
struct HolidayRoute {unsigned id=0,width=0,height=0;std::string scene;std::vector<Point> player,partner;};
struct HolidayFloat {unsigned md=0,sprite=0;std::string manifest,key,hash;};
struct HolidayRegistry {
    std::map<unsigned,HolidayRoute> routes;
    std::map<unsigned,HolidayFloat> floats;
    const HolidayRoute& route(unsigned id) const;
};
struct HolidaySample {std::string scene;Point player,partner,npc;unsigned playerDirection=0,partnerDirection=0,npcDirection=0;bool moving=false,bathing=false,npcVisible=false;};
HolidayRegistry loadHolidayRegistry(const std::filesystem::path& directory);
HolidaySample holidaySample(const ExplorationState&,const HolidayRegistry&,std::uint64_t now);
unsigned holidayFade(const ExplorationState&,std::uint64_t now);
bool startHoliday(ExplorationState&,const DialogueCatalogue&,const HolidayRegistry&,std::uint64_t now);
// True means the visit is complete and the existing return/evening flow must start.
bool advanceHoliday(ExplorationState&,const DialogueCatalogue&,const HolidayRegistry&,std::uint64_t now);
void checkpointHoliday(ExplorationState&,std::uint64_t now);
void restoreHolidayClocks(ExplorationState&,std::uint64_t now);
std::size_t holidayFrame(const Package&,unsigned clip,unsigned direction,std::uint64_t elapsedMs);
std::uint32_t chooseDungeon(VarietyState&,const std::vector<std::uint32_t>& ids,std::uint32_t current=0);
} // namespace pet
