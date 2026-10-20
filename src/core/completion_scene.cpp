#include "core/completion_scene.hpp"
#include "core/exploration_io.hpp"
#include <algorithm>
namespace pet {
const CompletionDestination* CompletionRegistry::destination(std::uint32_t id) const {
    const auto it=std::find_if(destinations.begin(),destinations.end(),[&](const auto& d){return d.dungeonId==id;});
    return it==destinations.end()?nullptr:&*it;
}
std::string CompletionRegistry::mapLabel(std::uint32_t id,std::uint32_t floor,bool completed) const {
    const auto it=names.find(id);
    const auto* end=completed?destination(id):nullptr;
    return "["+(end?end->name:it==names.end()?"Dungeon "+std::to_string(id):it->second)+"] - "+std::to_string(floor+1)+"F";
}
CompletionRegistry loadCompletionRegistry(const std::filesystem::path& path) {
    const auto bytes=readFile(path,65536);detail::Reader r(bytes);CompletionRegistry result;
    if(r.u32()!=0x4345444D || r.u32()!=1)throw std::runtime_error("invalid completion registry");
    result.romHash=r.str();if(result.romHash.size()!=64)throw std::runtime_error("invalid completion ROM hash");
    const auto count=r.count(1024);
    for(unsigned i=0;i<count;++i) {
        const auto id=r.count(65535);const auto name=r.str();
        if(name.empty() || !result.names.emplace(id,name).second)throw std::runtime_error("invalid completion map identity");
        if(!r.boolean())continue;
        CompletionDestination d;d.dungeonId=id;d.terminalId=r.count(65535);d.name=r.str();d.assetFile=r.str();
        const auto file=std::filesystem::path(d.assetFile);
        if(d.name.empty() || file.empty() || file.has_parent_path() || d.assetFile.find_first_of("\\:")!=std::string::npos || file.extension()!=".mdpa")
            throw std::runtime_error("unsafe completion asset path");
        d.width=r.count(4096);d.height=r.count(4096);
        if(!d.width || !d.height)throw std::runtime_error("empty completion scene");
        for(auto* point:{&d.playerEntry,&d.partnerEntry,&d.playerEnd,&d.partnerEnd}) {
            point->x=int(r.count(d.width-1));point->y=int(r.count(d.height-1));
        }
        result.destinations.push_back(std::move(d));
    }
    r.end();
    for(const auto& d:result.destinations)if(!result.names.count(d.terminalId))throw std::runtime_error("unknown completion destination ID");
    return result;
}
Point completionPosition(const CompletionDestination& d,const GameplayState& g,std::uint64_t now,bool partner) {
    const auto from=partner?d.partnerEntry:d.playerEntry,to=partner?d.partnerEnd:d.playerEnd;
    if(g.sceneBeat==SceneBeat::CompletionFadeIn)return from;
    if(g.sceneBeat!=SceneBeat::CompletionWalk)return to;
    const auto elapsed=std::min<std::uint64_t>(kCompletionWalkMs,now>g.phaseStartedMs?now-g.phaseStartedMs:0);
    return {from.x+int((int64_t(to.x)-from.x)*int64_t(elapsed)/int64_t(kCompletionWalkMs)),
            from.y+int((int64_t(to.y)-from.y)*int64_t(elapsed)/int64_t(kCompletionWalkMs))};
}
}
