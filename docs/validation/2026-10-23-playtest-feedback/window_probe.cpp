#include <windows.h>
#include <wtsapi32.h>
#include <iostream>
#include <string>
int main(int argc,char** argv){
 HWND window=nullptr;
 for(int i=0;i<100 && !window;++i){window=FindWindowW(L"MysteryDungeonPet.A2",nullptr);if(!window)Sleep(100);}
 if(!window)return 2;
 if(argc>1 && std::string(argv[1])=="close")PostMessageW(window,WM_CLOSE,0,0);
 else PostMessageW(window,WM_WTSSESSION_CHANGE,WTS_SESSION_UNLOCK,0);
 std::cout<<"posted to application window\n";return 0;
}
