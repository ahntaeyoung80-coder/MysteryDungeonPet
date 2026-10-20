#pragma once
#include <windows.h>
#include <string>
namespace pet {
class TextOverlay {
    HFONT font_;
public:
    TextOverlay();
    ~TextOverlay();
    TextOverlay(const TextOverlay&)=delete;
    TextOverlay& operator=(const TextOverlay&)=delete;
    void draw(HDC dc,void* bits,unsigned size,const std::wstring& text,bool blackout) const;
    void drawSpeaker(HDC dc,void* bits,unsigned size,const std::wstring& label,bool right) const;
};
}
