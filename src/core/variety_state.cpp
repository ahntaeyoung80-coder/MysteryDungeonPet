#include "core/exploration.hpp"
#include <algorithm>
#include <charconv>
#include <stdexcept>

namespace pet {
namespace {
void require(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
bool textId(const std::string& value) {
    return !value.empty() && value.size()<=128 && std::all_of(value.begin(),value.end(),[](unsigned char c) {
        return (c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='.' || c=='_' || c=='-';
    });
}
bool actor(const ExplorationState& s,std::uint32_t id) {
    if(id==0)return true;
    if(id==kCompanionActorId)return s.gameplay.companion.enabled;
    return id>0 && id<=kTiles && std::any_of(s.population.entities.begin(),s.population.entities.end(),[&](const auto& e) {
        return e.instanceId==id && e.kind==EntityKind::Friend && e.visible;
    });
}
std::uint64_t mix(std::uint64_t value) {
    value=(value^(value>>30))*0xbf58476d1ce4e5b9ULL;
    value=(value^(value>>27))*0x94d049bb133111ebULL;
    return value^(value>>31);
}
bool live(EventProgress p) {return p>=EventProgress::Approach && p<=EventProgress::Dismiss;}
}
std::string eventKey(const EventIdentity& id) {
    require(id.type>=VarietyEvent::TiredExplorer && id.type<=VarietyEvent::Holiday && id.run && id.day && id.floor<128 && id.trigger,
            "invalid event identity");
    return "event:"+std::to_string(static_cast<unsigned>(id.type))+":"+std::to_string(id.run)+":"+
        std::to_string(id.day)+":"+std::to_string(id.floor)+":"+std::to_string(id.trigger);
}
VarietyState initialVarietyState(std::uint64_t run,std::uint64_t seed,std::uint32_t day) {
    VarietyState result;
    // Fixed numeric domain salts; no std::hash or draws from map/combat RNG.
    const auto root=mix(seed^mix(run));
    result.eventRng=mix(root^0x4556454e545f3031ULL);
    result.dialogueRng=mix(root^0x4449414c4f475545ULL);
    result.scheduleRng=mix(root^0x5343484544554c45ULL);
    result.schedule.day=day;
    return result;
}
bool varietyBusy(const VarietyState& v) {return v.dialogue.active || live(v.event.progress);}
std::uint64_t nextGameplayDeadline(const ExplorationState& s) {
    const auto& v=s.variety;
    auto deadline=UINT64_MAX;
    if(v.dialogue.active)deadline=v.dialogue.deadlineMs;
    if(live(v.event.progress))deadline=std::min(deadline,v.event.deadlineMs);
    return varietyBusy(v)?deadline:s.gameplay.deadlineMs;
}
std::uint64_t drawVariety(VarietyState& v,VarietyRandomStream stream,std::uint64_t bound) {
    require(bound>0,"empty variety random range");
    std::uint64_t* state=nullptr;
    switch(stream) {
    case VarietyRandomStream::Event:state=&v.eventRng;break;
    case VarietyRandomStream::Dialogue:state=&v.dialogueRng;break;
    case VarietyRandomStream::Schedule:state=&v.scheduleRng;break;
    default:throw std::runtime_error("unknown variety RNG stream");
    }
    Rng rng{*state};const auto value=rng.bounded(bound);*state=rng.state;return value;
}
bool canStartVariety(const ExplorationState& s,std::uint64_t now) {
    const auto& g=s.gameplay;
    return !varietyBusy(s.variety) && s.phase==Phase::Explore && g.enabled && g.life==LifePhase::Explore &&
        g.sceneBeat==SceneBeat::None && g.hearts>kLowHp && (!g.companion.enabled || g.companion.hearts>kLowHp) &&
        g.partyActions.empty() && g.partyPlaybacks.empty() && now>=g.deadlineMs &&
        (!g.movement.actionId || (now>=g.movement.startedMs && now-g.movement.startedMs>=g.movement.durationMs)) &&
        (!g.companion.movement.actionId || (now>=g.companion.movement.startedMs && now-g.companion.movement.startedMs>=g.companion.movement.durationMs)) &&
        std::none_of(s.population.entities.begin(),s.population.entities.end(),[&](const auto& e) {
            return e.visible && !e.done && std::find(g.hostile.begin(),g.hostile.end(),e.instanceId)!=g.hostile.end() &&
                (canAttack(s.world,s.position,e.tile) || (g.companion.enabled && canAttack(s.world,g.companion.tile,e.tile)));
        });
}
void cancelVariety(ExplorationState& s) {
    auto& v=s.variety;
    const bool busy=varietyBusy(v);
    if(live(v.event.progress))v.event.progress=EventProgress::Abandoned;
    v.event.reservations.clear();v.event.actors.clear();v.dialogue={};
    if(busy) {s.target=-1;s.targetInstance=0;s.goalKind=GoalKind::None;}
}
bool commitVarietyResult(ExplorationState& s,const std::string& result) {
    require(textId(result),"invalid event result id");
    validateVarietyState(s);
    auto& v=s.variety;
    const auto key=eventKey(v.event.id)+":"+result;
    if(v.committed.count(key))return false;
    require(v.event.progress==EventProgress::Result,"event result before actual result boundary");
    require(v.committed.size()<4096,"event result journal full");
    v.committed.insert(key);
    return true;
}
bool reconcileDialogue(ExplorationState& s,std::uint32_t version,const std::string& bundle,const std::vector<std::string>& ids) {
    auto& d=s.variety.dialogue;
    if(!d.active)return true;
    if(d.catalogueVersion==version && d.bundle==bundle && d.line<ids.size() && ids[d.line]==d.lineId)return true;
    d={}; // Committed event results are independent of catalogue validity.
    if(live(s.variety.event.progress))cancelVariety(s);
    return false;
}
void validateVarietyState(const ExplorationState& s) {
    const auto& v=s.variety;const auto& e=v.event;const auto& d=v.dialogue;
    require(v.schedule.day==s.gameplay.day && static_cast<unsigned>(v.schedule.destination)<=3,"invalid day schedule");
    require(!v.schedule.mealCommitted || s.gameplay.meals>0,"daily meal without committed count");
    require(v.schedule.destination==DayDestination::Exploration || v.schedule.dungeon==0,"dungeon on non-exploration day");
    require(static_cast<unsigned>(e.progress)<=7 && static_cast<unsigned>(e.id.type)<=10,"invalid variety enum");
    if(e.progress==EventProgress::None) {
        require(e.id.type==VarietyEvent::None && !e.id.run && !e.id.day && !e.id.floor && !e.id.trigger && e.variant.empty() && !e.startedMs && !e.deadlineMs && e.actors.empty() && e.reservations.empty(),"nonempty inactive event");
    } else {
        (void)eventKey(e.id);
        require(textId(e.variant) && e.deadlineMs>e.startedMs && e.deadlineMs-e.startedMs<=300000,"invalid event variant/deadline");
        require(e.id.run<=s.runId && e.id.day<=s.gameplay.day,"future event identity");
        if(live(e.progress))require(e.id.run==s.runId && e.id.day==s.gameplay.day && e.id.floor==s.floorIndex && s.phase==Phase::Explore,"stale live event");
        else require(e.actors.empty() && e.reservations.empty(),"finished event owns pending resources");
    }
    require(e.actors.size()<=kTiles && e.reservations.size()<=16,"event resources exceed limit");
    std::set<std::uint32_t> actors;
    for(auto id:e.actors)require(id && id!=kCompanionActorId && actor(s,id) && actors.insert(id).second,"invalid event actor");
    std::set<std::pair<std::uint32_t,std::uint32_t>> reserved;
    for(const auto& r:e.reservations)require(actor(s,r.owner) && r.item==ITEM_ORAN_BERRY && r.quantity==1 &&
        (r.owner==0 || r.owner==kCompanionActorId) && reserved.insert({r.owner,r.item}).second &&
        (r.owner==0?s.gameplay.oran:s.gameplay.companion.oran)>=r.quantity,"invalid event reservation");
    if(d.active)require(d.deadlineMs>d.startedMs && d.deadlineMs-d.startedMs<=60000 && d.catalogueVersion>0 && textId(d.context) && textId(d.bundle) && textId(d.lineId) &&
        d.line<256 && d.revealed<=4096 && d.readRemainingMs<=60000 && (d.speaker==kSystemDialogueActorId || actor(s,d.speaker)) && actor(s,d.listener) &&
        d.speaker!=d.listener,"invalid dialogue session");
    else require(!d.startedMs && !d.deadlineMs && !d.catalogueVersion && !d.line && !d.revealed && !d.readRemainingMs && d.context.empty() && d.bundle.empty() && d.lineId.empty() && !d.speaker && !d.listener,"nonempty inactive dialogue");
    require(v.recent.size()<=64 && v.committed.size()<=4096,"variety history exceeds limit");
    std::set<std::string> contexts;
    for(const auto& r:v.recent)require(textId(r.context) && textId(r.bundle) && contexts.insert(r.context).second,"invalid recent dialogue");
    for(const auto& key:v.committed) {
        require(key.size()<=256 && key.rfind("event:",0)==0,"invalid event commit key");
        std::uint64_t numbers[5]{};std::size_t start=6;
        for(auto& number:numbers) {
            const auto end=key.find(':',start);require(end!=std::string::npos && end>start,"invalid event commit component");
            const auto parsed=std::from_chars(key.data()+start,key.data()+end,number);
            require(parsed.ec==std::errc{} && parsed.ptr==key.data()+end,"invalid event commit number");
            start=end+1;
        }
        require(numbers[0]>=1 && numbers[0]<=10 && numbers[1]>0 && numbers[1]<=s.runId &&
            numbers[2]>0 && numbers[2]<=s.gameplay.day && numbers[3]<128 && numbers[4]>0 && textId(key.substr(start)),"invalid event commit identity/result");
        const EventIdentity id{static_cast<VarietyEvent>(numbers[0]),numbers[1],numbers[4],static_cast<std::uint32_t>(numbers[2]),static_cast<std::uint32_t>(numbers[3])};
        require(key==eventKey(id)+":"+key.substr(start),"noncanonical event commit key");
    }
}
} // namespace pet
