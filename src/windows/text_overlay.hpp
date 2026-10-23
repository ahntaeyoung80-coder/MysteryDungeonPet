#pragma once
#include <windows.h>
#include <string>
namespace pet {
class TextOverlay {
    HFONT font_,hudFont_;
    HFONT compactFonts_[3]{};
public:
    TextOverlay();
    ~TextOverlay();
    TextOverlay(const TextOverlay&)=delete;
    TextOverlay& operator=(const TextOverlay&)=delete;
    void draw(HDC dc,void* bits,unsigned size,const std::wstring& text,bool blackout) const;
    void drawMapHud(HDC dc,void* bits,unsigned size,const std::wstring& label) const;
    void drawMapTitle(HDC dc,void* bits,unsigned size,const std::wstring&,unsigned opacity) const;
};
}
