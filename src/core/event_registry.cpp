#include "core/event_registry.hpp"
#include "core/exploration_io.hpp"
#include "core/package.hpp"
#include <algorithm>
#include <set>
namespace pet {
namespace {
Weights weights(detail::Reader& r){Weights result;const auto n=r.count(2048);std::set<unsigned> seen;for(unsigned i=0;i<n;++i){const auto id=r.count(65535);const auto w=r.u64();if(!seen.insert(id).second || w>65535)throw std::runtime_error("invalid event weight");result.push_back({id,w});}return result;}
std::vector<ItemCategory> items(detail::Reader& r){std::vector<ItemCategory> result;const auto n=r.count(64);for(unsigned i=0;i<n;++i){ItemCategory c;c.originalId=r.count(255);c.weight=r.u64();if(c.weight>65535)throw std::runtime_error("invalid item context weight");c.items=weights(r);result.push_back(std::move(c));}return result;}
}
const EventFloorFacts* EventRegistry::floor(const ExplorationState& s) const {
    const auto& p=s.floors.at(s.floorIndex);const auto i=std::find_if(floors.begin(),floors.end(),[&](const auto& f){return f.dungeon==p.dungeonId && f.floor==p.localFloor;});return i==floors.end()?nullptr:&*i;
}
EventRegistry loadEventRegistry(const std::filesystem::path& path) {
    const auto bytes=readFile(path,1024*1024);detail::Reader r(bytes);EventRegistry result;
    if(r.u32()!=0x5245444d)throw std::runtime_error("unknown event map registry");const auto schema=r.count(2);if(!schema)throw std::runtime_error("unknown event map registry");result.romHash=r.str();
    if(result.romHash.size()!=64)throw std::runtime_error("invalid event ROM identity");
    const auto count=r.count(8);std::set<unsigned> maps;
    for(unsigned i=0;i<count;++i) {
        EventMapChoice m;m.dungeon=r.count(65535);m.packageFile=r.str();m.assetVersion=r.str();
        if(!maps.insert(m.dungeon).second || m.packageFile!="dungeon-"+std::to_string(m.dungeon)+"/exploration.mdpe" || m.assetVersion.size()!=64)throw std::runtime_error("invalid selected map binding");
        result.maps.push_back(std::move(m));
    }
    const auto n=r.count(schema>=2?2048:128);std::set<std::pair<unsigned,unsigned>> floors;
    for(unsigned i=0;i<n;++i) {
        EventFloorFacts f;f.dungeon=r.count(65535);f.floor=r.count(127);f.shopChance=r.count(100);f.monsterHouseChance=r.count(100);f.trapDensity=r.count(255);
        if((schema==1 && !maps.count(f.dungeon)) || !floors.insert({f.dungeon,f.floor}).second)throw std::runtime_error("invalid event floor binding");
        f.monsterHouse=weights(r);f.traps=weights(r);f.shopItems=items(r);f.monsterHouseItems=items(r);result.floors.push_back(std::move(f));
    }
    r.end();return result;
}
} // namespace pet
