#include <windows.h>
#include <wtsapi32.h>
#include <cstdlib>
int main(int argc,char** argv) {
    HWND window=nullptr;
    for(unsigned attempt=0;attempt<200 && !window;++attempt) {
        window=FindWindowW(L"MysteryDungeonPet.A2",L"MysteryDungeonPet");Sleep(50);
    }
    if(!window)return 2;
    PostMessageW(window,WM_WTSSESSION_CHANGE,WTS_SESSION_UNLOCK,0);
    Sleep(argc>1?std::strtoul(argv[1],nullptr,10):4000);
    PostMessageW(window,WM_CLOSE,0,0);return 0;
}
