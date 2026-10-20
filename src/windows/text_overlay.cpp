#include "windows/text_overlay.hpp"
#include <algorithm>
#include <cstdint>
#include <stdexcept>
namespace pet {
TextOverlay::TextOverlay():font_(CreateFontW(-12,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
    OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,NONANTIALIASED_QUALITY,DEFAULT_PITCH,L"Malgun Gothic")) {
    hudFont_=CreateFontW(-11,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,NONANTIALIASED_QUALITY,DEFAULT_PITCH,L"Malgun Gothic");
    if(!font_ || !hudFont_) throw std::runtime_error("text font creation failed");
}
TextOverlay::~TextOverlay() {DeleteObject(font_);DeleteObject(hudFont_);}
void TextOverlay::drawMapHud(HDC dc,void* bits,unsigned size,const std::wstring& label) const {
    if(label.empty())return;
    const auto oldFont=SelectObject(dc,hudFont_);SetBkMode(dc,TRANSPARENT);
    auto shown=label;SIZE extent{};
    const auto suffix=label.rfind(L"] - ");
    if(suffix!=std::wstring::npos) {
        auto name=label.substr(1,suffix-1);const auto floor=label.substr(suffix);
        GetTextExtentPoint32W(dc,shown.c_str(),int(shown.size()),&extent);
        while(extent.cx>184 && name.size()>1) {
            name.pop_back();shown=L"["+name+L"…"+floor;
            GetTextExtentPoint32W(dc,shown.c_str(),int(shown.size()),&extent);
        }
    }
    RECT rect{8,24,192,40};auto shadow=rect;OffsetRect(&shadow,1,1);
    SetTextColor(dc,RGB(0,0,0));DrawTextW(dc,shown.c_str(),-1,&shadow,DT_RIGHT|DT_SINGLELINE|DT_NOPREFIX);
    SetTextColor(dc,RGB(255,255,255));const auto height=DrawTextW(dc,shown.c_str(),-1,&rect,DT_RIGHT|DT_SINGLELINE|DT_NOPREFIX);
    const auto flushed=GdiFlush();SelectObject(dc,oldFont);
    if(!height || !flushed)throw std::runtime_error("map HUD rasterization failed");
    auto* pixels=static_cast<std::uint8_t*>(bits);
    for(unsigned y=24;y<std::min(size,41U);++y)for(unsigned x=8;x<std::min(size,193U);++x)
        pixels[(y*size+x)*4+3]=255;
}
void TextOverlay::draw(HDC dc,void* bits,unsigned size,const std::wstring& text,bool blackout) const {
    if(text.empty()) return;
    const auto oldFont=SelectObject(dc,font_);
    SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(0,0,0));
    RECT rect=blackout?RECT{8,80,192,130}:RECT{16,152,184,184};
    auto shadow=rect;OffsetRect(&shadow,1,1);
    DrawTextW(dc,text.c_str(),-1,&shadow,(blackout?DT_CENTER:DT_LEFT)|DT_WORDBREAK|DT_NOPREFIX);
    SetTextColor(dc,RGB(255,255,255));
    const auto height=DrawTextW(dc,text.c_str(),-1,&rect,(blackout?DT_CENTER:DT_LEFT)|DT_WORDBREAK|DT_NOPREFIX);
    // GDI batches writes and clears DIB alpha. Flush before repairing alpha for ULW_ALPHA.
    const auto flushed=GdiFlush();SelectObject(dc,oldFont);
    if(!height || !flushed) throw std::runtime_error("text rasterization failed");
    auto* pixels=static_cast<std::uint8_t*>(bits);
    for(unsigned y=blackout?0:150;y<std::min(size,blackout?size:190U);++y)
        for(unsigned x=blackout?0:8;x<std::min(size,blackout?size:192U);++x) pixels[(y*size+x)*4+3]=255;
}
}
