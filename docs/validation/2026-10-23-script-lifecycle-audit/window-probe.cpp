#include <windows.h>
BOOL CALLBACK visit(HWND hwnd,LPARAM) {
 wchar_t name[128]{};GetClassNameW(hwnd,name,128);
 if(wcscmp(name,L"MysteryDungeonPet.A2")==0 || wcscmp(name,L"MysteryDungeonPet")==0) {
  SendMessageW(hwnd,WM_WTSSESSION_CHANGE,0x8,0);
 }
 return TRUE;
}
int main(){EnumWindows(visit,0);return 0;}
