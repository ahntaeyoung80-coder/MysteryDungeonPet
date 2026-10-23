#include "core/exploration.hpp"
#include "core/exploration_io.hpp"
#include "legacy_snapshot.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace pet;
namespace {
void check(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
template<class F> void rejects(F f,const char* message) {
    bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}check(rejected,message);
}
void crc(std::vector<std::uint8_t>& bytes) {
    std::uint32_t value=0xffffffff;
    for(std::size_t i=0;i<bytes.size()-4;++i) {
        value^=bytes[i];for(unsigned b=0;b<8;++b)value=(value>>1)^((value&1)?0xedb88320U:0);
    }
    value=~value;for(unsigned b=0;b<4;++b)bytes[bytes.size()-4+b]=std::uint8_t(value>>(8*b));
}
void put(std::vector<std::uint8_t>& bytes,std::size_t at,std::uint32_t value) {
    for(unsigned b=0;b<4;++b)bytes.at(at+b)=std::uint8_t(value>>(8*b));
}
std::size_t varietyStart(const std::vector<std::uint8_t>& bytes) {
    detail::Reader tail(bytes);auto end=bytes.size()-4;
    for(unsigned version=bytes[4];version>12;--version){tail.offset=end-4;end-=tail.u32()+4;}
    tail.offset=end-4;return end-4-tail.u32();
}
ExplorationState fixture() {
    Explorer e(700,42,"variety",fixtureProfiles(2),{0,0,0,0});e.enableGameplay(1000);e.enableCompanion();
    auto s=e.state();s.gameplay.oran=1;s.variety.schedule.destination=DayDestination::Exploration;
    s.variety.schedule.dungeon=s.floors.front().dungeonId;return s;
}
ExplorationState active() {
    auto s=fixture();auto& v=s.variety;
    v.event.id={VarietyEvent::TiredExplorer,s.runId,9,s.gameplay.day,s.floorIndex};
    v.event.progress=EventProgress::Dialogue;v.event.variant="encourage-a";v.event.deadlineMs=60000;
    v.event.reservations={{0,ITEM_ORAN_BERRY,1}};
    v.dialogue.active=true;v.dialogue.deadlineMs=30000;v.dialogue.catalogueVersion=3;
    v.dialogue.context="event.tired";v.dialogue.bundle="encourage-a";v.dialogue.lineId="encourage-a.2";
    v.dialogue.line=1;v.dialogue.revealed=7;v.dialogue.readRemainingMs=1600;
    v.dialogue.speaker=kCompanionActorId;v.dialogue.listener=0;
    v.recent={{"event.tired","encourage-a",12}};return s;
}
ExplorationState restored(const ExplorationState& s) {return decodeSnapshot(encodeSnapshot(s),s.assetVersion);}
void randomStreams() {
    const auto domains=initialVarietyState(700,42,1);
    check(domains.eventRng==2209810602000010667ULL && domains.dialogueRng==17364260219026959114ULL &&
        domains.scheduleRng==1859451423405835267ULL,"fixed RNG derivation changed");
    const auto baseline=fixture();auto s=baseline;
    check(s.variety.eventRng!=s.variety.dialogueRng && s.variety.eventRng!=s.variety.scheduleRng,"RNG domains alias");
    for(unsigned i=0;i<1000;++i) {
        drawVariety(s.variety,VarietyRandomStream::Dialogue,23);
        drawVariety(s.variety,VarietyRandomStream::Event,17);
        drawVariety(s.variety,VarietyRandomStream::Schedule,11);
    }
    check(s.rng.state==baseline.rng.state,"variety consumed production RNG");
    Explorer left(baseline),right(s);
    for(unsigned i=0;i<100;++i) {
        const auto a=left.advanceGameplay(1000+i*400,true),b=right.advanceGameplay(1000+i*400,true);
        check(a.event==b.event && left.state().position==right.state().position &&
            left.state().rng.state==right.state().rng.state && left.state().world.terrain==right.state().world.terrain,"variety draws changed exploration");
    }
    auto copy=restored(s);
    for(auto stream:{VarietyRandomStream::Event,VarietyRandomStream::Dialogue,VarietyRandomStream::Schedule})
        check(drawVariety(s.variety,stream,100000)==drawVariety(copy.variety,stream,100000),"RNG restore changed selection");
    const auto unchanged=encodeSnapshot(s);
    rejects([&]{drawVariety(s.variety,VarietyRandomStream::Event,0);},"zero RNG range accepted");
    check(unchanged==encodeSnapshot(s),"failed selection consumed RNG");
}
void dialogueAndCancel() {
    auto s=active();const auto encoded=encodeSnapshot(s);auto copy=restored(s);
    check(encoded==encodeSnapshot(copy) && copy.variety.dialogue.revealed==7 && copy.variety.event.reservations.size()==1,"detailed restore lost state");
    check(nextGameplayDeadline(copy)==30000,"held session schedules past gameplay deadline");
    Explorer paused(copy);paused.shiftPausedClocks(120000,1000);const auto resumed=restored(paused.state());
    check(nextGameplayDeadline(resumed)==150000 && resumed.variety.event.deadlineMs==180000 &&
        resumed.variety.dialogue.readRemainingMs==1600,"pause lost reading time/event deadline");
    Explorer held(copy);const auto plans=held.bfsCalls();
    for(unsigned i=0;i<100;++i){held.advanceGameplay(1000+i,true);held.advance();}
    check(encodeSnapshot(held.state())==encoded && held.bfsCalls()==plans,"frame polling advanced dialogue, action or RNG");
    check(reconcileDialogue(copy,3,"encourage-a",{"encourage-a.1","encourage-a.2"}),"matching catalogue rejected");
    s.variety.event.progress=EventProgress::Result;
    check(commitVarietyResult(s,"encouraged"),"first result missing");s.variety.event.progress=EventProgress::Dialogue;
    for(unsigned mismatch=0;mismatch<3;++mismatch) {
        auto changed=s;
        check(!reconcileDialogue(changed,mismatch==0?4:3,mismatch==1?"other":"encourage-a",
            {"encourage-a.1",mismatch==2?"deleted":"encourage-a.2"}),"changed catalogue silently substituted");
        check(!varietyBusy(changed.variety) && changed.variety.event.progress==EventProgress::Abandoned &&
            changed.variety.event.reservations.empty() && changed.variety.committed==s.variety.committed && changed.gameplay.oran==1,"catalogue cancellation changed committed stock/result");
        check(!commitVarietyResult(changed,"encouraged"),"cancelled result replayed");
    }
    held=Explorer(s);held.startGuildMorning(2000);const auto morning=restored(held.state());
    check(morning.variety.event.progress==EventProgress::Abandoned && !morning.variety.dialogue.active &&
        morning.variety.schedule.destination==s.variety.schedule.destination && morning.variety.committed==s.variety.committed &&
        morning.variety.eventRng==s.variety.eventRng && morning.gameplay.oran==1 && morning.target==-1,"normal boot lost schedule/results or pending resources survived");
    held=Explorer(active());held.advanceGameplay(60001,true);
    check(!varietyBusy(held.state().variety),"expired event blocked exploration forever");
    auto low=active();low.gameplay.hearts=kLowHp;held=Explorer(low);held.advanceGameplay(2000,true);
    check(!varietyBusy(held.state().variety),"survival did not cancel optional conversation");
    auto system=active();system.variety.dialogue.speaker=kSystemDialogueActorId;
    check(restored(system).variety.dialogue.speaker==kSystemDialogueActorId,"system speaker cannot survive restore");
    auto ready=fixture();check(canStartVariety(ready,1000),"safe boundary unavailable");
    ready.gameplay.movement={1,1000,400,0,ready.position-24,ready.position};
    check(!canStartVariety(ready,1399) && canStartVariety(ready,1400),"movement completion boundary wrong");
    ready.gameplay.life=LifePhase::Combat;check(!canStartVariety(ready,5000),"combat accepted new event");
}
void durableResults(const std::filesystem::path& directory) {
    auto s=active();s.variety.event.progress=EventProgress::Result;
    std::filesystem::create_directories(directory);const auto path=directory/"result.mdes";
    saveSnapshot(path,s);auto before=loadSnapshot(path,s.assetVersion);
    check(commitVarietyResult(before,"encouraged"),"precommit retry missing");
    saveSnapshot(path,before);auto after=loadSnapshot(path,s.assetVersion);
    check(!commitVarietyResult(after,"encouraged") && after.gameplay.oran==1 && after.variety.committed.size()==1,"durable result duplicated or reservation consumed stock");
    const auto old=encodeSnapshot(after);auto invalid=after;invalid.variety.dialogue.speaker=kTiles+1;
    rejects([&]{saveSnapshot(path,invalid);},"invalid save replaced durable state");
    check(encodeSnapshot(loadSnapshot(path,s.assetVersion))==old,"failed save damaged previous state");
    after.variety.event.progress=EventProgress::Dialogue;
    rejects([&]{commitVarietyResult(after,"premature");},"success recorded before result boundary");
    cancelVariety(after);Explorer e(after);e.startGuildMorning(1000);
    for(unsigned tick=0;tick<100 && e.state().phase!=Phase::Explore;++tick)
        e.advanceGameplay(std::max<std::uint64_t>(1001+tick,e.state().gameplay.deadlineMs),true);
    check(e.state().runId==s.runId+1 && e.state().gameplay.day==1 && e.state().variety.committed==after.variety.committed &&
        e.state().variety.schedule.day==1 && e.state().variety.eventRng==after.variety.eventRng,"new Explorer discarded same-day state");
}
void scheduledPackage() {
    auto s=fixture();Explorer e(s);e.startGuildMorning(1000);
    auto next=fixtureProfiles(3);for(auto& floor:next)floor.dungeonId=17;
    e.setNextRunPackage("next-variety",next);
    for(unsigned tick=0;tick<100 && e.state().phase!=Phase::Explore;++tick)e.advanceGameplay(e.state().gameplay.deadlineMs,true);
    check(e.state().assetVersion=="variety" && e.state().variety.schedule.dungeon==s.floors.front().dungeonId,
        "same-day restart changed locked destination");
    e.resetExploration(100000);
    for(unsigned tick=0;tick<100 && e.state().gameplay.day==1;++tick)e.advanceGameplay(e.state().gameplay.deadlineMs,true);
    check(e.state().assetVersion=="next-variety" && e.state().floors.size()==3 &&
        e.state().variety.schedule.dungeon==17 && e.state().gameplay.day==2,"deferred package upgrade lost at day transition");
}
void mealsAndDays() {
    auto s=fixture();Explorer e(s);e.startGuildMorning(1000);
    for(unsigned tick=0;tick<100 && e.state().phase!=Phase::Explore;++tick)e.advanceGameplay(e.state().gameplay.deadlineMs,true);
    check(e.state().runId==701 && e.state().gameplay.day==1,"morning changed day");
    e.resetExploration(100000);
    for(unsigned tick=0;tick<50 && e.state().gameplay.life!=LifePhase::Eat;++tick)e.advanceGameplay(e.state().gameplay.deadlineMs,true);
    check(e.state().gameplay.meals==1 && e.state().variety.schedule.mealCommitted,"daily meal missing");
    s=restored(e.state());e=Explorer(s);e.startGuildMorning(200000);
    for(unsigned tick=0;tick<100 && e.state().phase!=Phase::Explore;++tick)e.advanceGameplay(e.state().gameplay.deadlineMs,true);
    check(e.state().gameplay.day==1 && e.state().gameplay.meals==1 && e.state().variety.schedule.mealCommitted,"restart lost daily meal commit");
    e.resetExploration(300000);
    for(unsigned tick=0;tick<50 && e.state().gameplay.life!=LifePhase::Eat;++tick)e.advanceGameplay(e.state().gameplay.deadlineMs,true);
    check(e.state().gameplay.meals==1,"same-day new run duplicated meal");
    for(unsigned tick=0;tick<100 && e.state().gameplay.day==1;++tick) {
        e=Explorer(restored(e.state()));e.advanceGameplay(e.state().gameplay.deadlineMs,true);
    }
    check(e.state().gameplay.day==2 && e.state().variety.schedule.day==2 && !e.state().variety.schedule.mealCommitted &&
        e.state().gameplay.meals==1,"next-day commit or schedule reset incorrect");
}
void malformedAndLegacy() {
    const auto valid=active();auto bytes=encodeSnapshot(valid);
    check(bytes[4]==20,"new schema missing");
    for(std::size_t size=0;size<bytes.size();size+=std::max<std::size_t>(1,bytes.size()/37)) {
        auto truncated=bytes;truncated.resize(size);
        rejects([&]{decodeSnapshot(truncated,"variety");},"truncation accepted");
    }
    auto damaged=bytes;put(damaged,damaged.size()-8,UINT32_MAX);crc(damaged);
    rejects([&]{decodeSnapshot(damaged,"variety");},"invalid variety block length accepted");
    detail::Reader r(bytes);r.offset=varietyStart(bytes)+24;
    const auto activeAt=r.offset;r.boolean();r.u64();r.u64();r.u32();r.u32();r.u32();r.u32();r.str();r.str();r.str();
    const auto speakerAt=r.offset;r.u32();r.u32();const auto typeAt=r.offset;
    for(auto mutation:{std::pair<std::size_t,std::uint32_t>{activeAt,2},{speakerAt,kTiles+1},{typeAt,11}}) {
        damaged=bytes;put(damaged,mutation.first,mutation.second);crc(damaged);
        rejects([&]{decodeSnapshot(damaged,"variety");},"unknown ID/boolean accepted");
    }
    for(unsigned field=0;field<9;++field) {
        auto invalid=valid;
        switch(field) {
        case 0:invalid.variety.event.id.run++;break;
        case 1:invalid.variety.event.actors={kCompanionActorId};break;
        case 2:invalid.variety.event.reservations.push_back(invalid.variety.event.reservations.front());break;
        case 3:invalid.variety.schedule.day++;break;
        case 4:invalid.variety.committed.insert("event:1:700:1:0:9:bad:result");break;
        case 5:invalid.variety.recent.push_back(invalid.variety.recent.front());break;
        case 6:invalid.variety.dialogue.catalogueVersion=0;break;
        case 7:invalid.variety.event.deadlineMs=UINT64_MAX;break;
        case 8:invalid.variety.dialogue.deadlineMs=UINT64_MAX;break;
        }
        rejects([&]{encodeSnapshot(invalid);},"invalid cross-reference accepted");
    }
    auto legacy=fixture();legacy.gameplay.oran=0; // A spent berry acquisition is never restocked.
    legacy.inventory={{"pickup:699:0:1",ITEM_ORAN_BERRY}};legacy.onceKeys.insert("pickup:699:0:1");
    auto old=encodeSnapshot(legacy);removeVarietyBlock(old);
    for(unsigned schema:{9,10,11}) {
        old[4]=schema;crc(old);const auto migrated=decodeSnapshot(old,"variety");
        check(!varietyBusy(migrated.variety) && migrated.variety.committed.empty() && migrated.gameplay.oran==0 &&
            migrated.rng.state==legacy.rng.state && migrated.inventory.size()==1,"legacy import replayed a result or restocked berry");
    }
    auto eat=fixture();Explorer diner(eat);diner.resetExploration(1000);
    for(unsigned tick=0;tick<50 && diner.state().gameplay.life!=LifePhase::Eat;++tick)diner.advanceGameplay(diner.state().gameplay.deadlineMs,true);
    old=encodeSnapshot(diner.state());removeVarietyBlock(old);crc(old);
    check(decodeSnapshot(old,"variety").variety.schedule.mealCommitted,"legacy consumed meal not migrated");
}
}
int main(int argc,char** argv) {try {
    randomStreams();dialogueAndCancel();durableResults(argc>1?argv[1]:"variety-test-output");scheduledPackage();mealsAndDays();malformedAndLegacy();
    std::cout<<"variety: RNG isolation/replay, hold/cancel/timeout, atomic result retry, catalogue mismatch, same-day handoff/meal/day, MDES9..12 corruption/migration pass\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
