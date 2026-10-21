#include <windows.h>
#include <wtsapi32.h>
#include <string>
struct Search {DWORD process;HWND window;};
BOOL CALLBACK find(HWND window,LPARAM arg) {
    auto& search=*reinterpret_cast<Search*>(arg);DWORD pid=0;GetWindowThreadProcessId(window,&pid);
    wchar_t cls[128]{};GetClassNameW(window,cls,128);
    if(pid==search.process && std::wstring(cls)==L"MysteryDungeonPet.A2"){search.window=window;return FALSE;}
    return TRUE;
}
int wmain(int argc,wchar_t** argv) {
    if(argc<3)return 2;
    std::wstring cmd;for(int i=1;i<argc;++i){if(i>1)cmd+=L" ";cmd+=L"\""+std::wstring(argv[i])+L"\"";}
    STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION process{};
    if(!CreateProcessW(nullptr,cmd.data(),nullptr,nullptr,FALSE,0,nullptr,nullptr,&startup,&process))return 3;
    Search search{process.dwProcessId,nullptr};
    for(unsigned i=0;i<200 && !search.window;++i){EnumWindows(find,reinterpret_cast<LPARAM>(&search));Sleep(25);}
    if(search.window)PostMessageW(search.window,WM_WTSSESSION_CHANGE,WTS_SESSION_UNLOCK,0);
    const auto result=WaitForSingleObject(process.hProcess,180000);
    if(result!=WAIT_OBJECT_0)TerminateProcess(process.hProcess,4);
    DWORD code=4;GetExitCodeProcess(process.hProcess,&code);CloseHandle(process.hThread);CloseHandle(process.hProcess);
    return static_cast<int>(code);
}
