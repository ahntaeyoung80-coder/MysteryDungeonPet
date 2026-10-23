#include "core/gameplay_scene.hpp"
#include <iostream>
#include <stdexcept>
using namespace pet;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){try {
    GameplayState g;g.enabled=true;
    check(gameplayText(g).empty() && idlePortrait(g).speaker==PortraitRole::None,"quiet exploration gained an idle portrait");
    Surface screen(200,200);gameplayOverlay(screen,g,0,nullptr,false,false);
    check(screen.pixels[(144*200+8)*4+3]==255 && screen.pixels[(191*200+191)*4+3]==255,"persistent two-row panel missing");
    check(screen.pixels[(143*200+8)*4+3]==0 && screen.pixels[(192*200+8)*4+3]==0,"two-row panel bounds differ");
    for(auto state:{DesktopState::Unknown,DesktopState::Active,DesktopState::Idle,DesktopState::Locked,DesktopState::DisplayOff,DesktopState::Suspended}) {
        const auto text=desktopStatusText(g,state);check(!text.empty() && text.find(L'\n')==std::wstring::npos,"desktop status is empty or multiline");
    }
    check(desktopStatusText(g,DesktopState::Active)!=desktopStatusText(g,DesktopState::Idle),"observed PC activity not distinguished");
    g.companion.enabled=true;g.homeScene=true;
    for(auto life:{LifePhase::Wake,LifePhase::Ready}) {
        g.life=life;const auto p=idlePortrait(g);
        check(p.speaker==PortraitRole::None,"morning idle portrait remains visible");
    }
    g.homeScene=false;g.life=LifePhase::Combat;
    check(desktopStatusText(g,DesktopState::Active).find(L"전투")!=std::wstring::npos,"combat status mislabeled");
    g.life=LifePhase::Death;g.deathStage=DeathStage::Blackout;
    check(idlePortrait(g).speaker==PortraitRole::None && desktopStatusText(g,DesktopState::Active).empty(),"persistent UI leaked onto blackout");
    std::cout<<"persistent two-row viewport, hidden idle portraits, observed desktop status and blackout precedence pass\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
