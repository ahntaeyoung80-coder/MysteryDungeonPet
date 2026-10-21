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
    if(n.kind==NoticeKind::Victory)return L"적을 쓰러뜨렸다!";
    const auto name=itemDisplayName(n.item);
    if(n.kind==NoticeKind::Pickup)return name+L"\n획득했다!";
    if(n.item==ITEM_ORAN_BERRY)return L"오랭열매네!\n하트를 모두 채워 줘.";
    if(n.item==109 || n.item==110)return L"사과를 찾았네!\n잘 챙겨 두자.";
    return name+L"\n찾았네! 잘 챙겨 두자.";
}
void append(ExplorationState& s,NoticeKind kind,unsigned item) {
    auto& queue=s.variety.notices;
    if(queue.size()==64) {
        const auto comment=std::find_if(queue.begin(),queue.end(),[](const auto& n){return n.kind==NoticeKind::ItemComment;});
        queue.erase(comment==queue.end()?queue.begin():comment);
    }
    queue.push_back({kind,item,0,0});
}
}
std::wstring itemDisplayName(std::uint32_t item) {
    switch(item) {
#include "core/item_names.inc"
    default:return L"아이템 #"+std::to_wstring(item);
    }
}
void queueGameplayNotice(ExplorationState& s,NoticeKind kind,unsigned item) {
    append(s,kind,item);
    // Once per acquisition, on the dialogue stream. Rendering never draws RNG.
    if(kind==NoticeKind::Pickup && safeComment(s) && drawVariety(s.variety,VarietyRandomStream::Dialogue,5)==0)
        append(s,NoticeKind::ItemComment,item);
}
const GameplayNotice* activeGameplayNotice(const ExplorationState& s) {
    const auto& q=s.variety.notices;
    return !q.empty() && q.front().startedMs && canDisplay(s) &&
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
    return n && n->kind==NoticeKind::ItemComment?DialoguePortraits{PortraitRole::Partner,kCompanionActorId,PortraitEmotion::Happy}:DialoguePortraits{};
}
void updateGameplayNotices(ExplorationState& s,std::uint64_t now) {
    auto& q=s.variety.notices;
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
