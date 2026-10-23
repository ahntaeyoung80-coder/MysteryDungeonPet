#include "windows/text_overlay.hpp"
#include "core/gameplay_scene.hpp"
#include "core/gameplay_notice.hpp"
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
        const auto* p=static_cast<const uint8_t*>(bits);unsigned ink=0,secondRowInk=0;
        for(unsigned y=0;y<200;++y) for(unsigned x=0;x<200;++x) {
            const auto offset=(y*200+x)*4;
            if(p[offset+2]>100 && p[offset+2]>surface.pixels[offset+2]) {
                if(p[offset+3]!=255) throw std::runtime_error("text ink invisible to layered window");
                if(scene!=5 && (x<8 || x>=192 || y<152 || y>=186)) throw std::runtime_error("text outside box");
                ++ink;if(y>=168)++secondRowInk;
            }
        }
        if(scene==0 && secondRowInk!=0)throw std::runtime_error("short berry text unnecessarily uses two rows");
        if(ink<50) throw std::runtime_error("text missing from actual GDI raster");
    }
    for(const auto& label:{L"1F",L"7F",L"128F"}) {
        std::memset(bits,0,200*200*4);renderer.drawMapHud(dc,bits,200,label);
        const auto* p=static_cast<const uint8_t*>(bits);unsigned ink=0;
        for(unsigned y=0;y<200;++y)for(unsigned x=0;x<200;++x) {
            const auto offset=(y*200+x)*4;
            if(p[offset+2]>100) {
                if(p[offset+3]!=255 || x<146 || x>=193 || y<8 || y>=23)
                    throw std::runtime_error("map HUD outside upper-right area or transparent");
                ++ink;
            }
        }
        if(ink<10)throw std::runtime_error("floor HUD ink missing");
    }
    const auto compact=CreateFontW(-12,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,NONANTIALIASED_QUALITY,DEFAULT_PITCH,L"Malgun Gothic");
    const auto oldFont=SelectObject(dc,compact);
    WORD hangul=0xffff;
    if(GetGlyphIndicesW(dc,L"좋",1,&hangul,GGI_MARK_NONEXISTING_GLYPHS)==GDI_ERROR || hangul==0xffff)
        throw std::runtime_error("Korean glyph unavailable in the selected test font");
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path()/"assets/original/gameplay/items/items.mdpa";
    for(const auto& clip:loadPackage(root).clips) {
        const auto text=itemDisplayName(clip.id)+L"을 주웠다.";RECT box{0,0,168,32};
        if(!DrawTextW(dc,text.c_str(),-1,&box,DT_CALCRECT|DT_WORDBREAK|DT_NOPREFIX) || box.bottom>32 || box.right>168)
            throw std::runtime_error("selected item acquisition exceeds two rows");
    }
    SelectObject(dc,oldFont);DeleteObject(compact);
    SelectObject(dc,previous);DeleteObject(bitmap);DeleteDC(dc);
    std::cout<<"eight Korean text scenes and three map/floor HUD labels have visible opaque GDI ink\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
