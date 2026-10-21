#include "core/event_runtime.hpp"
#include "core/event_movement.hpp"
#include <algorithm>
namespace pet {
namespace {
Entity* entity(ExplorationState& s,unsigned id){const auto i=std::find_if(s.population.entities.begin(),s.population.entities.end(),[&](const auto& a){return a.instanceId==id && a.kind==EntityKind::Friend;});return i==s.population.entities.end()?nullptr:&*i;}
bool free(const ExplorationState& s,int tile) {
    return tile>=0 && tile<kTiles && s.world.walkable(tile) && !s.blocked[tile] && tile!=s.position && tile!=s.gameplay.companion.tile && tile!=s.world.stairs &&
        std::none_of(s.population.entities.begin(),s.population.entities.end(),[&](const auto& a){return a.visible && !a.done && a.tile==tile;});
}
bool safeTile(const ExplorationState& s,int tile) {
    return free(s,tile) && std::none_of(s.population.entities.begin(),s.population.entities.end(),[&](const auto& a){return a.visible && !a.done && std::find(s.gameplay.hostile.begin(),s.gameplay.hostile.end(),a.instanceId)!=s.gameplay.hostile.end() && canAttack(s.world,tile,a.tile);});
}
void abort(ExplorationState& s,std::uint64_t now){cancelVariety(s);s.gameplay.deadlineMs=now;}
GameplayUpdate speak(ExplorationState& s,const DialogueCatalogue& c,const std::string& stage,std::uint64_t now) {
    if(!startDialogueBundle(s,c,s.variety.event.variant+"."+stage,now))abort(s,now);
    return {GameplayEvent::DialoguePlayer,{}};
}
GameplayUpdate route(ExplorationState& s,unsigned leader,int target,unsigned stop,std::uint64_t now,bool& arrived) {
    const auto status=advanceEventRoute(s,leader,target,stop,now);arrived=status==EventRoute::Arrived;
    if(status==EventRoute::Blocked)abort(s,now);
    return {status==EventRoute::Moved?GameplayEvent::Move:GameplayEvent::None,{}};
}
}
bool movementEvent(VarietyEvent type){return type==VarietyEvent::LostPokemon || type==VarietyEvent::LostProperty || type==VarietyEvent::Reunion;}
bool startMovementEvent(ExplorationState& s,VarietyEvent type,std::uint64_t now,const DialogueCatalogue& c,unsigned actorId,const EventRegistry* maps) {
    auto& g=s.gameplay;const auto distances=bfs(s.world,s.position,s.blocked).distance;
    Entity* owner=nullptr;std::string variant;int target=-1,object=-1,source=-1;unsigned victim=0;
    if(type==VarietyEvent::LostPokemon || type==VarietyEvent::LostProperty) {
        const auto i=std::find_if(s.population.entities.begin(),s.population.entities.end(),[&](const auto& a){return
            (!actorId || a.instanceId==actorId) && a.kind==EntityKind::Friend && a.visible && !a.done && distances[a.tile]>0 && distances[a.tile]<=12 &&
            (type==VarietyEvent::LostProperty || canAttack(s.world,s.position,a.tile)) &&
            std::find(g.hostile.begin(),g.hostile.end(),a.instanceId)==g.hostile.end();});
        if(i==s.population.entities.end())return false;owner=&*i;
        if(type==VarietyEvent::LostPokemon) {
            const auto distance=bfs(s.world,owner->tile,s.blocked).distance;
            for(int tile=0;tile<kTiles;++tile)if(canAttack(s.world,tile,s.world.stairs) && safeTile(s,tile) && distance[tile]>=0 && (target<0 || distance[tile]<distance[target]))target=tile;
            if(target<0)return false;
            variant=drawVariety(s.variety,VarietyRandomStream::Event,2)?"E02.escort_a":"E02.escort_b";
        } else {
            for(int tile=0;tile<kTiles;++tile)if(distances[tile]>=1 && distances[tile]<=2 && safeTile(s,tile)){object=tile;break;}
            if(object<0)return false;
            variant=drawVariety(s.variety,VarietyRandomStream::Event,2)?"E06.return_a":"E06.return_b";
        }
    } else {
        const auto* f=maps?maps->floor(s):nullptr;
        if(!f || !f->trapDensity || std::none_of(f->traps.begin(),f->traps.end(),[](const auto& w){return w.originalId==6 && w.value;}))return false;
        victim=drawVariety(s.variety,VarietyRandomStream::Event,2)?kCompanionActorId:0;
        source=victim==0?s.position:g.companion.tile;target=victim==0?g.companion.tile:s.position;
        const auto contact="warp-contact:"+std::to_string(s.runId)+":"+std::to_string(s.floorIndex)+":"+std::to_string(source);
        if(g.onceKeys.count(contact))return false;
        const auto distance=bfs(s.world,target,s.blocked).distance;std::vector<int> destinations;
        for(int tile=0;tile<kTiles;++tile)if(distance[tile]>=4 && distance[tile]<=8 && safeTile(s,tile))destinations.push_back(tile);
        if(destinations.empty())return false;
        const auto destination=destinations[drawVariety(s.variety,VarietyRandomStream::Event,destinations.size())];
        variant=drawVariety(s.variety,VarietyRandomStream::Event,2)?"E08.reunion_a":"E08.reunion_b";
        if(!c.find(variant+".separated"))return false;
        g.onceKeys.insert(contact);g.independentParty=true;
        if(victim==0){s.position=destination;s.visited[destination]=1;s.discovered[destination]=1;g.movement={};}
        else{g.companion.tile=destination;g.companion.movement={};}
        ++s.occupancyVersion;
    }
    const std::string first=type==VarietyEvent::LostPokemon?"intro":type==VarietyEvent::LostProperty?"pickup":"separated";
    if(!c.find(variant+"."+first))return false;
    auto& e=s.variety.event;e={};e.id={type,s.runId,std::uint64_t(s.floorSteps)+1,g.day,s.floorIndex};e.variant=variant;
    e.progress=type==VarietyEvent::Reunion?EventProgress::Dialogue:EventProgress::Approach;e.startedMs=now;e.deadlineMs=now+120000;
    if(owner)e.actors={owner->instanceId};
    e.task=type==VarietyEvent::LostPokemon?EventTask::Escort:type==VarietyEvent::LostProperty?EventTask::Collect:EventTask::Reunite;
    e.targetTile=target;e.sourceTile=source;e.warpedActor=victim;
    if(type==VarietyEvent::LostProperty){e.objectTile=object;e.item=109;e.quantity=1;}
    g.walkStartedMs=g.companion.walkStartedMs=0;g.phaseStartedMs=now;g.deadlineMs=now+400;
    g.onceKeys.insert("event-floor:"+std::to_string(s.runId)+":"+std::to_string(s.floorIndex));
    g.onceKeys.insert("event-start:"+std::to_string(s.runId)+":"+std::to_string(s.floorIndex));
    if(type==VarietyEvent::Reunion)(void)startDialogueBundle(s,c,variant+".separated",now);
    return true;
}
GameplayUpdate advanceMovementEvent(ExplorationState& s,std::uint64_t now,const DialogueCatalogue& c) {
    auto& e=s.variety.event;auto& g=s.gameplay;bool talkFinished=false;
    if(s.variety.dialogue.active) {
        if(!selectedDialogue(s,c)){abort(s,now);return {};}
        if(!advanceDialoguePage(s,c,now))return {};
        talkFinished=true;
    } else if(now<g.deadlineMs)return {};
    if(e.progress==EventProgress::Approach) {
        if(e.id.type==VarietyEvent::LostProperty) {
            bool arrived=false;const auto update=route(s,0,e.objectTile,0,now,arrived);if(!arrived)return update;
            e.carrying=true;e.objectTile=-1;
        }
        e.progress=EventProgress::Dialogue;return speak(s,c,e.id.type==VarietyEvent::LostPokemon?"intro":"pickup",now);
    }
    if(e.progress==EventProgress::Dialogue) {
        e.progress=EventProgress::Action;
        if(e.id.type==VarietyEvent::LostProperty)e.task=EventTask::FindOwner;
        g.phaseStartedMs=g.deadlineMs=now;return {};
    }
    if(e.progress==EventProgress::Action) {
        bool arrived=false;GameplayUpdate update;
        if(e.id.type==VarietyEvent::LostPokemon)update=route(s,e.actors.front(),e.targetTile,0,now,arrived);
        else if(e.id.type==VarietyEvent::LostProperty) {
            auto* owner=entity(s,e.actors.front());if(!owner || !e.carrying){abort(s,now);return {};}
            if(talkFinished)arrived=canAttack(s.world,s.position,owner->tile);
            else {update=route(s,0,owner->tile,1,now,arrived);if(arrived)return speak(s,c,"owner",now);}
        } else update=route(s,e.warpedActor,e.targetTile,1,now,arrived);
        if(!arrived)return update;
        e.progress=EventProgress::Result;
        if(e.id.type==VarietyEvent::LostProperty){e.carrying=false;e.item=e.quantity=0;}
        (void)commitVarietyResult(s,e.id.type==VarietyEvent::LostPokemon?"escorted":e.id.type==VarietyEvent::LostProperty?"returned":"reunited");
        return speak(s,c,e.id.type==VarietyEvent::Reunion?"reunion":"success",now);
    }
    if(e.progress==EventProgress::Result){e.progress=EventProgress::Dismiss;g.deadlineMs=now+400;g.phaseStartedMs=now;return {};}
    if(e.progress==EventProgress::Dismiss){finishVarietyEvent(s);g.deadlineMs=now;return {GameplayEvent::Explore,{}};}
    abort(s,now);return {};
}
} // namespace pet
