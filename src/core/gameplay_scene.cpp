#include "core/gameplay_scene.hpp"
#include "core/exploration_io.hpp"
#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <stdexcept>
#include <set>
namespace pet {
bool partyHealing(const GameplayState& g) {
    return g.independentParty && std::any_of(g.partyActions.begin(),g.partyActions.end(),[](const auto& a){return a.kind==PartyActionKind::Berry && !a.finished;});
}
std::wstring gameplayText(const GameplayState& g) {
    if(partyHealing(g) || (g.life==LifePhase::Combat && (g.combat==CombatPhase::BerryText || g.combat==CombatPhase::CompanionBerryText)) || g.sceneBeat==SceneBeat::ExplorationBerryText) return L"오랭열매! 2하트 회복";
    if(companionDialogue(g)) return g.dialogueStage==DialogueStage::Player?L"잘 따라오고 있지? 잠깐 쉬어 가자.":L"물론이지! 함께라면 든든해.";
    if(g.life==LifePhase::Dialogue) return g.dialogueStage==DialogueStage::Player?L"안녕! 같이 탐험하자.":L"조심해서 다녀와!";
    if(g.life==LifePhase::Sleep && g.sceneBeat==SceneBeat::BedFace) return L"잘 자! 내일도 같이 탐험하자.";
    if(g.life==LifePhase::Wake && g.sceneBeat==SceneBeat::WakeDelay) return L"노공룡: 일어나아아! 아침이다아아!";
    if(g.life==LifePhase::Ready && g.sceneBeat==SceneBeat::MorningFace) return L"좋은 아침! 함께 출발하자.";
    if(g.life==LifePhase::Death && g.deathStage==DeathStage::Blackout) return L"눈앞이 깜깜해졌다";
    return {};
}
std::wstring revealedGameplayText(const GameplayState& g,std::uint64_t now) {
    const auto text=gameplayText(g);
    if(g.life==LifePhase::Death)return text;
    const auto elapsed=now>g.phaseStartedMs?now-g.phaseStartedMs:0;
    return text.substr(0,std::min<std::uint64_t>(text.size(),1+elapsed/25));
}
std::size_t loopFrameAt(const Package& p,std::uint32_t clip,std::uint64_t elapsed) {
    const auto& frames=p.frames(clip,0);std::uint64_t total=0;
    for(const auto& f:frames)total+=f.ticks;
    auto tick=(elapsed*p.tickHz/1000)%total;
    for(std::size_t i=0;i<frames.size();++i){if(tick<frames[i].ticks)return i;tick-=frames[i].ticks;}
    return frames.size()-1;
}
Surface originalDialoguePanel(const Surface& atlas,const Package& package) {
    return originalWindowPanel(atlas,package,184,48);
}
Surface originalWindowPanel(const Surface& atlas,const Package& package,unsigned width,unsigned height) {
    if(width<24 || height<24 || width>200 || height>200 || width%8 || height%8)
        throw std::runtime_error("original window requires 8px tile dimensions");
    const auto& tiles=package.frames(0,0);
    Surface panel(width,height);std::vector<Layer> layers;
    constexpr unsigned ids[3][3]{{2,3,4},{5,1,6},{7,8,9}};
    for(unsigned y=0;y<height;y+=8)for(unsigned x=0;x<width;x+=8) {
        const auto& tile=tiles.at(ids[y==0?0:y==height-8?2:1][x==0?0:x==width-8?2:1]);
        layers.push_back({&atlas,tile.rect,{0,0},{int32_t(x),int32_t(y)}});
    }
    compose(panel,layers,{0,0},1);return panel;
}
const DialogueActor& DialogueCast::actor(PortraitRole role) const {
    const auto found=std::find_if(actors.begin(),actors.end(),[&](const auto& a){return a.role==role;});
    if(found==actors.end())throw std::runtime_error("dialogue role has no character binding");
    return *found;
}
std::string DialogueCast::label(PortraitRole role) const {
    const char* prefix=role==PortraitRole::Player?"나 · ":role==PortraitRole::Partner?"동료 · ":role==PortraitRole::Friendly?"NPC · ":"";
    return prefix+actor(role).displayName;
}
DialogueCast loadDialogueCast(const std::filesystem::path& path) {
    const auto bytes=readFile(path,8192);detail::Reader r(bytes);
    if(r.u32()!=0x4344444d || r.u32()!=1 || r.count(4)!=4)throw std::runtime_error("invalid dialogue cast header");
    DialogueCast cast;std::set<unsigned> roles;
    for(unsigned i=0;i<4;++i) {
        DialogueActor a;a.role=static_cast<PortraitRole>(r.count(4));a.portraitClip=r.count(65535);
        a.characterId=r.str();a.bodyAssetKey=r.str();a.bodyAtlasSha256=r.str();a.bodyManifest=r.str();a.displayName=r.str();
        if(a.role==PortraitRole::None || !roles.insert(static_cast<unsigned>(a.role)).second || !a.portraitClip ||
           a.characterId.empty() || a.bodyAssetKey.empty() || a.displayName.empty() || a.bodyAtlasSha256.size()!=64 ||
           !std::all_of(a.bodyAtlasSha256.begin(),a.bodyAtlasSha256.end(),[](char c){return (c>='0' && c<='9') || (c>='a' && c<='f');}))
            throw std::runtime_error("invalid dialogue cast identity");
        const auto manifest=std::filesystem::u8path(a.bodyManifest);
        if(a.bodyManifest.empty() || manifest.is_absolute() || a.bodyManifest.find_first_of("\\:")!=std::string::npos)
            throw std::runtime_error("invalid dialogue body path");
        for(const auto& part:manifest)if(part==".." || part==".")throw std::runtime_error("unsafe dialogue body path");
        cast.actors.push_back(std::move(a));
    }
    r.end();return cast;
}
void validateDialogueActor(const DialogueActor& actor,const Package& body,const Package& portraits) {
    if(actor.bodyAssetKey!=body.assetKey || actor.bodyAtlasSha256!=body.pngSha256 || portraits.resolve(actor.portraitClip).sourceMetadata!=actor.characterId)
        throw std::runtime_error("dialogue body/portrait character identity mismatch: "+actor.characterId);
}
DialoguePortraits dialoguePortraits(const GameplayState& g) {
    if(g.life==LifePhase::Dialogue) {
        const auto other=companionDialogue(g)?PortraitRole::Partner:PortraitRole::Friendly;
        return g.dialogueStage==DialogueStage::Player?DialoguePortraits{PortraitRole::Player,0}:
            DialoguePortraits{other,g.enemyInstance};
    }
    if((g.life==LifePhase::Sleep && g.sceneBeat==SceneBeat::BedFace) ||
       (g.life==LifePhase::Ready && g.sceneBeat==SceneBeat::MorningFace))
        return {PortraitRole::Player,0};
    if(g.life==LifePhase::Wake && g.sceneBeat==SceneBeat::WakeDelay)
        return {PortraitRole::Alarm,109};
    return {};
}
void dialoguePortraitOverlay(Surface& s,const Surface& atlas,const Package& package,DialoguePortraits portraits,
    const DialogueCast& cast,const Surface& frame,const Surface& nameplate) {
    if(portraits.speaker==PortraitRole::None)return;
    const auto& actor=cast.actor(portraits.speaker);
    const auto& rect=package.frames(actor.portraitClip,0).front().rect;
    if(package.resolve(actor.portraitClip).sourceMetadata!=actor.characterId || rect.w!=40 || rect.h!=40 ||
       rect.x<0 || rect.y<0 || uint64_t(rect.x)+40>atlas.width || uint64_t(rect.y)+40>atlas.height)
        throw std::runtime_error("invalid dialogue portrait character/region");
    auto blit=[&](const Surface& source,Rect area,unsigned left,unsigned top) {
        for(unsigned y=0;y<unsigned(area.h) && top+y<s.height;++y)for(unsigned x=0;x<unsigned(area.w) && left+x<s.width;++x) {
            const auto src=((area.y+y)*source.width+area.x+x)*4,dst=((top+y)*s.width+left+x)*4;
            const auto inv=255-source.pixels[src+3];
            for(unsigned ch=0;ch<4;++ch)s.pixels[dst+ch]=uint8_t(source.pixels[src+ch]+(s.pixels[dst+ch]*inv+127)/255);
        }
    };
    const bool player=portraits.speaker==PortraitRole::Player;const unsigned left=player?8:136;
    blit(frame,{0,0,int32_t(frame.width),int32_t(frame.height)},left,88);
    blit(atlas,rect,left+8,96);
    blit(nameplate,{0,0,int32_t(nameplate.width),int32_t(nameplate.height)},player?72:16,120);
}
bool guildAppleVisible(const GameplayState& g) {
    return g.homeScene && !bedroomScene(g);
}
unsigned bedroomDirection(const GameplayState& g,unsigned fallback,bool partner) {
    const bool beforeBed=g.life==LifePhase::Sleep && (g.sceneBeat==SceneBeat::NightFadeIn ||
        g.sceneBeat==SceneBeat::BedFadeIn || g.sceneBeat==SceneBeat::BedFace);
    return beforeBed || g.sceneBeat==SceneBeat::MorningFace?(partner?2U:6U):fallback;
}
unsigned bedroomPlaybackScale(const GameplayState& g) {
    return g.life==LifePhase::Sleep && g.sceneBeat==SceneBeat::BedFace?4U:1U;
}
BedroomExitSample bedroomExit(Point start,const GameplayState& g,std::uint64_t now,bool partner) {
    // The leader walks straight left; the companion follows once the leader has passed.
    const int end=partner?104:80;
    const std::array<Point,2> path{{start,{end,184}}};
    std::uint64_t elapsed=g.sceneBeat==SceneBeat::MorningFadeOut?kRoomExitMs:now>g.phaseStartedMs?now-g.phaseStartedMs:0;
    if(partner) {
        constexpr std::uint64_t followDelay=1200;
        if(elapsed<followDelay)return {start,6,false,false};
        elapsed-=followDelay;
    }
    for(unsigned i=1;i<path.size();++i) {
        const auto from=path[i-1],to=path[i];
        const auto dx=to.x-from.x,dy=to.y-from.y;
        const auto duration=std::uint64_t(std::lround(std::hypot(dx,dy)*400/24));
        const unsigned direction=dx<0?(dy<0?5:dy>0?7:6):dx>0?(dy<0?3:dy>0?1:2):dy<0?4:0;
        if(elapsed<duration) return {{from.x+int32_t(dx*int64_t(elapsed)/int64_t(duration)),from.y+int32_t(dy*int64_t(elapsed)/int64_t(duration))},direction,false,true};
        elapsed-=duration;
    }
    return {path.back(),6,true,false};
}
Point bedroomExitCamera(const GameplayState& g,std::uint64_t now) {
    const auto p=bedroomExit({204,184},g,now).position;
    const int left=std::max(0,180-p.x);
    // Start at the greeting's camera and ease into tracking; keep the vertical view steady.
    const int shift=left<24?left*left/48:left-12;
    return {180-shift,184};
}
BedroomExitSample loudredAlarm(Point original,const GameplayState& g,std::uint64_t now) {
    const auto elapsed=std::min<std::uint64_t>(800,now>g.phaseStartedMs?now-g.phaseStartedMs:0);
    if(g.sceneBeat==SceneBeat::AlarmEnter)return {{original.x-48+int32_t(48*elapsed/800),original.y},2,elapsed==800};
    if(g.sceneBeat==SceneBeat::AlarmLeave)return {{original.x-int32_t(48*elapsed/800),original.y},6,elapsed==800};
    return {original,2,false};
}
void gameplayOverlay(Surface& s, const GameplayState& g, std::uint64_t now,const Surface* panel) {
    auto pixel = [&](unsigned x, unsigned y, unsigned blue, unsigned green, unsigned red) {
        if (x >= s.width || y >= s.height) return;
        const auto i = (y * s.width + x) * 4;
        s.pixels[i] = static_cast<std::uint8_t>(blue); s.pixels[i+1] = static_cast<std::uint8_t>(green);
        s.pixels[i+2] = static_cast<std::uint8_t>(red); s.pixels[i+3] = 255;
    };
    if (dungeonScene(g)) {
        static const char* heart[]{"0110110", "1111111", "1111111", "0111110", "0011100", "0001000"};
        for (unsigned h = 0; h < kMaxHp; ++h) for (unsigned y = 0; y < 6; ++y) for (unsigned x = 0; x < 7; ++x)
            if (heart[y][x] == '1') for(unsigned yy=0;yy<2;++yy) for(unsigned xx=0;xx<2;++xx)
                pixel(8 + 18*h + 2*x+xx,8+2*y+yy,35,35,h<g.hearts?230:65);
    }
    if (!panel && (partyHealing(g) || (g.life == LifePhase::Combat && (g.combat == CombatPhase::BerryText || g.combat == CombatPhase::CompanionBerryText)) || g.sceneBeat==SceneBeat::ExplorationBerryText))
        for (unsigned y = 150; y < std::min(190U, s.height); ++y)
            for (unsigned x = 8; x < std::min(192U, s.width); ++x) pixel(x, y, 40, 30, 20);
    if(g.life==LifePhase::Death) {
        const auto elapsed=now>g.phaseStartedMs?now-g.phaseStartedMs:0;
        const auto opacity=g.deathStage==DeathStage::Blackout?255:std::min<std::uint64_t>(255,elapsed*255/750);
        for(std::size_t i=0;i<s.pixels.size();i+=4) {
            for(unsigned ch=0;ch<3;++ch) s.pixels[i+ch]=static_cast<uint8_t>(s.pixels[i+ch]*(255-opacity)/255);
            s.pixels[i+3]=255; // death is an opaque blackout, never desktop transparency
        }
    }
    const bool out=g.sceneBeat==SceneBeat::DiningExit || g.sceneBeat==SceneBeat::BedFadeOut || g.sceneBeat==SceneBeat::MorningFadeOut || g.sceneBeat==SceneBeat::FloorFadeOut;
    const bool in=g.sceneBeat==SceneBeat::NightFadeIn || g.sceneBeat==SceneBeat::ExploreFadeIn || g.sceneBeat==SceneBeat::BedFadeIn || g.sceneBeat==SceneBeat::FloorFadeIn;
    if(out||in) {
        const auto elapsed=now>g.phaseStartedMs?now-g.phaseStartedMs:0;
        const auto duration=(g.sceneBeat==SceneBeat::FloorFadeOut || g.sceneBeat==SceneBeat::FloorFadeIn)?kFloorFadeMs:750;
        const auto opacity=out?std::min<uint64_t>(255,elapsed*255/duration):elapsed>=duration?0:(duration-elapsed)*255/duration;
        for(size_t i=0;i<s.pixels.size();i+=4) {for(unsigned ch=0;ch<3;++ch) s.pixels[i+ch]=static_cast<uint8_t>(s.pixels[i+ch]*(255-opacity)/255);s.pixels[i+3]=255;}
    }
    if(!panel && (g.life==LifePhase::Dialogue || (g.life==LifePhase::Wake && g.sceneBeat==SceneBeat::WakeDelay) || (g.life==LifePhase::Sleep && g.sceneBeat==SceneBeat::BedFace) || (g.life==LifePhase::Ready && g.sceneBeat==SceneBeat::MorningFace)))
        for(unsigned y=150;y<190;++y) for(unsigned x=8;x<192;++x) pixel(x,y,40,30,20);
    if(panel && g.life!=LifePhase::Death && !gameplayText(g).empty()) {
        for(unsigned y=0;y<panel->height && y+144<s.height;++y)
            for(unsigned x=0;x<panel->width && x+8<s.width;++x) {
                const auto src=(y*panel->width+x)*4,dst=((y+144)*s.width+x+8)*4;
                const auto inv=255-panel->pixels[src+3];
                for(unsigned ch=0;ch<4;++ch)s.pixels[dst+ch]=uint8_t(panel->pixels[src+ch]+(s.pixels[dst+ch]*inv+127)/255);
            }
    }
    if (g.life == LifePhase::ReturnHome) {
        const auto elapsed = now > g.phaseStartedMs ? now - g.phaseStartedMs : 0;
        const auto opacity = g.fadeInOnly ? (elapsed>=750?0:(750-elapsed)*255/750) : g.homeScene ? (elapsed >= 1500 ? 0 : std::min<std::uint64_t>(255, (1500-elapsed)*255/750)) : std::min<std::uint64_t>(255, elapsed*255/750);
        for (std::size_t i = 0; i < s.pixels.size(); i += 4)
            for (unsigned ch = 0; ch < 3; ++ch) s.pixels[i+ch] = static_cast<std::uint8_t>(s.pixels[i+ch] * (255-opacity) / 255);
    }
}
}
