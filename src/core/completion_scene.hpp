#pragma once
#include "core/exploration.hpp"
#include "core/surface.hpp"
#include <map>
namespace pet {
struct CompletionDestination {
    std::uint32_t dungeonId=0,terminalId=0,width=0,height=0;
    std::string name,assetFile;
    Point playerEntry{},partnerEntry{},playerEnd{},partnerEnd{};
};
struct CompletionRegistry {
    std::string romHash;
    std::map<std::uint32_t,std::string> names;
    std::vector<CompletionDestination> destinations;
    const CompletionDestination* destination(std::uint32_t) const;
    std::string mapLabel(std::uint32_t,std::uint32_t floor,bool completed) const;
};
CompletionRegistry loadCompletionRegistry(const std::filesystem::path&);
Point completionPosition(const CompletionDestination&,const GameplayState&,std::uint64_t now,bool partner=false);
}
