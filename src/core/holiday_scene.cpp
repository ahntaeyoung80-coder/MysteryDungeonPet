#include "core/holiday_scene.hpp"
#include "core/exploration_io.hpp"
#include <algorithm>
#include <cmath>
namespace pet {
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
unsigned routeId(HolidayBeat b) {
    if(b==HolidayBeat::WestWalk || b==HolidayBeat::WestVisit)return 1;
    if(b==HolidayBeat::WestExit || b==HolidayBeat::WestFade)return 2;
    if(b==HolidayBeat::EastWalk || b==HolidayBeat::EastDepart)return 3;
    if(b==HolidayBeat::SpringWalk || b==HolidayBeat::SpringVisit)return 4;
    if(b==HolidayBeat::BathEnter || b==HolidayBeat::BathTalk || b==HolidayBeat::BathRest)return 5;
    if(b==HolidayBeat::BathExit || b==HolidayBeat::SpringDepart)return 6;
    return 0;
}
bool walking(HolidayBeat b){return b==HolidayBeat::PathWalk || b==HolidayBeat::WestWalk || b==HolidayBeat::WestExit || b==HolidayBeat::EastWalk || b==HolidayBeat::SpringWalk || b==HolidayBeat::BathEnter || b==HolidayBeat::BathExit;}
std::uint64_t duration(const HolidayRoute& r){return std::max<std::uint64_t>(400,(std::max(r.player.size(),r.partner.size())-1)*8000/60);}
std::pair<Point,unsigned> position(const std::vector<Point>& path,std::uint64_t elapsed) {
    const auto step=std::min<std::size_t>(path.size()-1,elapsed*60/8000);if(step+1==path.size())return {path.back(),0};
    const auto from=path[step],to=path[step+1];const auto fraction=(elapsed*60)%8000;
    const int dx=to.x-from.x,dy=to.y-from.y;
    const auto dir=dx<0?6U:dx>0?2U:dy<0?4U:0U;
    return {{from.x+int(dx*std::int64_t(fraction)/8000),from.y+int(dy*std::int64_t(fraction)/8000)},dir};
}
void beat(ExplorationState& s,HolidayBeat b,const HolidayRegistry& registry,std::uint64_t now,std::uint64_t ms=0) {
    auto& h=s.variety.holiday;h.beat=b;h.startedMs=now;h.elapsedMs=0;
    if(!ms)ms=walking(b)?duration(registry.route(routeId(b))):400;
    h.deadlineMs=now+ms;s.gameplay.phaseStartedMs=now;s.gameplay.deadlineMs=h.deadlineMs;
}
bool speak(ExplorationState& s,const DialogueCatalogue& c,const HolidayRegistry& r,HolidayBeat b,const std::string& stage,std::uint64_t now) {
    beat(s,b,r,now);s.variety.event.progress=EventProgress::Dialogue;
    if(!startDialogueBundle(s,c,s.variety.event.variant+"."+stage,now))return false;
    s.variety.holiday.deadlineMs=s.variety.dialogue.deadlineMs;return true;
}
}
const HolidayRoute& HolidayRegistry::route(unsigned id) const {const auto i=routes.find(id);if(i==routes.end())throw std::runtime_error("holiday route missing");return i->second;}
HolidayRegistry loadHolidayRegistry(const std::filesystem::path& root) {
    HolidayRegistry result;const auto bytes=readFile(root/"routes.mdhr",128*1024);detail::Reader r(bytes);
    require(r.u32()==0x5248444d && r.u32()==1,"unknown holiday route schema");const auto n=r.count(16);
    for(unsigned i=0;i<n;++i) {
        HolidayRoute route;route.id=r.count(15);route.scene=r.str();route.width=r.count(4096);route.height=r.count(4096);
        require(route.width>=200 && route.height>=200 && (route.scene=="P01P01A" || route.scene=="T01P01A" || route.scene=="T01P02A" || route.scene=="P02P01A"),"unverified holiday ground");
        for(auto* path:{&route.player,&route.partner}) {
            const auto count=r.count(1024);require(count>0,"empty holiday path");
            for(unsigned j=0;j<count;++j){Point p{int(r.count(route.width-1)),int(r.count(route.height-1))};if(!path->empty())require(std::abs(p.x-path->back().x)+std::abs(p.y-path->back().y)==8,"invalid holiday path segment");path->push_back(p);}
        }
        require(result.routes.emplace(route.id,std::move(route)).second,"duplicate holiday route");
    }
    r.end();for(unsigned i=0;i<7;++i)(void)result.route(i);
    const auto floats=readFile(root/"float.mdhs",65536);detail::Reader f(floats);
    require(f.u32()==0x5348444d && f.u32()==2,"unknown holiday Float schema");const auto count=f.count(64);
    for(unsigned i=0;i<count;++i) {
        HolidayFloat body;body.md=f.count(65535);body.sprite=f.count(65535);body.manifest=f.str();body.key=f.str();body.hash=f.str();
        require(body.md && body.manifest=="float/"+std::to_string(body.sprite)+"/asset.mdpa" && !body.key.empty() && body.hash.size()==64 && result.floats.emplace(body.md,std::move(body)).second,"invalid holiday Float binding");
    }
    f.end();return result;
}
HolidaySample holidaySample(const ExplorationState& s,const HolidayRegistry& registry,std::uint64_t now) {
    const auto& h=s.variety.holiday;auto b=h.beat;
    if(b==HolidayBeat::ReturnFade)b=s.variety.schedule.destination==DayDestination::Town?HolidayBeat::EastDepart:HolidayBeat::SpringDepart;
    const auto& route=registry.route(routeId(b));HolidaySample result;result.scene=route.scene;
    const auto elapsed=now>h.startedMs?now-h.startedMs:0;result.moving=walking(b);
    if(b==HolidayBeat::Morning){result.player=route.player.front();result.partner=route.partner.front();}
    else if(result.moving){auto p=position(route.player,elapsed),q=position(route.partner,elapsed);result.player=p.first;result.partner=q.first;result.playerDirection=p.second;result.partnerDirection=q.second;}
    else{result.player=route.player.back();result.partner=route.partner.back();}
    result.npc=result.scene=="P02P01A"?Point{316,132}:Point{364,244};result.npcVisible=result.scene=="T01P01A" || result.scene=="P02P01A";
    result.npcDirection=result.scene=="P02P01A"?7:4;
    result.bathing=(b==HolidayBeat::BathEnter || b==HolidayBeat::BathTalk || b==HolidayBeat::BathRest || b==HolidayBeat::BathExit) && result.player.y<=268;
    if(!result.moving){result.playerDirection=2;result.partnerDirection=6;}
    if(!result.moving && s.variety.dialogue.active && s.variety.dialogue.speaker!=kSystemDialogueActorId) {
        const auto point=[&](unsigned id){return id==0?result.player:id==kCompanionActorId?result.partner:result.npc;};
        const auto& d=s.variety.dialogue;const auto speaker=point(d.speaker),listener=point(d.listener);
        const auto look=[](Point from,Point to){const int dx=(to.x>from.x)-(to.x<from.x),dy=(to.y>from.y)-(to.y<from.y);return dx<0?(dy<0?5U:dy>0?7U:6U):dx>0?(dy<0?3U:dy>0?1U:2U):dy<0?4U:0U;};
        result.playerDirection=look(result.player,d.speaker==0?listener:speaker);
        result.partnerDirection=look(result.partner,d.speaker==kCompanionActorId?listener:speaker);
        result.npcDirection=look(result.npc,d.speaker==kHolidayActorId?listener:speaker);
    }
    return result;
}
unsigned holidayFade(const ExplorationState& s,std::uint64_t now) {
    const auto& h=s.variety.holiday;const auto elapsed=now>h.startedMs?now-h.startedMs:0;
    if(h.beat==HolidayBeat::PathFade || h.beat==HolidayBeat::WestFade || h.beat==HolidayBeat::ReturnFade)return unsigned(std::min<std::uint64_t>(255,elapsed*255/std::max<std::uint64_t>(1,h.deadlineMs-h.startedMs)));
    if(h.beat==HolidayBeat::WestWalk || h.beat==HolidayBeat::EastWalk || h.beat==HolidayBeat::SpringWalk || h.beat==HolidayBeat::Morning)return unsigned(255-std::min<std::uint64_t>(255,elapsed*255/500));
    return 0;
}
bool startHoliday(ExplorationState& s,const DialogueCatalogue& c,const HolidayRegistry& r,std::uint64_t now) {
    auto& v=s.variety;auto& g=s.gameplay;
    if(v.holiday.active || varietyBusy(v) || v.schedule.holidayCommitted || !g.companion.enabled ||
       (v.schedule.destination!=DayDestination::Town && v.schedule.destination!=DayDestination::HotSpring))return false;
    g.life=LifePhase::Explore;g.sceneBeat=SceneBeat::None;g.homeScene=false;s.phase=Phase::Explore;
    g.partyActions.clear();g.partyPlaybacks.clear();g.partyPhase=PartyPhase::Idle;g.enemyInstance=0;
    g.movement={};g.companion.movement={};g.walkStartedMs=g.companion.walkStartedMs=0;
    v.holiday={};v.holiday.active=true;v.holiday.npcMd=v.schedule.destination==DayDestination::Town?383:352;
    auto& e=v.event;e={};e.id={VarietyEvent::Holiday,s.runId,1,g.day,s.floorIndex};e.progress=EventProgress::Approach;e.startedMs=now;e.deadlineMs=now+120000;e.actors={kHolidayActorId};
    const auto choice=drawVariety(v,VarietyRandomStream::Dialogue,2);
    e.variant=v.schedule.destination==DayDestination::Town?(choice?"E10.town_a":"E10.town_b"):(choice?"E10.spring_a":"E10.spring_b");
    if(!speak(s,c,r,HolidayBeat::Morning,"morning",now)){cancelVariety(s);return false;}return true;
}
bool advanceHoliday(ExplorationState& s,const DialogueCatalogue& c,const HolidayRegistry& r,std::uint64_t now) {
    auto& v=s.variety;auto& h=v.holiday;
    if(!h.active)return false;
    if(v.dialogue.active){if(!selectedDialogue(s,c)){cancelVariety(s);return false;}if(!advanceDialoguePage(s,c,now))return false;}
    else if(now<h.deadlineMs)return false;
    const bool town=v.schedule.destination==DayDestination::Town;
    switch(h.beat) {
    case HolidayBeat::Morning:beat(s,HolidayBeat::PathWalk,r,now);break;
    case HolidayBeat::PathWalk:beat(s,HolidayBeat::PathFade,r,now,500);break;
    case HolidayBeat::PathFade:beat(s,town?HolidayBeat::WestWalk:HolidayBeat::SpringWalk,r,now);break;
    case HolidayBeat::WestWalk:(void)speak(s,c,r,HolidayBeat::WestVisit,"visit",now);break;
    case HolidayBeat::WestVisit:beat(s,HolidayBeat::WestExit,r,now);break;
    case HolidayBeat::WestExit:beat(s,HolidayBeat::WestFade,r,now,500);break;
    case HolidayBeat::WestFade:beat(s,HolidayBeat::EastWalk,r,now);break;
    case HolidayBeat::EastWalk:(void)speak(s,c,r,HolidayBeat::EastDepart,"depart",now);break;
    case HolidayBeat::SpringWalk:(void)speak(s,c,r,HolidayBeat::SpringVisit,"visit",now);break;
    case HolidayBeat::SpringVisit:beat(s,HolidayBeat::BathEnter,r,now);break;
    case HolidayBeat::BathEnter:(void)speak(s,c,r,HolidayBeat::BathTalk,"bath",now);break;
    case HolidayBeat::BathTalk:beat(s,HolidayBeat::BathRest,r,now,3000);break;
    case HolidayBeat::BathRest:beat(s,HolidayBeat::BathExit,r,now);break;
    case HolidayBeat::BathExit:(void)speak(s,c,r,HolidayBeat::SpringDepart,"depart",now);break;
    case HolidayBeat::EastDepart:case HolidayBeat::SpringDepart:beat(s,HolidayBeat::ReturnFade,r,now,750);break;
    case HolidayBeat::ReturnFade:
        v.event.progress=EventProgress::Result;(void)commitVarietyResult(s,"visited");v.schedule.holidayCommitted=true;
        v.lastHoliday=v.schedule.destination;v.lastHolidayAt=v.completedExplorations;return true;
    default:cancelVariety(s);return false;
    }
    if(h.active && !v.dialogue.active){v.event.progress=EventProgress::Action;s.gameplay.deadlineMs=h.deadlineMs;}
    return false;
}
void checkpointHoliday(ExplorationState& s,std::uint64_t now) {
    auto& h=s.variety.holiday;if(h.active)h.elapsedMs=unsigned(std::min<std::uint64_t>(s.variety.dialogue.active?120000:h.deadlineMs-h.startedMs,now>h.startedMs?now-h.startedMs:0));
}
void restoreHolidayClocks(ExplorationState& s,std::uint64_t now) {
    auto& h=s.variety.holiday;if(!h.active)return;
    const auto duration=h.deadlineMs-h.startedMs;h.startedMs=now>=h.elapsedMs?now-h.elapsedMs:0;h.deadlineMs=h.startedMs+duration;
    s.variety.event.startedMs=now;s.variety.event.deadlineMs=now+120000;s.gameplay.phaseStartedMs=h.startedMs;s.gameplay.deadlineMs=h.deadlineMs;
}
std::size_t holidayFrame(const Package& p,unsigned clip,unsigned direction,std::uint64_t elapsed) {
    const auto& frames=p.frames(clip,direction);std::uint64_t total=0;for(const auto& f:frames)total+=f.ticks;
    auto tick=(elapsed*p.tickHz/1000)%total;for(std::size_t i=0;i<frames.size();++i){if(tick<frames[i].ticks)return i;tick-=frames[i].ticks;}return frames.size()-1;
}
GameplayUpdate Explorer::returnFromHoliday(std::uint64_t now){
    state_.completed=false;
    if(state_.variety.schedule.holidayCommitted){state_.variety.event.progress=EventProgress::Finished;state_.variety.event.actors.clear();state_.variety.dialogue={};state_.variety.holiday={};}
    return beginReturn(now,false);
}
void Explorer::restoreHolidayClock(std::uint64_t now){restoreHolidayClocks(state_,now);}
} // namespace pet
