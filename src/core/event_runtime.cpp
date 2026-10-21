#include "core/event_runtime.hpp"
#include <algorithm>
namespace pet {
namespace {
Entity* entity(ExplorationState& s,unsigned id) {
    const auto i=std::find_if(s.population.entities.begin(),s.population.entities.end(),[&](const auto& e){return e.instanceId==id && e.kind==EntityKind::Friend;});
    return i==s.population.entities.end()?nullptr:&*i;
}
Mask occupied(const ExplorationState& s,unsigned except=0) {
    auto result=s.blocked;result[s.position]=1;if(s.gameplay.companion.enabled)result[s.gameplay.companion.tile]=1;
    for(const auto& e:s.population.entities)if(e.kind==EntityKind::Friend && e.visible && e.instanceId!=except)result[e.tile]=1;
    return result;
}
unsigned freeId(const ExplorationState& s) {
    for(unsigned id=1;id<=kTiles;++id)if(std::none_of(s.population.entities.begin(),s.population.entities.end(),[&](const auto& e){return e.instanceId==id;}))return id;
    return 0;
}
std::string floorKey(const ExplorationState& s){return "event-floor:"+std::to_string(s.runId)+":"+std::to_string(s.floorIndex);}
std::string resultStage(VarietyEvent type){return type==VarietyEvent::TiredExplorer?"success":"depart";}
void finish(ExplorationState& s) {
    auto& e=s.variety.event;
    if(e.id.type==VarietyEvent::GuildVisit)for(auto id:e.actors)if(auto* a=entity(s,id)){a->done=true;a->visible=false;}
    e.progress=EventProgress::Finished;e.actors.clear();e.reservations.clear();s.variety.dialogue={};
    s.target=-1;s.targetInstance=0;s.goalKind=GoalKind::None;s.gameplay.deadlineMs=0;++s.occupancyVersion;
}
bool moveVisitor(ExplorationState& s,bool departing,std::uint64_t now) {
    auto* a=entity(s,s.variety.event.actors.front());if(!a)return false;
    const auto distance=bfs(s.world,s.position,s.blocked).distance;
    const auto blocked=occupied(s,a->instanceId);int best=-1;
    for(const auto& [dx,dy]:kDirections) {
        const int next=a->tile+dy*kWidth+dx;
        if(next<0 || next>=kTiles || next==s.world.stairs || !canMove(s.world,a->tile,next,blocked) || distance[next]<0)continue;
        if((departing?distance[next]>distance[a->tile]:distance[next]<distance[a->tile]) && (best<0 || (departing?distance[next]>distance[best]:distance[next]<distance[best])))best=next;
    }
    if(best<0)return false;
    const int from=a->tile;a->tile=best;a->direction=directionBetween(from,best);
    a->movement={s.variety.event.id.trigger,now,400,a->direction,from,best};
    ++s.occupancyVersion;s.gameplay.phaseStartedMs=now;s.gameplay.deadlineMs=now+400;return true;
}
}
bool eventActive(const ExplorationState& s){return s.variety.event.progress>=EventProgress::Approach && s.variety.event.progress<=EventProgress::Dismiss;}
bool startVarietyEvent(ExplorationState& s,VarietyEvent type,std::uint64_t now,const DialogueCatalogue& c,std::uint32_t actorId) {
    if(!canStartVariety(s,now) || !s.gameplay.companion.enabled || s.position==s.world.stairs)return false;
    if(type!=VarietyEvent::Wonder && type!=VarietyEvent::GuildVisit && type!=VarietyEvent::TiredExplorer)return false;
    const auto distance=bfs(s.world,s.position,s.blocked).distance;std::string variant;unsigned selectedActor=0,shareOwner=0;bool share=false;
    if(type==VarietyEvent::Wonder) {
        if(s.floors[s.floorIndex].dungeonId!=15 || !c.find("E05.view_a.view"))return false;
        variant=drawVariety(s.variety,VarietyRandomStream::Event,2)?"E05.view_a":"E05.view_b";
    } else if(type==VarietyEvent::TiredExplorer) {
        const auto i=std::find_if(s.population.entities.begin(),s.population.entities.end(),[&](const auto& a){return
            (!actorId || a.instanceId==actorId) && a.kind==EntityKind::Friend && a.visible && !a.done && canAttack(s.world,s.position,a.tile) &&
            std::find(s.gameplay.hostile.begin(),s.gameplay.hostile.end(),a.instanceId)==s.gameplay.hostile.end();});
        if(i==s.population.entities.end() || !c.find("E01.encourage.intro"))return false;
        selectedActor=i->instanceId;variant="E01.encourage";
        if(s.gameplay.oran>=2 && c.find("E01.share_a.intro")){variant="E01.share_a";share=true;}
        else if(s.gameplay.companion.oran>=2 && c.find("E01.share_b.intro")){variant="E01.share_b";shareOwner=kCompanionActorId;share=true;}
    } else {
        const auto count=std::count_if(s.population.entities.begin(),s.population.entities.end(),[](const auto& a){return a.kind==EntityKind::Friend && a.visible && !a.done;});
        const auto mask=occupied(s);int spawn=-1;
        for(int tile=0;tile<kTiles;++tile)if(distance[tile]>=2 && distance[tile]<=3 && !mask[tile] && tile!=s.world.stairs){spawn=tile;break;}
        selectedActor=freeId(s);
        if(count>=s.spawnPolicy.friendBudget || spawn<0 || !selectedActor || !c.find("E09.bidoof.intro") || !c.find("E09.corphish.intro"))return false;
        const bool bidoof=drawVariety(s.variety,VarietyRandomStream::Event,2)==0;variant=bidoof?"E09.bidoof":"E09.corphish";
        s.population.entities.push_back({selectedActor,bidoof?434U:369U,EntityKind::Friend,spawn,false});++s.occupancyVersion;
    }
    auto& e=s.variety.event;e={};e.id={type,s.runId,std::uint64_t(s.floorSteps)+1,s.gameplay.day,s.floorIndex};
    e.progress=EventProgress::Approach;e.variant=variant;e.startedMs=now;e.deadlineMs=now+120000;
    if(selectedActor)e.actors={selectedActor};
    if(share)e.reservations={{shareOwner,ITEM_ORAN_BERRY,1}};
    auto& g=s.gameplay;g.walkStartedMs=g.companion.walkStartedMs=0;g.phaseStartedMs=now;g.deadlineMs=now+400;
    g.onceKeys.insert(floorKey(s));g.onceKeys.insert("event-start:"+std::to_string(s.runId)+":"+std::to_string(s.floorIndex));
    return true;
}
bool tryAutomaticEvent(ExplorationState& s,std::uint64_t now,const DialogueCatalogue& c) {
    if(s.floorSteps<16 || s.floorSteps%32!=16 || !canStartVariety(s,now) || !s.gameplay.companion.enabled || s.gameplay.onceKeys.count(floorKey(s)))return false;
    const auto trigger="event-evaluated:"+std::to_string(s.runId)+":"+std::to_string(s.floorIndex)+":"+std::to_string(s.floorSteps);
    if(!s.gameplay.onceKeys.insert(trigger).second)return false;
    const auto prefix="event-start:"+std::to_string(s.runId)+":";
    if(std::count_if(s.gameplay.onceKeys.begin(),s.gameplay.onceKeys.end(),[&](const auto& k){return k.rfind(prefix,0)==0;})>=2)return false;
    // Conservative first policy: one opportunity per 32 completed moves, at most one per floor/two per run.
    // The original spawn/item weights and encounter/combat RNG are not modified.
    const auto draw=drawVariety(s.variety,VarietyRandomStream::Event,4);
    if(draw==3)return false;
    const auto type=draw==0?VarietyEvent::Wonder:draw==1?VarietyEvent::GuildVisit:VarietyEvent::TiredExplorer;
    return startVarietyEvent(s,type,now,c);
}
GameplayUpdate advanceVarietyEvent(ExplorationState& s,std::uint64_t now,const DialogueCatalogue& c) {
    auto& e=s.variety.event;auto& g=s.gameplay;
    const bool stale=e.id.run!=s.runId || e.id.day!=g.day || e.id.floor!=s.floorIndex || s.phase!=Phase::Explore || g.life!=LifePhase::Explore || g.sceneBeat!=SceneBeat::None;
    const bool actorsMissing=std::any_of(e.actors.begin(),e.actors.end(),[&](auto id){const auto* a=entity(s,id);return !a || !a->visible || a->done;});
    if(stale || now>=e.deadlineMs || g.hearts<=kLowHp || !g.companion.enabled || g.companion.hearts<=kLowHp || actorsMissing) {
        cancelVariety(s);g.deadlineMs=now;return {GameplayEvent::Explore,{}};
    }
    if(s.variety.dialogue.active) {
        if(!selectedDialogue(s,c)){cancelVariety(s);g.deadlineMs=now;return {};}
        if(!advanceDialoguePage(s,c,now))return {};
    } else if(now<g.deadlineMs)return {};
    if(e.progress==EventProgress::Approach) {
        if(e.id.type==VarietyEvent::GuildVisit) {
            auto* a=entity(s,e.actors.front());
            if(!canAttack(s.world,s.position,a->tile)) {
                if(!moveVisitor(s,false,now)){cancelVariety(s);g.deadlineMs=now;}
                return {GameplayEvent::ActorMove,{}};
            }
        }
        e.progress=EventProgress::Dialogue;
        if(!startDialogueBundle(s,c,e.variant+(e.id.type==VarietyEvent::Wonder?".view":".intro"),now)){cancelVariety(s);g.deadlineMs=now;}
        return {GameplayEvent::DialoguePlayer,{}};
    }
    if(e.progress==EventProgress::Dialogue) {
        e.progress=EventProgress::Result;
        if(e.id.type==VarietyEvent::TiredExplorer && e.variant!="E01.encourage") {
            if(!commitOranShare(s)){cancelVariety(s);g.deadlineMs=now;return {};}
        } else (void)commitVarietyResult(s,e.id.type==VarietyEvent::TiredExplorer?"encouraged":e.id.type==VarietyEvent::Wonder?"observed":"visited");
        if(!startDialogueBundle(s,c,e.variant+"."+resultStage(e.id.type),now)){cancelVariety(s);g.deadlineMs=now;}
        return {GameplayEvent::DialogueNpc,{}};
    }
    if(e.progress==EventProgress::Result){e.progress=EventProgress::Dismiss;g.phaseStartedMs=now;g.deadlineMs=now+400;return {};}
    if(e.progress==EventProgress::Dismiss) {
        if(e.id.type==VarietyEvent::GuildVisit) {
            auto* a=entity(s,e.actors.front());const auto distance=bfs(s.world,s.position,s.blocked).distance;
            if(distance[a->tile]<3 && moveVisitor(s,true,now))return {GameplayEvent::ActorMove,{}};
        }
        finish(s);g.deadlineMs=now;return {GameplayEvent::Explore,{}};
    }
    cancelVariety(s);g.deadlineMs=now;return {};
}
bool commitOranShare(ExplorationState& s) {
    const auto& e=s.variety.event;
    if(e.id.type!=VarietyEvent::TiredExplorer || e.progress!=EventProgress::Result ||
       s.variety.committed.count(eventKey(e.id)+":shared") || e.reservations.size()!=1 || e.actors.empty())return false;
    const auto reserved=e.reservations.front();const auto* npc=entity(s,e.actors.front());
    if(reserved.item!=ITEM_ORAN_BERRY || reserved.quantity!=1 || stockQuantity(s,reserved.owner,reserved.item)<2 || !npc ||
       !npc->visible || npc->done || !canAttack(s.world,s.position,npc->tile) || s.gameplay.hearts<=kLowHp || s.gameplay.companion.hearts<=kLowHp)return false;
    auto candidate=s;candidate.variety.event.reservations.clear();
    if(!changeStock(candidate,reserved.owner,reserved.item,-1) || !commitVarietyResult(candidate,"shared"))return false;
    s=std::move(candidate);return true;
}
} // namespace pet
