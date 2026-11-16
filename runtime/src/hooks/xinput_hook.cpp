
// MIT License, Copyright (c) 2026 XeTrinityz. See runtime/licenses/UPSTREAM-NOTICES.md.

#include "xinput_hook.h"

#include "../core/log.h"
#include "../core/settings.h"

#include <MinHook.h>

#include "../core/state.h"

namespace ml::hooks
{
    using XInputGetState_t = DWORD (WINAPI*)(DWORD, XINPUT_STATE*);



    static XInputGetState_t o_1_4   = nullptr;
    static XInputGetState_t o_1_3   = nullptr;
    static XInputGetState_t o_9_1_0 = nullptr;

    static void Neutralize(XINPUT_STATE* s)
    {
        const DWORD packet = s->dwPacketNumber;
        ZeroMemory(&s->Gamepad, sizeof s->Gamepad);
        s->dwPacketNumber = packet + 1;
    }




    static void MaskShortcut(XINPUT_STATE* s)
    {
        const unsigned mask = State::Get().padShortcut.load();
        if (!mask)
            return;
        auto& pad = s->Gamepad;
        const unsigned held = pad.wButtons |
            (pad.bLeftTrigger >= 64 ? (1u << 16) : 0) | (pad.bRightTrigger >= 64 ? (1u << 17) : 0);
        if ((held & mask) != mask)
            return;
        pad.wButtons &= static_cast<WORD>(~mask);
        if (mask & (1u << 16)) pad.bLeftTrigger = 0;
        if (mask & (1u << 17)) pad.bRightTrigger = 0;
    }
    static void FilterState(XINPUT_STATE* state,bool foreground){
        if(State::Get().BlocksGameInput()&&foreground)Neutralize(state);else MaskShortcut(state);
    }



    #define ML_XINPUT_DETOUR(NAME, ORIG)                              \
        static DWORD WINAPI NAME(DWORD i, XINPUT_STATE* s)                 \
        {                                                                  \
            const DWORD r = ORIG(i, s);                                    \
            if (r == ERROR_SUCCESS && s)                                   \
            {                                                              \
                FilterState(s,State::ForegroundIsOurs());                    \
            }                                                              \
            return r;                                                      \
        }
    ML_XINPUT_DETOUR(hk_1_4,   o_1_4)
    ML_XINPUT_DETOUR(hk_1_3,   o_1_3)
    ML_XINPUT_DETOUR(hk_9_1_0, o_9_1_0)
    #undef ML_XINPUT_DETOUR

    struct Target
    {
        const wchar_t*    dll;
        void*             detour;
        XInputGetState_t* original;
        bool              done;
    };

    static Target g_targets[] = {
        { L"xinput1_4.dll",   reinterpret_cast<void*>(&hk_1_4),   &o_1_4,   false },
        { L"xinput1_3.dll",   reinterpret_cast<void*>(&hk_1_3),   &o_1_3,   false },
        { L"xinput9_1_0.dll", reinterpret_cast<void*>(&hk_9_1_0), &o_9_1_0, false },
    };

    void EnsureXInputHooks()
    {
        static bool s_allDone = false;
        if (s_allDone)
            return;


        if (!Settings::Get().hookXInput&&!State::Get().blockGameInput)
        {
            static bool s_said = false;
            if (!s_said)
            {
                s_said = true;
                LOG("HookXInput=0: the controller is left alone, so it still reaches the game "
                    "while the menu is open.");
            }
            return;
        }

        bool anyPending = false;
        for (auto& t : g_targets)
        {
            if (t.done)
                continue;

            HMODULE mod = GetModuleHandleW(t.dll);
            if (!mod)
            {
                anyPending = true;
                continue;
            }

            FARPROC proc = GetProcAddress(mod, "XInputGetState");
            if (!proc)
            {
                t.done = true;
                continue;
            }

            if (MH_CreateHook(reinterpret_cast<void*>(proc), t.detour,
                              reinterpret_cast<void**>(t.original)) != MH_OK)
            {
                anyPending = true;
                continue;
            }

            if (MH_EnableHook(reinterpret_cast<void*>(proc)) == MH_OK)
            {
                t.done = true;
            }
            else
            {
                anyPending = true;
            }
        }

        if (!anyPending)
            s_allDone = true;
    }




    static XInputGetState_t RawGetState()
    {
        if (o_1_4) return o_1_4;
        if (o_1_3) return o_1_3;
        if (o_9_1_0) return o_9_1_0;
        static XInputGetState_t direct = nullptr;
        static bool looked = false;
        if (!looked)
        {
            looked = true;
            HMODULE mod = GetModuleHandleW(L"xinput1_4.dll");
            if (!mod) mod = GetModuleHandleW(L"xinput1_3.dll");
            if (!mod) mod = LoadLibraryW(L"xinput1_4.dll");
            if (mod) direct = reinterpret_cast<XInputGetState_t>(GetProcAddress(mod, "XInputGetState"));
        }

        return direct;
    }

    bool ReadPadRaw(XINPUT_GAMEPAD* out)
    {
        static int slot = -1;
        static DWORD nextLook = 0;
        const auto get = RawGetState();
        if (!get)
            return false;
        XINPUT_STATE state = {};
        if (slot >= 0)
        {
            if (get(static_cast<DWORD>(slot), &state) == ERROR_SUCCESS)
            {
                *out = state.Gamepad;
                return true;
            }
            slot = -1;
            nextLook = 0;
        }
        const DWORD now = GetTickCount();
        if (nextLook && static_cast<LONG>(now - nextLook) < 0)
            return false;
        nextLook = now + 2000;
        for (DWORD i = 0; i < XUSER_MAX_COUNT; ++i)
            if (get(i, &state) == ERROR_SUCCESS)
            {
                slot = static_cast<int>(i);
                *out = state.Gamepad;
                return true;
            }
        return false;
    }
#ifdef MODACTIVATOR_TESTS
    extern "C" int HexAppealControllerPolicyTest(){
        auto& state=State::Get();state.menuOpen=true;state.lastDrawAt=GetTickCount();state.blockGameInput=true;state.padShortcut=0;
        XINPUT_STATE source{};source.dwPacketNumber=4;source.Gamepad.wButtons=XINPUT_GAMEPAD_A;source.Gamepad.sThumbLX=17000;source.Gamepad.bRightTrigger=200;
        auto value=source;FilterState(&value,true);if(value.Gamepad.wButtons||value.Gamepad.sThumbLX||value.Gamepad.bRightTrigger||value.dwPacketNumber!=5)return 1;
        state.blockGameInput=false;value=source;FilterState(&value,true);if(memcmp(&value,&source,sizeof(source)))return 2;
        state.blockGameInput=true;state.lastDrawAt=GetTickCount()-501;value=source;FilterState(&value,true);if(memcmp(&value,&source,sizeof(source)))return 3;
        state.lastDrawAt=GetTickCount();value=source;FilterState(&value,false);if(memcmp(&value,&source,sizeof(source)))return 4;
        state.menuOpen=false;value=source;FilterState(&value,true);if(memcmp(&value,&source,sizeof(source)))return 5;
        return 0;
    }
#endif
}
