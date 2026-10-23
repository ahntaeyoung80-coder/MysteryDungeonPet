#include "core/exploration_package.hpp"
#include <iostream>
#include <map>
#include <stdexcept>
using namespace pet;
ExplorationState read(const char* file) {
 for(unsigned root:{1,4,8,14,17,21,35,41}) {
  auto p=loadExplorationPackage("assets/original/exploration/dungeon-"+std::to_string(root)+"/exploration.mdpe");
  try{return loadSnapshot(file,p.assetVersion);}
  catch(const std::runtime_error& e){if(std::string(e.what())!="snapshot asset version mismatch")throw;}
 }
 throw std::runtime_error("unknown snapshot package");
}
int main(int argc,char** argv){try{
 if(argc!=3)throw std::runtime_error("provide before and after snapshots");
 const auto a=read(argv[1]),b=read(argv[2]);std::map<std::string,unsigned> inventory;
 for(const auto& i:b.inventory)inventory.emplace(i.onceKey,i.originalId);
 for(const auto& i:a.inventory)if(!inventory.count(i.onceKey) || inventory.at(i.onceKey)!=i.originalId)throw std::runtime_error("prior item missing or changed");
 if(a.gameplay.day!=b.gameplay.day || a.gameplay.meals!=b.gameplay.meals)throw std::runtime_error("relaunch changed day/meals");
 if(a.floors.front().dungeonId==b.floors.front().dungeonId)throw std::runtime_error("relaunch did not select another root");
 std::cout<<"{\"pass\":true,\"day\":"<<b.gameplay.day<<",\"meals\":"<<b.gameplay.meals<<",\"prior_items_preserved\":"<<a.inventory.size()<<",\"after_items\":"<<b.inventory.size()<<",\"before_oran\":["<<a.gameplay.oran<<','<<a.gameplay.companion.oran<<"],\"after_oran\":["<<b.gameplay.oran<<','<<b.gameplay.companion.oran<<"]}\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
