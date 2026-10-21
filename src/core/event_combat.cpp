#include "core/event_runtime.hpp"
#include <algorithm>
namespace pet {
namespace {
Entity* entity(ExplorationState& s,unsigned id){const auto i=std::find_if(s.population.entities.begin(),s.population.entities.end(),[&](const auto& a){return a.instanceId==id && a.kind==EntityKind::Friend;});return i==s.population.entities.end()?nullptr:&*i;}
unsigned nextId(const ExplorationState& s){for(unsigned id=1;id<=kTiles;++id)if(std::none_of(s.population.entities.begin(),s.population.entities.end(),[&](const auto& a){return a.instanceId==id;}))return id;return 0;}
bool available(const ExplorationState& s,int tile){return tile>=0 && tile<kTiles && s.world.walkable(tile) && !s.blocked[tile] && tile!=s.position && tile!=s.gameplay.companion.tile && tile!=s.world.stairs && std::none_of(s.population.entities.begin(),s.population.entities.end(),[&](const auto& a){return a.visible && a.tile==tile;});}
unsigned sampleEvent(ExplorationState& s,const Weights& weights){Rng rng{s.variety.eventRng};unsigned id=0;const bool ok=sample(weights,rng,id);s.variety.eventRng=rng.state;return ok?id:0;}
unsigned sampleItem(ExplorationState& s,const std::vector<ItemCategory>& categories) {
    Weights weights;for(const auto& c:categories)weights.push_back({c.originalId,c.weight});const auto id=sampleEvent(s,weights);
    const auto category=std::find_if(categories.begin(),categories.end(),[&](const auto& c){return c.originalId==id;});
    return category==categories.end()?0:sampleEvent(s,category->items);
}
GameplayUpdate speak(ExplorationState& s,const DialogueCatalogue& c,const std::string& stage,std::uint64_t now){if(!startDialogueBundle(s,c,s.variety.event.variant+"."+stage,now)){cancelVariety(s);s.gameplay.deadlineMs=now;}return {GameplayEvent::DialoguePlayer,{}};}
}
bool combatEvent(VarietyEvent type){return type==VarietyEvent::Shop || type==VarietyEvent::MonsterHouse || type==VarietyEvent::JointBattle;}
bool startCombatEvent(ExplorationState& s,VarietyEvent type,std::uint64_t now,const DialogueCatalogue& c,unsigned actorId,const EventRegistry* maps) {
    auto candidate=s;auto& g=candidate.gameplay;auto& e=candidate.variety.event;
    const auto* facts=maps?maps->floor(candidate):nullptr;
    if(!facts || (type==VarietyEvent::Shop && !facts->shopChance) || (type==VarietyEvent::MonsterHouse && !facts->monsterHouseChance))return false;
    const int room=s.world.room[s.position];if(room<0)return false;
    const auto distances=bfs(s.world,s.position,s.blocked).distance;
    const auto count=std::count_if(s.population.entities.begin(),s.population.entities.end(),[](const auto& a){return a.kind==EntityKind::Friend && a.visible && !a.done;});
    const unsigned needed=type==VarietyEvent::Shop?1:2;
    if(count+needed>s.spawnPolicy.friendBudget)return false;
    std::vector<int> tiles;
    for(int tile=0;tile<kTiles;++tile)if(s.world.room[tile]==room && distances[tile]>=1 && distances[tile]<=4 && available(s,tile))tiles.push_back(tile);
    if(tiles.size()<needed)return false;
    unsigned helper=0;
    if(type==VarietyEvent::JointBattle) {
        const auto i=std::find_if(s.population.entities.begin(),s.population.entities.end(),[&](const auto& a){return (!actorId || a.instanceId==actorId) && a.kind==EntityKind::Friend && a.visible && !a.done && canAttack(s.world,s.position,a.tile) && std::find(g.hostile.begin(),g.hostile.end(),a.instanceId)==g.hostile.end();});
        if(i==s.population.entities.end())return false;helper=i->instanceId;
    }
    e={};e.id={type,s.runId,std::uint64_t(s.floorSteps)+1,g.day,s.floorIndex};e.progress=EventProgress::Approach;e.startedMs=now;e.deadlineMs=now+120000;
    if(type==VarietyEvent::Shop) {
        const auto id=nextId(candidate);if(!id)return false;
        candidate.population.entities.push_back({id,383,EntityKind::Friend,tiles.front(),false});e.actors=e.temporaryActors={id};e.sourceTile=tiles.front();
        for(unsigned i=0;i<7;++i){const auto item=sampleItem(candidate,facts->shopItems);if(item)e.goods.push_back({item,1});}
        if(e.goods.empty())return false;
        const bool oran=std::any_of(e.goods.begin(),e.goods.end(),[](const auto& x){return x.item==ITEM_ORAN_BERRY;});
        if(oran && stockQuantity(candidate,0,109)>=3 && g.oran<kMaxStock) {
            e.variant="E03.trade_a";e.task=EventTask::Trade;e.item=ITEM_ORAN_BERRY;e.quantity=1;e.reservations={{0,109,3}};
        } else {e.variant="E03.browse";e.item=e.goods.front().item;e.quantity=1;}
    } else {
        e.variant=type==VarietyEvent::MonsterHouse?(drawVariety(candidate.variety,VarietyRandomStream::Event,2)?"E04.house_a":"E04.house_b"):
            (drawVariety(candidate.variety,VarietyRandomStream::Event,2)?"E07.assist_a":"E07.assist_b");
        if(helper)e.actors={helper};
        const auto& weights=type==VarietyEvent::MonsterHouse?facts->monsterHouse:candidate.floors[candidate.floorIndex].normalFriends;
        for(unsigned i=0;i<needed;++i) {
            const auto md=sampleEvent(candidate,weights),id=nextId(candidate);if(!md || !id)return false;
            candidate.population.entities.push_back({id,md,EntityKind::Friend,tiles[i],false});g.hostile.push_back(id);e.opponents.push_back(id);
        }
        e.task=EventTask::Battle;
    }
    if(!c.find(e.variant+".intro"))return false;
    g.walkStartedMs=g.companion.walkStartedMs=0;g.phaseStartedMs=now;g.deadlineMs=now+400;
    g.onceKeys.insert("event-floor:"+std::to_string(s.runId)+":"+std::to_string(s.floorIndex));g.onceKeys.insert("event-start:"+std::to_string(s.runId)+":"+std::to_string(s.floorIndex));
    ++candidate.occupancyVersion;s=std::move(candidate);return true;
}
bool commitShopExchange(ExplorationState& s) {
    const auto& e=s.variety.event;
    if(e.id.type!=VarietyEvent::Shop || e.task!=EventTask::Trade || e.progress!=EventProgress::Result || e.item!=ITEM_ORAN_BERRY || e.quantity!=1 ||
       e.reservations.size()!=1 || e.reservations[0].owner!=0 || e.reservations[0].item!=109 || e.reservations[0].quantity!=3 ||
       stockQuantity(s,0,109)<3 || s.gameplay.oran>=kMaxStock || s.variety.committed.count(eventKey(e.id)+":exchanged"))return false;
    auto candidate=s;auto& next=candidate.variety.event;
    const auto good=std::find_if(next.goods.begin(),next.goods.end(),[](const auto& x){return x.item==ITEM_ORAN_BERRY && x.quantity;});if(good==next.goods.end())return false;
    next.reservations.clear();if(!changeStock(candidate,0,109,-3) || !changeStock(candidate,0,ITEM_ORAN_BERRY,1))return false;
    if(--good->quantity==0)next.goods.erase(good);
    if(!commitVarietyResult(candidate,"exchanged"))return false;s=std::move(candidate);return true;
}
GameplayUpdate advanceCombatEvent(ExplorationState& s,std::uint64_t now,const DialogueCatalogue& c) {
    auto& e=s.variety.event;auto& g=s.gameplay;
    if(s.variety.dialogue.active){if(!selectedDialogue(s,c)){cancelVariety(s);g.deadlineMs=now;return {};}if(!advanceDialoguePage(s,c,now))return {};}
    else if(now<g.deadlineMs)return {};
    if(e.progress==EventProgress::Approach){e.progress=EventProgress::Dialogue;return speak(s,c,"intro",now);}
    if(e.progress==EventProgress::Dialogue) {
        if(e.id.type!=VarietyEvent::Shop) {
            if(g.actionRound==UINT64_MAX){cancelVariety(s);g.deadlineMs=now;return {};}
            e.progress=EventProgress::Action;g.independentParty=true;g.partyActions.clear();g.partyPlaybacks.clear();g.partyPhase=PartyPhase::Player;
            ++g.actionRound;g.companionIntent=CompanionIntent::Follow;g.companionTarget=0;g.deadlineMs=g.phaseStartedMs=now;return {};
        }
        e.progress=EventProgress::Result;
        if(e.task==EventTask::Trade){if(!commitShopExchange(s)){cancelVariety(s);g.deadlineMs=now;return {};}}
        else (void)commitVarietyResult(s,"browsed");
        return speak(s,c,"success",now);
    }
    if(e.progress==EventProgress::Result){e.progress=EventProgress::Dismiss;g.phaseStartedMs=now;g.deadlineMs=now+400;return {};}
    if(e.progress==EventProgress::Dismiss){finishVarietyEvent(s);g.deadlineMs=now;return {GameplayEvent::Explore,{}};}
    cancelVariety(s);g.deadlineMs=now;return {};
}
} // namespace pet
