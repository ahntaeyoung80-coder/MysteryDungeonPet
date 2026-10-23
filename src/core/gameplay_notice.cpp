#include "core/gameplay_notice.hpp"
#include <algorithm>
namespace pet {
namespace {
bool safeComment(const ExplorationState& s) {
    const auto& g=s.gameplay;
    return g.companion.enabled && g.hearts>kLowHp && g.companion.hearts>kLowHp &&
        canAttack(s.world,s.position,g.companion.tile) &&
        std::none_of(s.population.entities.begin(),s.population.entities.end(),[&](const auto& e){
            const auto distance=[&](int tile){return std::max(std::abs(tile%kWidth-e.tile%kWidth),std::abs(tile/kWidth-e.tile/kWidth));};
            return e.visible && !e.done && std::find(g.hostile.begin(),g.hostile.end(),e.instanceId)!=g.hostile.end() &&
                (distance(s.position)<=4 || distance(g.companion.tile)<=4);
        });
}
bool canDisplay(const ExplorationState& s) {
    const auto& g=s.gameplay;
    return !varietyBusy(s.variety) && s.phase==Phase::Explore && dungeonScene(g) &&
        g.sceneBeat==SceneBeat::None && (g.life==LifePhase::Explore || g.life==LifePhase::Combat) && gameplayText(g).empty();
}
std::wstring text(const GameplayNotice& n) {
    if(n.kind==NoticeKind::Victory)return {};
    const auto name=itemDisplayName(n.item);
    if(n.kind==NoticeKind::Pickup) {
        const auto last=name.back();
        const bool consonant=last>=0xac00 && last<=0xd7a3 && (last-0xac00)%28!=0;
        return name+(consonant?L"을 주웠다.":L"를 주웠다.");
    }
    return itemCommentText(n.item,n.variant);
}
PortraitEmotion itemMood(unsigned item) {
    if((item>=300 && item<=361) || (item>=188 && item<=276) || item==362)return PortraitEmotion::Inspired;
    if(item==111 || item==105 || item==117)return PortraitEmotion::Worried;
    return item==183?PortraitEmotion::Joyous:PortraitEmotion::Happy;
}
void append(ExplorationState& s,NoticeKind kind,unsigned item,unsigned variant=0) {
    auto& queue=s.variety.notices;
    if(queue.size()==64) {
        const auto comment=std::find_if(queue.begin(),queue.end(),[](const auto& n){return n.kind==NoticeKind::ItemComment;});
        queue.erase(comment==queue.end()?queue.begin():comment);
    }
    queue.push_back({kind,item,0,0,variant});
}
}
std::wstring itemDisplayName(std::uint32_t item) {
    switch(item) {
#include "core/item_names.inc"
    default:return L"이름 모를 도구";
    }
}
std::wstring itemCommentText(std::uint32_t item,std::uint32_t variant) {
    switch(item) {
#include "core/item_comments.inc"
    default:return L"이건 뭘까? 자세히 보고 싶어.";
    }
}
void queueGameplayNotice(ExplorationState& s,NoticeKind kind,unsigned item) {
    if(kind==NoticeKind::Victory)return;
    if(kind!=NoticeKind::ItemComment)append(s,kind,item);
    if(kind==NoticeKind::ItemComment || (kind==NoticeKind::Pickup && safeComment(s) && drawVariety(s.variety,VarietyRandomStream::Dialogue,5)==0)) {
        const auto context="item."+std::to_string(item);
        auto recent=std::find_if(s.variety.recent.begin(),s.variety.recent.end(),[&](const auto& r){return r.context==context;});
        unsigned variant=drawVariety(s.variety,VarietyRandomStream::Dialogue,recent==s.variety.recent.end()?3:2);
        if(recent!=s.variety.recent.end()) {
            const auto previous=unsigned(recent->bundle.back()-'0');if(variant>=previous)++variant;
            recent->bundle="v"+std::to_string(variant);
        } else {
            if(s.variety.recent.size()==64)s.variety.recent.erase(s.variety.recent.begin());
            s.variety.recent.push_back({context,"v"+std::to_string(variant),0});
        }
        append(s,NoticeKind::ItemComment,item,variant);
    }
}
const GameplayNotice* activeGameplayNotice(const ExplorationState& s) {
    const auto& q=s.variety.notices;
    return !q.empty() && q.front().kind!=NoticeKind::Victory && q.front().startedMs && canDisplay(s) &&
        (q.front().kind!=NoticeKind::ItemComment || safeComment(s))?&q.front():nullptr;
}
std::wstring gameplayNoticeText(const ExplorationState& s) {const auto* n=activeGameplayNotice(s);return n?text(*n):std::wstring{};}
std::wstring revealedGameplayNotice(const ExplorationState& s,std::uint64_t now) {
    const auto* n=activeGameplayNotice(s);if(!n)return {};
    const auto value=text(*n);const auto elapsed=now>n->startedMs?now-n->startedMs:0;
    return value.substr(0,std::min<std::uint64_t>(value.size(),1+elapsed/25));
}
DialoguePortraits gameplayNoticePortraits(const ExplorationState& s) {
    const auto* n=activeGameplayNotice(s);
    return n && n->kind==NoticeKind::ItemComment?DialoguePortraits{PortraitRole::Partner,kCompanionActorId,itemMood(n->item)}:DialoguePortraits{};
}
void updateGameplayNotices(ExplorationState& s,std::uint64_t now) {
    auto& q=s.variety.notices;
    q.erase(std::remove_if(q.begin(),q.end(),[](const auto& n){return n.kind==NoticeKind::Victory;}),q.end());
    if(s.phase!=Phase::Explore || s.gameplay.life==LifePhase::Death || s.gameplay.homeScene){q.clear();return;}
    if(!q.empty() && q.front().startedMs && now>=q.front().deadlineMs)q.erase(q.begin());
    // Optional comments are discarded when danger or another conversation takes priority.
    while(!q.empty() && q.front().kind==NoticeKind::ItemComment && (!safeComment(s) || varietyBusy(s.variety)))q.erase(q.begin());
    if(q.empty())return;
    auto& n=q.front();
    if(!canDisplay(s)){n.startedMs=n.deadlineMs=0;return;}
    if(!n.startedMs){n.startedMs=std::max<std::uint64_t>(1,now);n.deadlineMs=n.startedMs+(text(n).size()-1)*25+1800;}
}
void restoreGameplayNoticeClock(ExplorationState& s,std::uint64_t now) {
    for(auto& notice:s.variety.notices)notice.startedMs=notice.deadlineMs=0;
    updateGameplayNotices(s,now);
}
} // namespace pet
