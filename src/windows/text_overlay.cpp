#include "windows/text_overlay.hpp"
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>
namespace pet {
TextOverlay::TextOverlay():font_(CreateFontW(-12,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
    OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,NONANTIALIASED_QUALITY,DEFAULT_PITCH,L"Malgun Gothic")) {
    hudFont_=CreateFontW(-11,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,NONANTIALIASED_QUALITY,DEFAULT_PITCH,L"Malgun Gothic");
    if(!font_ || !hudFont_) throw std::runtime_error("text font creation failed");
}
TextOverlay::~TextOverlay() {DeleteObject(font_);DeleteObject(hudFont_);}
void TextOverlay::drawMapTitle(HDC dc,void* bits,unsigned size,const std::wstring& text,unsigned opacity) const {
    if(text.empty() || !opacity)return;
    auto* pixels=static_cast<std::uint8_t*>(bits);std::vector<std::uint8_t> original(pixels,pixels+size*size*4);
    for(unsigned y=78;y<std::min(size,112U);++y)for(unsigned x=4;x+4<size;++x)
        for(unsigned ch=0;ch<3;++ch)pixels[(y*size+x)*4+ch]/=2;
    const auto old=SelectObject(dc,font_);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(255,255,255));
    RECT rect{8,78,LONG(size)-8,112};
    const auto height=DrawTextW(dc,text.c_str(),-1,&rect,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
    const auto flushed=GdiFlush();SelectObject(dc,old);
    if(!height || !flushed)throw std::runtime_error("map title rasterization failed");
    for(unsigned y=78;y<std::min(size,112U);++y)for(unsigned x=0;x<size;++x) {
        const auto at=(y*size+x)*4;
        for(unsigned ch=0;ch<3;++ch)pixels[at+ch]=std::uint8_t((unsigned(pixels[at+ch])*opacity+unsigned(original[at+ch])*(255-opacity)+127)/255);
        pixels[at+3]=255;
    }
}
void TextOverlay::drawMapHud(HDC dc,void* bits,unsigned size,const std::wstring& label) const {
    if(label.empty())return;
    const auto oldFont=SelectObject(dc,hudFont_);SetBkMode(dc,TRANSPARENT);
    RECT rect{146,8,192,22};auto shadow=rect;OffsetRect(&shadow,1,1);
    SetTextColor(dc,RGB(0,0,0));DrawTextW(dc,label.c_str(),-1,&shadow,DT_RIGHT|DT_SINGLELINE|DT_NOPREFIX);
    SetTextColor(dc,RGB(255,255,255));const auto height=DrawTextW(dc,label.c_str(),-1,&rect,DT_RIGHT|DT_SINGLELINE|DT_NOPREFIX);
    const auto flushed=GdiFlush();SelectObject(dc,oldFont);
    if(!height || !flushed)throw std::runtime_error("map HUD rasterization failed");
    auto* pixels=static_cast<std::uint8_t*>(bits);
    for(unsigned y=8;y<std::min(size,23U);++y)for(unsigned x=146;x<std::min(size,193U);++x)
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
