#include "windows/text_overlay.hpp"
#include "core/gameplay_scene.hpp"
#include <cstring>
#include <iostream>
#include <stdexcept>
int main() {try {
    using namespace pet;
    HDC dc=CreateCompatibleDC(nullptr);BITMAPINFO info{};
    info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=200;info.bmiHeader.biHeight=-200;
    info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    void* bits=nullptr;const auto bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);
    if(!dc || !bitmap) throw std::runtime_error("DIB creation failed");
    const auto previous=SelectObject(dc,bitmap);TextOverlay renderer;
    GameplayState g;g.sceneBeat=SceneBeat::ExplorationBerryText;
    for(unsigned scene=0;scene<8;++scene) {
        if(scene==1) {g.sceneBeat=SceneBeat::None;g.life=LifePhase::Dialogue;g.dialogueStage=DialogueStage::Player;}
        if(scene==2) g.dialogueStage=DialogueStage::Npc;
        if(scene==3) {g.life=LifePhase::Sleep;g.sceneBeat=SceneBeat::BedFace;}
        if(scene==4) {g.life=LifePhase::Ready;g.sceneBeat=SceneBeat::MorningFace;}
        if(scene==5) {g.life=LifePhase::Death;g.sceneBeat=SceneBeat::None;g.deathStage=DeathStage::Blackout;}
        if(scene==6) {g.life=LifePhase::Wake;g.sceneBeat=SceneBeat::WakeDelay;}
        if(scene==7) {g.life=LifePhase::Pose;g.sceneBeat=SceneBeat::CompletionText;}
        Surface surface(200,200);gameplayOverlay(surface,g,0);
        std::memcpy(bits,surface.pixels.data(),surface.pixels.size());
        renderer.draw(dc,bits,200,gameplayText(g),scene==5);
        const auto* p=static_cast<const uint8_t*>(bits);unsigned ink=0;
        for(unsigned y=0;y<200;++y) for(unsigned x=0;x<200;++x) {
            const auto offset=(y*200+x)*4;
            if(p[offset+2]>100 && p[offset+2]>surface.pixels[offset+2]) {
                if(p[offset+3]!=255) throw std::runtime_error("text ink invisible to layered window");
                if(scene!=5 && (x<8 || x>=192 || y<150 || y>=190)) throw std::runtime_error("text outside box");
                ++ink;
            }
        }
        if(ink<50) throw std::runtime_error("text missing from actual GDI raster");
    }
    for(const auto& label:{L"[뜨거운 동굴 상층] - 1F",L"[뜨거운 동굴 정상] - 7F",L"[아주 긴 이름의 던전에서도 층수는 보존한다] - 99F"}) {
        std::memset(bits,0,200*200*4);renderer.drawMapHud(dc,bits,200,label);
        const auto* p=static_cast<const uint8_t*>(bits);unsigned ink=0;
        for(unsigned y=0;y<200;++y)for(unsigned x=0;x<200;++x) {
            const auto offset=(y*200+x)*4;
            if(p[offset+2]>100) {
                if(p[offset+3]!=255 || x<8 || x>=193 || y<24 || y>=41)
                    throw std::runtime_error("map HUD outside upper-right area or transparent");
                ++ink;
            }
        }
        if(ink<50)throw std::runtime_error("map/floor HUD ink missing");
    }
    SelectObject(dc,previous);DeleteObject(bitmap);DeleteDC(dc);
    std::cout<<"eight Korean text scenes and three map/floor HUD labels have visible opaque GDI ink\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
