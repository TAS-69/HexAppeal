#include <Windows.h>
#include <MinHook.h>
#include "runtime.h"
#include "hooks/dx12_hook.h"
#include "core/log.h"
#include "npc_runtime.h"
#include <cwchar>

static DWORD WINAPI start(LPVOID self) {
    wchar_t path[32768]{};GetModuleFileNameW(nullptr,path,32768);
    const auto name=wcsrchr(path,L'\\');
    if(_wcsicmp(name?name+1:path,L"CrimsonDesert.exe")!=0)return 0;
    auto& r=hex::Runtime::get();r.initialize(static_cast<HMODULE>(self));
    if(MH_Initialize()!=MH_OK){LOG_ERR("Could not initialize the menu hook library.");return 0;}
    const auto ini=r.folder/L"HexAppeal.ini";
    const bool reshade=GetPrivateProfileIntW(L"General",L"UseReShadeEvents",1,ini.c_str())!=0;
    if(!ml::hooks::InstallDX12Hooks(static_cast<HMODULE>(self),reshade))LOG_ERR("No overlay presentation route could be installed.");
    hex::startNpcRuntime();
    return 0;
}
BOOL WINAPI DllMain(HINSTANCE self,DWORD reason,LPVOID) {
    if(reason==DLL_PROCESS_ATTACH){DisableThreadLibraryCalls(self);const auto thread=CreateThread(nullptr,0,start,self,0,nullptr);if(thread)CloseHandle(thread);}
    return TRUE;
}
