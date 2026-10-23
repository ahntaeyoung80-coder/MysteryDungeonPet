#include "core/dialogue_catalogue.hpp"
#include "event_test_fixture.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace pet;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
void crc(std::vector<uint8_t>& b) {
    uint32_t c=0xffffffffU;
    for(size_t i=0;i<b.size()-4;++i){c^=b[i];for(unsigned j=0;j<8;++j)c=(c>>1)^((c&1)?0xedb88320U:0);}
    c=~c;for(unsigned j=0;j<4;++j)b[b.size()-4+j]=uint8_t(c>>(8*j));
}
size_t stockOffset(const ExplorationState& base,bool partner) {
    auto one=base;if(partner)one.gameplay.companion.oran=1;else one.gameplay.oran=1;
    const auto a=encodeSnapshot(base),b=encodeSnapshot(one);
    size_t offset=a.size();unsigned count=0;
    for(size_t i=0;i<a.size()-4;++i)if(a[i]!=b[i]){offset=i;++count;}
    check(count==1,"could not isolate legacy stock field");return offset;
}
int main(){try {
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path()/"assets/original";
    const auto p=loadExplorationPackage(root/"exploration/dungeon-15/exploration.mdpe");
    const auto c=loadDialogueCatalogue(root/"gameplay/dialogue/catalogue.mdlc");
    auto base=eventFixture(p);base.population.entities.clear();
    for(bool independent:{false,true})for(unsigned own:{0U,1U})for(unsigned partner:{0U,1U}) {
        auto s=base;s.position=125;s.visited[s.position]=1;s.gameplay.companion.tile=124;
        s.gameplay.independentParty=independent;s.gameplay.oran=own;s.gameplay.companion.oran=partner;
        s.population.entities={{1,70,EntityKind::Item,s.position,false}};
        s.target=s.position;s.targetInstance=1;s.goalKind=GoalKind::Item;
        Explorer e(s);e.advanceGameplay(1000,ActorCompletion{true,true,true},c);
        const auto after=e.state();const auto total=after.gameplay.oran+after.gameplay.companion.oran;
        check(after.gameplay.oran<=1 && after.gameplay.companion.oran<=1,"pickup stacked berries");
        if(own && partner)check(total==2 && after.inventory.empty() && !after.population.entities[0].done,"full party removed floor berry");
        else check(total==own+partner+1 && after.inventory.size()==1 && after.population.entities[0].done,"empty holder did not receive one berry");
        Explorer restored(decodeSnapshot(encodeSnapshot(after),p.assetVersion));
        restored.advanceGameplay(1000,ActorCompletion{},c);
        check(restored.state().gameplay.oran+restored.state().gameplay.companion.oran==total,"restore duplicated berry pickup");
    }
    // A full solo actor must leave the item; a missing partner is not an extra slot.
    {auto s=base;s.gameplay.companion={};s.gameplay.oran=1;
     check(!canReceiveOran(s) && !receiveOran(s) && s.gameplay.oran==1,"solo actor used absent companion capacity");}
    // Shared stock API enforces the same cap without changing ordinary item limits.
    {auto s=base;check(changeStock(s,0,70,1) && !changeStock(s,0,70,1),"stock API allowed a second berry");
     check(changeStock(s,kCompanionActorId,70,1) && !changeStock(s,kCompanionActorId,70,1),"partner stock API allowed stacking");
     check(changeStock(s,0,109,3) && stockQuantity(s,0,109)==3,"berry limit changed apples");}
    // Import genuine MDES18 legacy field values, keeping its checksum valid.
    const auto ownOffset=stockOffset(base,false),partnerOffset=stockOffset(base,true);
    auto bytes=encodeSnapshot(base);
    // Drop the MDES19 variant extension to exercise an actual MDES18 import.
    while(bytes[4]>18){uint32_t length=0;for(unsigned j=0;j<4;++j)length|=uint32_t(bytes[bytes.size()-8+j])<<(8*j);bytes.erase(bytes.end()-8-length,bytes.end()-4);--bytes[4];}
    bytes[ownOffset]=99;bytes[partnerOffset]=2;crc(bytes);
    const auto imported=decodeSnapshot(bytes,p.assetVersion);
    check(imported.gameplay.oran==1 && imported.gameplay.companion.oran==1 && imported.runId==base.runId,"legacy stacks not normalized on load");
    check(decodeSnapshot(encodeSnapshot(imported),p.assetVersion).gameplay.oran==1,"normalized save not stable");
    auto raw=base;raw.gameplay.oran=99;raw.gameplay.companion.oran=2;
    Explorer normalized(raw);check(normalized.state().gameplay.oran==1 && normalized.state().gameplay.companion.oran==1,"in-memory legacy import bypassed cap");
    bool rejected=false;try{(void)encodeSnapshot(raw);}catch(...){rejected=true;}check(rejected,"writer accepted stacked berries");
    bytes[ownOffset]=100;crc(bytes);rejected=false;try{(void)decodeSnapshot(bytes,p.assetVersion);}catch(...){rejected=true;}check(rejected,"invalid old count silently accepted");
    std::cout<<"single berry per actor: pickup allocation/full-floor preservation/solo/restore/stock API/legacy 99+2 migration pass\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
