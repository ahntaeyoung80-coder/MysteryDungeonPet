#include "core/exploration.hpp"
#include "core/package.hpp"
#include "core/exploration_io.hpp"
#include <iostream>
int main(int argc,char** argv) {try {
 if(argc!=2)return 2;
 auto bytes=pet::readFile(argv[1],8*1024*1024);
 pet::detail::Reader header(bytes);header.u32();auto version=header.u32();auto asset=header.str();
 const auto s=pet::decodeSnapshot(bytes,asset);const auto& v=s.variety;
 std::cout<<"{\"schema\":"<<version<<",\"run\":"<<s.runId<<",\"day\":"<<s.gameplay.day
 <<",\"meals\":"<<s.gameplay.meals<<",\"oran\":"<<s.gameplay.oran<<",\"life\":"<<unsigned(s.gameplay.life)
 <<",\"scheduleDay\":"<<v.schedule.day<<",\"destination\":"<<unsigned(v.schedule.destination)
 <<",\"dungeon\":"<<v.schedule.dungeon<<",\"eventRng\":\""<<v.eventRng
 <<"\",\"dialogueRng\":\""<<v.dialogueRng<<"\",\"scheduleRng\":\""<<v.scheduleRng
 <<"\",\"results\":"<<v.committed.size()<<",\"busy\":"<<pet::varietyBusy(v)<<"}\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
