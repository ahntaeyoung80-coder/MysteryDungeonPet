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
    if(n.kind==NoticeKind::Pickup) {
        const auto last=name.back();
        const bool consonant=last>=0xac00 && last<=0xd7a3 && (last-0xac00)%28!=0;
        return name+(consonant?L"을 주웠다.":L"를 주웠다.");
    }
    if(n.item==ITEM_ORAN_BERRY)return L"지치면 이 열매를 먹자!";
    if(n.item==109)return L"사과네! 배고플 때 먹자.";
    if(n.item==110)return L"커다란 사과! 든든하겠다.";
    if(n.item==111)return L"이건 먹기 전에 살펴보자.";
    if(n.item==73)return L"만약을 위해 잘 챙겨 두자.";
    if(n.item==105 || n.item==117)return L"닮았어도 다른 도구야!";
    if(n.item==183)return L"돈이네! 차곡차곡 모으자.";
    if(n.item==362)return L"기술을 연결하는 상자래!";
    if(n.item>=300 && n.item<=361) {
        if(n.item==330)return L"돌아갈 때 쓸 수 있겠어!";
        if(n.item==318 || n.item==331 || n.item==332)return L"숨은 길도 보이려나?";
        if(n.item==314)return L"어디로 워프하게 될까?";
        return n.item%2?L"이 구슬, 정말 신기하다!":L"반짝이는 구슬! 예쁘다!";
    }
    if(n.item>=188 && n.item<=276)return n.item%2?L"어떤 기술을 배울까?":L"새로운 기술이라니 멋져!";
    if(n.item>=119 && n.item<=137)return n.item%2?L"이 구미는 무슨 맛일까?":L"알록달록한 구미네!";
    if(n.item>=13 && n.item<=50)return n.item%2?L"잘 어울리는 장비겠어!":L"어떤 힘이 숨어 있을까?";
    if(n.item>=69 && n.item<=108)return L"효과를 살피고 써 보자.";
    if(n.item<=11)return L"멀리서도 쓸 수 있겠어!";
    return L"탐험에 도움이 되겠지!";
}
PortraitEmotion itemMood(unsigned item) {
    if((item>=300 && item<=361) || (item>=188 && item<=276) || item==362)return PortraitEmotion::Inspired;
    if(item==111 || item==105 || item==117)return PortraitEmotion::Worried;
    return item==183?PortraitEmotion::Joyous:PortraitEmotion::Happy;
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
    default:return L"이름 모를 도구";
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
    return n && n->kind==NoticeKind::ItemComment?DialoguePortraits{PortraitRole::Partner,kCompanionActorId,itemMood(n->item)}:DialoguePortraits{};
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
