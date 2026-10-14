#include "core/gameplay_scene.hpp"
#include <algorithm>
#include <cstdlib>
namespace pet {
BedroomExitSample bedroomExit(Point start,const GameplayState& g,std::uint64_t now) {
    const std::array<Point,5> path{{start,{204,208},{120,208},{120,184},{-24,184}}};
    std::uint64_t elapsed=now>g.phaseStartedMs?now-g.phaseStartedMs:0;
    for(unsigned i=1;i<path.size();++i) {
        const auto from=path[i-1],to=path[i];
        const auto dx=to.x-from.x,dy=to.y-from.y;
        const auto duration=std::uint64_t(std::abs(dx)+std::abs(dy))*400/24;
        const unsigned direction=dx<0?6:dx>0?2:dy<0?4:0;
        if(elapsed<duration) return {{from.x+int32_t(dx*int64_t(elapsed)/int64_t(duration)),from.y+int32_t(dy*int64_t(elapsed)/int64_t(duration))},direction,false};
        elapsed-=duration;
    }
    return {path.back(),6,true};
}
void gameplayOverlay(Surface& s, const GameplayState& g, std::uint64_t now) {
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
    if ((g.life == LifePhase::Combat && g.combat == CombatPhase::BerryText) || g.sceneBeat==SceneBeat::ExplorationBerryText)
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
    if(g.life==LifePhase::Dialogue) for(unsigned y=150;y<190;++y) for(unsigned x=8;x<192;++x) pixel(x,y,40,30,20);
    if (g.life == LifePhase::ReturnHome) {
        const auto elapsed = now > g.phaseStartedMs ? now - g.phaseStartedMs : 0;
        const auto opacity = g.fadeInOnly ? (elapsed>=750?0:(750-elapsed)*255/750) : g.homeScene ? (elapsed >= 1500 ? 0 : std::min<std::uint64_t>(255, (1500-elapsed)*255/750)) : std::min<std::uint64_t>(255, elapsed*255/750);
        for (std::size_t i = 0; i < s.pixels.size(); i += 4)
            for (unsigned ch = 0; ch < 3; ++ch) s.pixels[i+ch] = static_cast<std::uint8_t>(s.pixels[i+ch] * (255-opacity) / 255);
    }
}
}
