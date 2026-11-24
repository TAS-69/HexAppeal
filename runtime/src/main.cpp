#include <Windows.h>
#include <MinHook.h>
#include "runtime.h"
#include "hooks/dx12_hook.h"
#include "core/log.h"
#include "npc_runtime.h"
#include "skin_runtime.h"
#include "character_addons.h"
#include "wardrobe.h"
#include "game_signatures.h"
#include <cwchar>

static DWORD WINAPI start(LPVOID self) {
    wchar_t path[32768]{};GetModuleFileNameW(nullptr,path,32768);
    const auto name=wcsrchr(path,L'\\');
    if(_wcsicmp(name?name+1:path,L"CrimsonDesert.exe")!=0)return 0;
    auto& r=hex::Runtime::get();r.initialize(static_cast<HMODULE>(self));
    // Only for a game build missing from the address table: runs on its own
    // thread, so the DXGI attachment below is never delayed.
    hex::startGameLayoutResolution(r.folder/L"HexAppeal.ini");
    const auto hooks=MH_Initialize();
    if(hooks!=MH_OK&&hooks!=MH_ERROR_ALREADY_INITIALIZED){
        LOG_ERR("Hook initialization failed (%s). The overlay can still use its swapchain wrapper.",MH_StatusToString(hooks));
    }
    const auto ini=r.folder/L"HexAppeal.ini";
    const bool reshade=GetPrivateProfileIntW(L"General",L"UseReShadeEvents",1,ini.c_str())!=0;
    if(!ml::hooks::InstallDX12Hooks(static_cast<HMODULE>(self),reshade))LOG_ERR("No overlay presentation route could be installed.");
    auto manifest=r.configuredDependencyPaths().manifest;
    if(manifest.empty())manifest=r.folder/L"dmm/manifest.tsv";
    else if(manifest.is_relative())manifest=r.folder.parent_path()/manifest;
    hex::initializeCharacterAddons(r.profiles.parent_path(),manifest);
    hex::startSkinTone();
    hex::startWardrobeRuntime();
    hex::startNpcRuntime();
    return 0;
}
BOOL WINAPI DllMain(HINSTANCE self,DWORD reason,LPVOID) {
    if(reason==DLL_PROCESS_ATTACH){DisableThreadLibraryCalls(self);const auto thread=CreateThread(nullptr,0,start,self,0,nullptr);if(thread)CloseHandle(thread);else OutputDebugStringW(L"Hex Appeal: could not start ASI worker.\n");}
    return TRUE;
}
