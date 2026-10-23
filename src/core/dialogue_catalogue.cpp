#include "core/dialogue_catalogue.hpp"
#include "core/exploration_io.hpp"
#include "core/event_runtime.hpp"
#include "core/holiday_scene.hpp"
#include "core/gameplay_notice.hpp"
#include <algorithm>
#include <set>
#include <stdexcept>
namespace pet {
namespace {
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
bool identity(const std::string& s) {
    return !s.empty() && s.size()<=128 && std::all_of(s.begin(),s.end(),[](unsigned char c){return
        (c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='.' || c=='_' || c=='-';});
}
const std::vector<std::string> contexts{"companion.checkin","npc.greet","sleep.greet","wake.alarm","morning.depart","defeat.player","defeat.partner","boss.prepare"};
std::wstring decodeText(const std::string& text) {
    std::wstring result;
    for(std::size_t i=0;i<text.size();) {
        const auto c=static_cast<unsigned char>(text[i++]);unsigned value=c,extra=0;
        if(c>=0xc2 && c<=0xdf){value=c&31;extra=1;}
        else if(c>=0xe0 && c<=0xef){value=c&15;extra=2;}
        else require(c<128,"dialogue text is not supported BMP UTF-8");
        require(i+extra<=text.size(),"truncated dialogue UTF-8");
        for(unsigned j=0;j<extra;++j){const auto t=static_cast<unsigned char>(text[i++]);require((t&0xc0)==0x80,"invalid UTF-8 continuation");value=(value<<6)|(t&63);}
        require((!extra || value>=(extra==1?128U:2048U)) && !(value>=0xd800 && value<=0xdfff) &&
            (value>=32 || value=='\n'),"invalid dialogue code point");
        result.push_back(static_cast<wchar_t>(value));
    }
    unsigned rows=1,width=0;
    for(auto c:result){if(c=='\n'){++rows;width=0;}else width+=c<128?1:2;require(width<=26 && rows<=2,"dialogue page exceeds text box");}
    require(!result.empty(),"empty dialogue page");return result;
}
std::uint32_t actor(const ExplorationState& s,PortraitRole role) {
    return role==PortraitRole::Player?0:role==PortraitRole::Partner?kCompanionActorId:
        role==PortraitRole::Friendly?(eventActive(s) && !s.variety.event.actors.empty()?s.variety.event.actors.front():s.gameplay.enemyInstance):kSystemDialogueActorId;
}
bool actorExists(const ExplorationState& s,PortraitRole role) {
    if(role==PortraitRole::Player)return true;
    if(role==PortraitRole::Partner)return s.gameplay.companion.enabled;
    if(role==PortraitRole::Alarm)return s.gameplay.homeScene;
    if(role==PortraitRole::None)return true;
    if(role!=PortraitRole::Friendly)return false;
    if(s.variety.holiday.active)return true;
    return std::any_of(s.population.entities.begin(),s.population.entities.end(),[&](const auto& e){return
        e.instanceId==actor(s,role) && e.kind==EntityKind::Friend && e.visible && (!e.done || bossConversation(s)) &&
        std::find(s.gameplay.hostile.begin(),s.gameplay.hostile.end(),e.instanceId)==s.gameplay.hostile.end();});
}
bool safePair(const ExplorationState& s,bool healthy=true) {
    const auto& g=s.gameplay;
    if(!g.hearts || (healthy && g.hearts<=kLowHp) || (g.companion.enabled && (!g.companion.hearts || (healthy && g.companion.hearts<=kLowHp))) ||
       std::any_of(g.partyActions.begin(),g.partyActions.end(),[](const auto& a){return !a.finished && a.kind!=PartyActionKind::Wait;}) || !g.partyPlaybacks.empty())return false;
    return std::none_of(s.population.entities.begin(),s.population.entities.end(),[&](const auto& e){return
        e.visible && !e.done && std::find(g.hostile.begin(),g.hostile.end(),e.instanceId)!=g.hostile.end() &&
        (canAttack(s.world,s.position,e.tile) || (g.companion.enabled && canAttack(s.world,g.companion.tile,e.tile)));});
}
void assignLine(ExplorationState& s,const DialogueBundle& b,unsigned index,std::uint64_t now,std::uint64_t minimum=0) {
    const auto& line=b.lines.at(index);auto& d=s.variety.dialogue;auto& g=s.gameplay;
    d.active=true;d.context=b.context;d.bundle=b.id;d.line=index;d.lineId=line.id;
    d.speaker=actor(s,line.display.speaker);d.listener=actor(s,line.listener);
    d.startedMs=now;d.deadlineMs=now+std::max<std::uint64_t>(minimum,(line.display.text.size()-1)*25+line.holdMs);
    d.revealed=1;d.readRemainingMs=line.holdMs;
    g.phaseStartedMs=now;g.deadlineMs=d.deadlineMs;
    if(g.life==LifePhase::Dialogue)g.dialogueStage=line.display.speaker==PortraitRole::Player?DialogueStage::Player:DialogueStage::Npc;
    if(eventActive(s)) {
        g.walkStartedMs=g.companion.walkStartedMs=0;
        if(s.variety.holiday.active)return;
        auto tile=[&](unsigned id){if(!id)return s.position;if(id==kCompanionActorId)return g.companion.tile;
            const auto it=std::find_if(s.population.entities.begin(),s.population.entities.end(),[&](const auto& e){return e.instanceId==id;});return it==s.population.entities.end()?-1:it->tile;};
        const int speaker=tile(d.speaker),listener=tile(d.listener);
        if(speaker>=0 && listener>=0) {
            g.playerDirection=directionBetween(s.position,d.speaker==0?listener:speaker);
            if(g.companion.enabled)g.companion.direction=directionBetween(g.companion.tile,d.speaker==kCompanionActorId?listener:speaker);
            for(auto& e:s.population.entities)if(std::find(s.variety.event.actors.begin(),s.variety.event.actors.end(),e.instanceId)!=s.variety.event.actors.end())e.direction=directionBetween(e.tile,e.instanceId==d.speaker?listener:speaker);
        }
    }
}
bool begin(ExplorationState& s,const DialogueCatalogue& c,std::uint64_t now) {
    const auto context=dialogueContext(s);
    if(context.empty() || !dialogueEligible(s,context))return false;
    const auto recent=std::find_if(s.variety.recent.begin(),s.variety.recent.end(),[&](const auto& r){return r.context==context;});
    std::vector<const DialogueBundle*> candidates;
    for(const auto& b:c.bundles)if(b.context==context && std::all_of(b.lines.begin(),b.lines.end(),[&](const auto& l){return actorExists(s,l.display.speaker) && actorExists(s,l.listener);}))candidates.push_back(&b);
    if(candidates.size()>1 && recent!=s.variety.recent.end())candidates.erase(std::remove_if(candidates.begin(),candidates.end(),[&](const auto* b){return b->id==recent->bundle;}),candidates.end());
    unsigned total=0;for(auto* b:candidates)total+=b->weight;
    if(!total)return false;
    auto draw=drawVariety(s.variety,VarietyRandomStream::Dialogue,total);const DialogueBundle* selected=nullptr;
    for(auto* b:candidates){if(draw<b->weight){selected=b;break;}draw-=b->weight;}
    require(selected!=nullptr,"dialogue selection failed");
    if(recent!=s.variety.recent.end())recent->bundle=selected->id;
    else {if(s.variety.recent.size()==64)s.variety.recent.erase(s.variety.recent.begin());s.variety.recent.push_back({context,selected->id,0});}
    const auto minimum=s.gameplay.deadlineMs>now?std::min<std::uint64_t>(5000,s.gameplay.deadlineMs-now):0;
    assignLine(s,*selected,0,now,minimum);s.variety.dialogue.catalogueVersion=c.version;return true;
}
bool animationDone(const ExplorationState& s,ActorCompletion done) {
    const auto& g=s.gameplay;
    if(g.life==LifePhase::Dialogue)return s.variety.dialogue.speaker==0?done.player:
        s.variety.dialogue.speaker==kCompanionActorId?done.partner:done.enemy;
    if(g.life==LifePhase::Sleep || g.life==LifePhase::Ready)return done.player && (!g.companion.enabled || done.partner);
    return true;
}
}
const DialogueBundle* DialogueCatalogue::find(const std::string& id) const {
    const auto i=std::find_if(bundles.begin(),bundles.end(),[&](const auto& b){return b.id==id;});return i==bundles.end()?nullptr:&*i;
}
DialogueCatalogue loadDialogueCatalogue(const std::filesystem::path& path) {
    const auto bytes=readFile(path,256*1024);detail::Reader r(bytes);DialogueCatalogue c;
    require(r.u32()==0x434c444d && r.u32()==1,"unsupported dialogue catalogue");c.version=r.u32();require(c.version>0,"invalid dialogue version");
    const auto count=r.count(128);require(count>0,"empty dialogue catalogue");std::set<std::string> ids,lines;
    for(unsigned i=0;i<count;++i) {
        DialogueBundle b;b.id=r.str();b.context=r.str();b.weight=r.count(100);
        const std::set<std::string> eventContexts{"event.E01.intro","event.E01.success","event.E05.view","event.E05.depart","event.E09.intro","event.E09.depart","event.E02.intro","event.E02.success","event.E06.pickup","event.E06.owner","event.E06.success","event.E08.separated","event.E08.reunion","event.E03.intro","event.E03.success","event.E04.intro","event.E04.victory","event.E07.intro","event.E07.victory","event.E10.morning","event.E10.visit","event.E10.bath","event.E10.depart","boss.befriend"};
        const bool event=eventContexts.count(b.context)!=0;
        require(identity(b.id) && ids.insert(b.id).second && (event || std::find(contexts.begin(),contexts.end(),b.context)!=contexts.end()) && b.weight,"invalid dialogue bundle");
        const auto n=r.count(32);require(n>0,"empty dialogue bundle");
        for(unsigned j=0;j<n;++j){CatalogueLine l;l.id=r.str();l.display.id=r.u32();l.display.speaker=PortraitRole(r.count(4));l.listener=PortraitRole(r.count(4));l.display.emotion=PortraitEmotion(r.count(13));l.holdMs=r.count(5000);l.display.text=decodeText(r.str());
            require(identity(l.id) && lines.insert(l.id).second && l.holdMs>=700,"invalid catalogue line");
            const auto p=l.display.speaker,q=l.listener;const auto id=l.display.id;
            const bool valid=b.context=="boss.prepare"?(id==1201 && p==PortraitRole::Partner && q==PortraitRole::Player):
                b.context=="companion.checkin"?(id==12 && p==PortraitRole::Partner && q==PortraitRole::Player):
                b.context=="npc.greet"?(id==22 && p==PortraitRole::Friendly && q==PortraitRole::Player):
                b.context=="wake.alarm"?(id==41 && p==PortraitRole::Alarm && q==PortraitRole::Player):
                b.context=="sleep.greet"?(id==31 && p==PortraitRole::Partner && q==PortraitRole::Player):
                b.context=="morning.depart"?(id==51 && p==PortraitRole::Partner && q==PortraitRole::Player):
                (id==(b.context=="defeat.player"?101U:102U) && p==PortraitRole::Partner && q==PortraitRole::Player);
            require(event?(p<PortraitRole::Alarm && q>PortraitRole::None && q<PortraitRole::Alarm && p!=q && l.display.id>=1000):valid,"context speaker identity mismatch");b.lines.push_back(std::move(l));}
        require(event || b.lines.front().display.speaker==(b.context=="npc.greet"?PortraitRole::Friendly:b.context=="wake.alarm"?PortraitRole::Alarm:PortraitRole::Partner),"invalid initial speaker");
        if(b.context=="companion.checkin" || b.context=="npc.greet") {
            require(b.lines.back().display.speaker!=PortraitRole::Player,"dialogue response missing");bool replied=false;
            for(const auto& l:b.lines){if(l.display.speaker!=PortraitRole::Player)replied=true;else require(!replied,"question after response");}
        }
        c.bundles.push_back(std::move(b));
    }
    r.end();return c;
}
std::string dialogueContext(const ExplorationState& s) {
    if(eventActive(s) && s.variety.dialogue.active)return s.variety.dialogue.context;
    const auto& g=s.gameplay;
    if(bossPreparation(s))return "boss.prepare";
    if(bossConversation(s))return "boss.befriend";
    if(g.life==LifePhase::Dialogue)return companionDialogue(g)?"companion.checkin":"npc.greet";
    if(g.life==LifePhase::Sleep && g.sceneBeat==SceneBeat::BedFace)return "sleep.greet";
    if(g.life==LifePhase::Wake && g.sceneBeat==SceneBeat::WakeDelay)return "wake.alarm";
    if(g.life==LifePhase::Ready && g.sceneBeat==SceneBeat::MorningFace)return "morning.depart";
    if(g.life==LifePhase::Death && g.deathStage==DeathStage::PartnerTalk)return g.companion.hearts?"defeat.player":"defeat.partner";
    return {};
}
bool dialogueEligible(const ExplorationState& s,const std::string& context) {
    if(context!=dialogueContext(s))return false;
    if(s.variety.holiday.active)return s.gameplay.companion.enabled;
    if(eventActive(s))return safePair(s) && s.gameplay.companion.enabled;
    const auto& g=s.gameplay;
    if(context=="boss.prepare")return bossPreparation(s) && g.companion.enabled;
    if(context=="boss.befriend")return bossConversation(s) && !fixedBattlePending(s);
    if(context=="companion.checkin")return safePair(s) && g.companion.enabled && canAttack(s.world,s.position,g.companion.tile);
    if(context=="npc.greet")return safePair(s,false) && actorExists(s,PortraitRole::Friendly) && std::any_of(s.population.entities.begin(),s.population.entities.end(),[&](const auto& e){return e.instanceId==g.enemyInstance && canAttack(s.world,s.position,e.tile);});
    if(context=="wake.alarm")return g.homeScene;
    if(context=="sleep.greet" || context=="morning.depart")return g.homeScene && g.companion.enabled;
    if(context=="defeat.player")return g.companion.enabled && g.companion.hearts && !g.hearts;
    if(context=="defeat.partner")return g.companion.enabled && !g.companion.hearts && g.hearts;
    return false;
}
const CatalogueLine* selectedDialogue(const ExplorationState& s,const DialogueCatalogue& c) {
    const auto& d=s.variety.dialogue;if(!d.active || d.catalogueVersion!=c.version || d.context!=dialogueContext(s))return nullptr;
    const auto* b=c.find(d.bundle);if(!b || b->context!=d.context || d.line>=b->lines.size())return nullptr;
    const auto& l=b->lines[d.line];
    return l.id==d.lineId && actor(s,l.display.speaker)==d.speaker && actor(s,l.listener)==d.listener?&l:nullptr;
}
std::wstring catalogueText(const ExplorationState& s,const DialogueCatalogue& c) {
    if(const auto* l=selectedDialogue(s,c))return l->display.text;
    if(!dialogueContext(s).empty())return {}; // No candidate means no actor's line, including solo defeat.
    const auto text=gameplayText(s.gameplay);
    return text.empty()?gameplayNoticeText(s):text;
}
std::wstring revealedCatalogueText(const ExplorationState& s,const DialogueCatalogue& c,std::uint64_t now) {
    const auto text=catalogueText(s,c);const auto& d=s.variety.dialogue;
    if(!d.active) {
        if(!dialogueContext(s).empty())return {};
        if(gameplayText(s.gameplay).empty() && activeGameplayNotice(s))return revealedGameplayNotice(s,now);
        return revealedGameplayText(s.gameplay,now);
    }
    const auto elapsed=now>d.startedMs?now-d.startedMs:0;
    return text.substr(0,std::min<std::uint64_t>(text.size(),1+elapsed/25));
}
DialoguePortraits cataloguePortraits(const ExplorationState& s,const DialogueCatalogue& c) {
    const auto* l=selectedDialogue(s,c);if(!l)return gameplayNoticePortraits(s);
    return {l->display.speaker,l->display.speaker==PortraitRole::Alarm?109U:s.variety.dialogue.speaker,l->display.emotion};
}
void checkpointDialogue(ExplorationState& s,const DialogueCatalogue& c,std::uint64_t now) {
    const auto* line=selectedDialogue(s,c);if(!line)return;auto& d=s.variety.dialogue;
    const auto elapsed=now>d.startedMs?now-d.startedMs:0;
    d.revealed=static_cast<unsigned>(std::min<std::uint64_t>(line->display.text.size(),1+elapsed/25));
    const auto reveal=(line->display.text.size()-1)*25;
    d.readRemainingMs=static_cast<unsigned>(elapsed<reveal?d.deadlineMs-d.startedMs-reveal:now<d.deadlineMs?d.deadlineMs-now:0);
}
void Explorer::restoreDialogue(const DialogueCatalogue& c,std::uint64_t now) {
    auto& s=state_;restoreGameplayNoticeClock(s,now);auto& d=s.variety.dialogue;if(!d.active)return;
    const auto* line=selectedDialogue(s,c);
    if(!line || !dialogueEligible(s,d.context)) {
        if(eventActive(s)){const bool holiday=s.variety.holiday.active;cancelVariety(s);s.gameplay.deadlineMs=now;invalidate();if(holiday)(void)returnFromHoliday(now);return;}
        d={};if(dialogueContext(s).empty())return;
        if(s.gameplay.life==LifePhase::Dialogue && !companionDialogue(s.gameplay) && !actorExists(s,PortraitRole::Friendly)) {
            s.gameplay.life=LifePhase::Explore;s.gameplay.enemyInstance=0;s.gameplay.deadlineMs=now;invalidate();return;
        }
        if(s.gameplay.life==LifePhase::Dialogue)s.gameplay.dialogueStage=DialogueStage::Npc;
        s.gameplay.deadlineMs=now;(void)advanceGameplay(now,ActorCompletion{true,true,true});return;
    }
    const auto revealed=std::min<unsigned>(d.revealed,static_cast<unsigned>(line->display.text.size()));
    const auto elapsed=revealed?std::uint64_t(revealed-1)*25:0;
    d.startedMs=now>=elapsed?now-elapsed:0;
    d.deadlineMs=now+(line->display.text.size()-std::max(1U,revealed))*25+d.readRemainingMs;
    if(d.deadlineMs<=d.startedMs)d.deadlineMs=d.startedMs+1;
    s.gameplay.phaseStartedMs=d.startedMs;s.gameplay.deadlineMs=d.deadlineMs;
}
GameplayUpdate Explorer::advanceGameplay(std::uint64_t now,ActorCompletion done,const DialogueCatalogue& c,const EventRegistry* maps,const HolidayRegistry* visits) {
    updateGameplayNotices(state_,now);
    const auto update=advanceCatalogueGameplay(now,done,c,maps,visits);
    updateGameplayNotices(state_,now);
    return update;
}
GameplayUpdate Explorer::advanceCatalogueGameplay(std::uint64_t now,ActorCompletion done,const DialogueCatalogue& c,const EventRegistry* maps,const HolidayRegistry* visits) {
    auto& s=state_;auto& g=s.gameplay;auto& d=s.variety.dialogue;
    if(s.variety.holiday.active) {
        if(!visits || now>=s.variety.event.deadlineMs){cancelVariety(s);return returnFromHoliday(now);}
        const bool complete=advanceHoliday(s,c,*visits,now);
        if(complete || !s.variety.holiday.active)return returnFromHoliday(now);
        return {};
    }
    if(visits && g.life==LifePhase::Explore && g.sceneBeat==SceneBeat::ExploreFadeIn &&
       (s.variety.schedule.destination==DayDestination::Town || s.variety.schedule.destination==DayDestination::HotSpring)) {
        if(startHoliday(s,c,*visits,now))return {};
    }
    if(eventBattle(s)) {
        const auto update=advanceParty(now,done);auto& event=s.variety.event;
        const auto helper=event.id.type==VarietyEvent::JointBattle && !event.actors.empty()?event.actors.front():0;
        const auto actor=helper?std::find_if(s.population.entities.begin(),s.population.entities.end(),[&](const auto& a){return a.instanceId==helper;}):s.population.entities.end();
        const bool lost=helper && (actor==s.population.entities.end() || actor->done || !actor->remainingHits);
        if(g.life==LifePhase::Death || s.phase!=Phase::Explore || lost) {
            cancelVariety(s);if(lost && actor!=s.population.entities.end())actor->visible=false;invalidate();return update;
        }
        const bool settled=g.partyPlaybacks.empty() && std::none_of(g.partyActions.begin(),g.partyActions.end(),[](const auto& a){return !a.finished;});
        const bool victory=!event.opponents.empty() && std::all_of(event.opponents.begin(),event.opponents.end(),[&](auto id){const auto i=std::find_if(s.population.entities.begin(),s.population.entities.end(),[&](const auto& a){return a.instanceId==id;});return i!=s.population.entities.end() && i->done;});
        if(settled && victory) {
            g.partyActions.clear();g.partyPhase=PartyPhase::Idle;g.life=LifePhase::Explore;g.companionIntent=CompanionIntent::Follow;g.companionTarget=0;
            if(event.id.type==VarietyEvent::JointBattle && !event.partyContributed){cancelVariety(s);g.deadlineMs=now;invalidate();return update;}
            event.progress=EventProgress::Result;(void)commitVarietyResult(s,"victory");
            if(!startDialogueBundle(s,c,event.variant+".victory",now)){cancelVariety(s);g.deadlineMs=now;}
            return update;
        }
        if(settled && (now>=event.deadlineMs || (g.hearts<=kLowHp && !canUseOran(s,0)) || (g.companion.hearts<=kLowHp && !canUseOran(s,kCompanionActorId)))) {
            g.partyActions.clear();g.partyPhase=PartyPhase::Idle;cancelVariety(s);g.life=LifePhase::Explore;g.deadlineMs=now;invalidate();
        }
        return update;
    }
    if(eventActive(s)){const auto update=advanceVarietyEvent(s,now,c,maps);if(!eventActive(s))invalidate();return update;}
    if(!d.active && tryAutomaticEvent(s,now,c,maps))return s.variety.event.id.type==VarietyEvent::Reunion?GameplayUpdate{GameplayEvent::Warp,{}}:GameplayUpdate{GameplayEvent::DialoguePlayer,{}};
    if(d.active) {
        const auto* b=c.find(d.bundle);const auto* l=selectedDialogue(s,c);
        if(!b || !l || !dialogueEligible(s,d.context)) {
            restoreDialogue(c,now);return {};
        }
        checkpointDialogue(s,c,now);
        if(now<d.deadlineMs)return {};
        const bool timeout=now-d.deadlineMs>=6000;
        if(!animationDone(s,done) && !timeout)return {};
        if(d.line+1<b->lines.size()) {
            const auto previous=g.dialogueStage;assignLine(s,*b,d.line+1,now);
            return {previous!=g.dialogueStage?GameplayEvent::DialogueNpc:GameplayEvent::None,{}};
        }
        d={};g.deadlineMs=now;if(g.life==LifePhase::Dialogue)g.dialogueStage=DialogueStage::Npc;
        // Only the finite dialogue animation can time out here; combat is never advanced by this gate.
        if(timeout)done=ActorCompletion{true,true,true};
        return advanceGameplay(now,done);
    }
    if(!dialogueContext(s).empty()) {
        if(begin(s,c,now))return {};
        // No eligible bundle: skip this speech phase without drawing again on every frame.
        if(g.life==LifePhase::Dialogue && !companionDialogue(g) && !actorExists(s,PortraitRole::Friendly)) {
            g.life=LifePhase::Explore;g.enemyInstance=0;g.deadlineMs=now;invalidate();return {GameplayEvent::Explore,{}};
        }
        g.deadlineMs=now;if(g.life==LifePhase::Dialogue)g.dialogueStage=DialogueStage::Npc;
        return advanceGameplay(now,ActorCompletion{true,true,true});
    }
    const auto update=advanceGameplay(now,done);
    if(!dialogueContext(s).empty())(void)begin(s,c,now);
    return update;
}
bool startDialogueBundle(ExplorationState& s,const DialogueCatalogue& c,const std::string& id,std::uint64_t now) {
    const auto* b=c.find(id);if(!b || !std::all_of(b->lines.begin(),b->lines.end(),[&](const auto& l){return actorExists(s,l.display.speaker) && actorExists(s,l.listener);}))return false;
    assignLine(s,*b,0,now);s.variety.dialogue.catalogueVersion=c.version;return true;
}
bool advanceDialoguePage(ExplorationState& s,const DialogueCatalogue& c,std::uint64_t now) {
    auto& d=s.variety.dialogue;if(!d.active)return true;
    const auto* b=c.find(d.bundle);if(!b || !selectedDialogue(s,c)){d={};return true;}
    checkpointDialogue(s,c,now);if(now<d.deadlineMs)return false;
    if(d.line+1<b->lines.size()){assignLine(s,*b,d.line+1,now);return false;}
    d={};return true;
}
} // namespace pet
