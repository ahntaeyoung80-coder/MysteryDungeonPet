#include "core/gameplay_notice.hpp"
#include "core/dialogue_catalogue.hpp"
#include "core/exploration_io.hpp"
#include "event_test_fixture.hpp"
#include <iostream>
#include <stdexcept>
using namespace pet;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
ExplorationState roundTrip(const ExplorationState& s){return decodeSnapshot(encodeSnapshot(s),s.assetVersion);}
int main(){try {
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path()/"assets/original";
    const auto package=loadExplorationPackage(root/"exploration/dungeon-15/exploration.mdpe");
    const auto catalogue=loadDialogueCatalogue(root/"gameplay/dialogue/catalogue.mdlc");
    auto base=eventFixture(package);base.population.entities.clear();base.gameplay.independentParty=true;
    // Every selected item, including newly selected expeditions, has a product name.
    for(const auto& clip:loadPackage(root/"gameplay/items/items.mdpa").clips) {
        const auto name=itemDisplayName(clip.id);
        check(name!=L"이름 모를 도구" && name.find(L"#")==std::wstring::npos,"selected item lacks a real display name");
    }
    // All floor item identities have names fitting the dialogue row.
    for(unsigned dungeon:{0U,15U,18U,1U,4U,8U,14U,17U,21U,35U,41U})for(const auto& floor:loadExplorationPackage(root/"exploration"/("dungeon-"+std::to_string(dungeon))/"exploration.mdpe").floors)
        for(const auto& category:floor.floorItems)if(category.weight)for(const auto& item:category.items)if(item.value){
            const auto name=itemDisplayName(item.originalId);check(name.rfind(L"아이템 #",0)!=0,"spawnable item missing name");
            unsigned width=0;for(auto c:name)width+=c<128?1:2;check(width<=26,"item name overflows dialogue row");}
    for(bool independent:{false,true})for(unsigned item:{70U,109U,188U,183U}) {
        auto s=base;s.position=125;s.visited[s.position]=1;s.gameplay.companion.tile=124;s.gameplay.independentParty=independent;s.population.entities={{1,item,EntityKind::Item,s.position,false}};
        s.target=s.position;s.targetInstance=1;s.goalKind=GoalKind::Item;
        Explorer live(s);auto update=live.advanceGameplay(1000,ActorCompletion{true,true,true},catalogue);
        check(update.event==GameplayEvent::Pickup && live.state().inventory.size()==1,"actual pickup boundary not reached");
        const auto& notice=live.state().variety.notices.front();
        check(notice.kind==NoticeKind::Pickup && notice.item==item,"notice differs from acquired item");
        check(catalogueText(live.state(),catalogue).find(itemDisplayName(item))==0 && catalogueText(live.state(),catalogue).find(L"주웠다.")!=std::wstring::npos && catalogueText(live.state(),catalogue).find(L"\n")==std::wstring::npos,"pickup has no named text box");
        auto saved=roundTrip(live.state());const auto count=saved.variety.notices.size();const auto stock=stockQuantity(saved,0,item)+stockQuantity(saved,kCompanionActorId,item);
        restoreGameplayNoticeClock(saved,50000);check(activeGameplayNotice(saved) && saved.variety.notices.size()==count,"notice lost on resume");
        Explorer resumed(saved);resumed.advanceGameplay(50000,ActorCompletion{},catalogue);
        check(resumed.state().inventory.size()==1 && stockQuantity(resumed.state(),0,item)+stockQuantity(resumed.state(),kCompanionActorId,item)==stock,"notice replayed pickup effect");
    }
    {auto s=base;queueGameplayNotice(s,NoticeKind::Pickup,70);updateGameplayNotices(s,1000);
     check(revealedGameplayNotice(s,1000).size()==1 && revealedGameplayNotice(s,1800)==gameplayNoticeText(s),"typewriter broken");
     const auto original=s.variety.notices.front().deadlineMs;Explorer paused(s);paused.shiftPausedClocks(10000,1100);
     check(roundTrip(paused.state()).variety.notices.front().deadlineMs==original+10000,"pause loses notice reading time");
     s.gameplay.life=LifePhase::Dialogue;s.gameplay.enemyInstance=kCompanionActorId;updateGameplayNotices(s,1100);
     check(!activeGameplayNotice(s) && !s.variety.notices.front().startedMs,"notice overlaps conversation");}
    for(unsigned item:{70U,109U,110U,111U,73U,105U,183U,362U,314U,318U,330U,312U,188U,119U,17U,69U,1U}) {
        auto s=base;queueGameplayNotice(s,NoticeKind::ItemComment,item);updateGameplayNotices(s,1000);
        check(!gameplayNoticeText(s).empty() && gameplayNoticeText(s).find(L"\n")==std::wstring::npos,"item reaction missing or multiline");
        const auto face=gameplayNoticePortraits(s);
        check(face.speaker==PortraitRole::Partner,"item reaction lacks companion speaker");
        if(item==314 || item==318 || item==330 || item==312)check(face.emotion==PortraitEmotion::Inspired,"orb reaction does not have sparkling eyes");
        if(item==111 || item==105)check(face.emotion==PortraitEmotion::Worried,"suspicious item has cheerful reaction");
    }
    unsigned comments=0;
    for(unsigned seed=0;seed<1000;++seed) {
        auto s=base;s.variety.dialogueRng=seed;const auto world=s.rng.state,event=s.variety.eventRng;
        queueGameplayNotice(s,NoticeKind::Pickup,70);updateGameplayNotices(s,1000);
        if(s.variety.notices.size()==2) {
            ++comments;const auto deadline=s.variety.notices.front().deadlineMs;updateGameplayNotices(s,deadline);
            check(cataloguePortraits(s,catalogue).speaker==PortraitRole::Partner && catalogueText(s,catalogue).find(L"지치면")!=std::wstring::npos,"item comment has wrong speaker/content");
            check(roundTrip(s).variety.notices.front().kind==NoticeKind::ItemComment,"comment choice not persisted");
            s.population.entities={{1,237,EntityKind::Friend,101,false}};s.gameplay.hostile={1};updateGameplayNotices(s,deadline+1);
            check(s.variety.notices.empty(),"optional comment interrupts nearby combat");
        }
        check(world==s.rng.state && event==s.variety.eventRng,"comment changed combat/event RNG");
    }
    check(comments>150 && comments<250,"occasional pickup comments not near 20 percent");
    // A KO is reported after its faint playback, never at damage impact or on restoration polling.
    {auto s=base;auto& g=s.gameplay;s.population.entities={{1,237,EntityKind::Friend,101,false}};g.hostile={1};s.population.entities[0].remainingHits=0;
     g.partyPhase=PartyPhase::Player;g.partyPlaybacks={{1,g.actionRound,1000,2000,1,33,6}};g.nextPartyId=2;
     Explorer live(s);live.advanceGameplay(1500,ActorCompletion{},catalogue);
     check(live.state().variety.notices.empty(),"victory reported before faint ends");
     live.advanceGameplay(2000,ActorCompletion{},catalogue);
     check(live.state().population.entities[0].done && live.state().variety.notices.size()==1 && live.state().variety.notices.front().kind==NoticeKind::Victory,"finished KO missing notice");
     check(catalogueText(live.state(),catalogue)==L"적을 쓰러뜨렸다!","victory text missing");
     auto saved=roundTrip(live.state());Explorer resumed(saved);resumed.advanceGameplay(2000,ActorCompletion{},catalogue);
     check(resumed.state().variety.notices.size()==1,"restored faint duplicated notice");}
    for(unsigned hp:{1U,2U,3U}) {
        auto s=base;s.gameplay.hearts=hp;s.gameplay.oran=1;Explorer live(s);live.advanceGameplay(1000,ActorCompletion{},catalogue);
        check(live.state().gameplay.hearts==kMaxHp && !live.state().gameplay.oran,"Oran not full in one action");
        check(catalogueText(live.state(),catalogue).find(L"모두 회복")!=std::wstring::npos && catalogueText(live.state(),catalogue).find(L"2하트")==std::wstring::npos,"stale partial-heal text");
        auto saved=roundTrip(live.state());Explorer resumed(saved);resumed.advanceGameplay(1100,ActorCompletion{},catalogue);
        check(resumed.state().gameplay.hearts==kMaxHp && !resumed.state().gameplay.oran,"heal replay changed HP/stock");
    }
    // Real MDES16 shape: the new bounded extension is absent, rather than version bytes relabelled alone.
    {auto s=base;queueGameplayNotice(s,NoticeKind::Pickup,70);updateGameplayNotices(s,1000);auto bytes=encodeSnapshot(s);
     while(bytes[4]>16){detail::Reader r(bytes);r.offset=bytes.size()-8;const auto size=r.u32();bytes.erase(bytes.end()-8-size,bytes.end()-4);--bytes[4];}
     std::uint32_t crc=0xffffffffU;for(std::size_t i=0;i<bytes.size()-4;++i){crc^=bytes[i];for(unsigned bit=0;bit<8;++bit)crc=(crc>>1)^((crc&1)?0xedb88320U:0);}
     crc=~crc;for(unsigned i=0;i<4;++i)bytes[bytes.size()-4+i]=std::uint8_t(crc>>(i*8));
     check(decodeSnapshot(bytes,s.assetVersion).variety.notices.empty(),"MDES16 import invents notices");
     s.variety.notices[0].kind=NoticeKind(0);bool rejected=false;try{encodeSnapshot(s);}catch(...){rejected=true;}check(rejected,"invalid notice enum accepted");}
    std::cout<<"named real pickups, KO boundary/once, full Oran text, "<<comments<<"/1000 comments, safety/RNG isolation, pause/restore/MDES16 migration pass\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
