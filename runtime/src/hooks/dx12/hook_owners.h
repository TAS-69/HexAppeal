










    static const char* OwningModule(void* addr, char* out, size_t n)
    {
        HMODULE m = nullptr;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCSTR>(addr), &m);
        char path[MAX_PATH] = "?";
        if (m) GetModuleFileNameA(m, path, MAX_PATH);
        const char* base = strrchr(path, '\\');
        strncpy(out, base ? base + 1 : path, n - 1);
        out[n - 1] = 0;
        return out;
    }




    static const char* OwnModuleName()
    {
        static const std::string name = [] {
            char b[64] = "";
            OwningModule(reinterpret_cast<void*>(&OwnModuleName), b, sizeof b);
            return std::string(b);
        }();
        return name.c_str();
    }



    static void DescribeDetour(const uint8_t* p, const void* at, char* out, size_t n);

    static void LogHookedModule(const char* what, void* addr)
    {
        char mod[64];
        OwningModule(addr, mod, sizeof mod);



        char into[96] = "";
        if (addr && !IsBadReadPtr(addr, 16))
        {
            uint8_t b[16];
            memcpy(b, addr, sizeof b);
            DescribeDetour(b, addr, into, sizeof into);
        }
        LOG("%s @ %p in %s%s", what, addr, mod, into);
    }



    static bool IsSystemOwner(const char* mod)
    {
        return _stricmp(mod, "dxgi.dll") == 0 || _stricmp(mod, "d3d12.dll") == 0 ||
               _stricmp(mod, "d3d12core.dll") == 0;
    }












    static bool ModuleInSystemDir(void* addr)
    {
        HMODULE m = nullptr;
        if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                reinterpret_cast<LPCSTR>(addr), &m) || !m)
            return false;
        char path[MAX_PATH] = "";
        if (!GetModuleFileNameA(m, path, MAX_PATH)) return false;
        char sys[MAX_PATH] = "";
        const UINT n = GetSystemDirectoryA(sys, MAX_PATH);
        if (!n || n >= MAX_PATH) return false;
        return _strnicmp(path, sys, n) == 0 && (path[n] == '\\' || path[n] == '/');
    }
















    static bool DetourTarget(const uint8_t* p, const void* at, void** out)
    {
        const uintptr_t a = reinterpret_cast<uintptr_t>(at);
        if (p[0] == 0xE9)
        {
            int32_t rel = 0; memcpy(&rel, p + 1, sizeof rel);
            *out = reinterpret_cast<void*>(a + 5 + static_cast<intptr_t>(rel));
            return true;
        }
        if (p[0] == 0xEB)
        {
            *out = reinterpret_cast<void*>(a + 2 + static_cast<intptr_t>(static_cast<int8_t>(p[1])));
            return true;
        }
        if (p[0] == 0xFF && p[1] == 0x25)
        {


            int32_t disp = 0; memcpy(&disp, p + 2, sizeof disp);
            void* slot = reinterpret_cast<void*>(a + 6 + static_cast<intptr_t>(disp));
            if (IsBadReadPtr(slot, sizeof(void*))) return false;
            memcpy(out, slot, sizeof(void*));
            return true;
        }
        if (p[0] == 0x48 && p[1] == 0xB8 && p[10] == 0xFF && p[11] == 0xE0)
        {
            memcpy(out, p + 2, sizeof(void*));
            return true;
        }
        return false;
    }








    static void DescribeDetour(const uint8_t* p, const void* at, char* out, size_t n)
    {
        out[0] = 0;
        void* dest = nullptr;
        if (!DetourTarget(p, at, &dest) || !dest) return;

        char owner[64];
        OwningModule(dest, owner, sizeof owner);













        for (int hop = 0; hop < 6 && (IsSystemOwner(owner) || owner[0] == '?'); ++hop)
        {
            if (IsBadReadPtr(dest, 16)) break;
            uint8_t b[16];
            memcpy(b, dest, sizeof b);
            void* next = nullptr;
            if (!DetourTarget(b, dest, &next) || !next || next == dest) break;
            dest = next;
            OwningModule(dest, owner, sizeof owner);
        }


        if (owner[0] == '?')
            snprintf(out, n, ", into an allocated stub at %p", dest);
        else
            snprintf(out, n, ", into %s at %p", owner, dest);
    }

    static const char* ExistingDetour(const uint8_t* p)
    {
        if (p[0] == 0xE9) return "jmp rel32";
        if (p[0] == 0xFF && p[1] == 0x25) return "jmp [rip+disp]";
        if (p[0] == 0xEB) return "jmp short";
        if (p[0] == 0x68 && p[5] == 0xC3) return "push/ret";
        if (p[0] == 0x48 && p[1] == 0xB8 && p[10] == 0xFF && p[11] == 0xE0) return "mov rax, imm64; jmp rax";
        return nullptr;
    }



    static bool DetourOwner(void* fn, char* out, size_t n)
    {
        if (!fn || IsBadReadPtr(fn, 16)) return false;
        uint8_t b[16];
        memcpy(b, fn, sizeof b);
        if (!ExistingDetour(b)) return false;
        void* dest = nullptr;
        if (!DetourTarget(b, fn, &dest) || !dest) { strncpy(out, "?", n); out[n - 1] = 0; return true; }
        OwningModule(dest, out, n);
        for (int hop = 0; hop < 6 && (IsSystemOwner(out) || out[0] == '?'); ++hop)
        {
            if (IsBadReadPtr(dest, 16)) break;
            uint8_t c[16];
            memcpy(c, dest, sizeof c);
            void* next = nullptr;
            if (!DetourTarget(c, dest, &next) || !next) break;
            dest = next;
            OwningModule(dest, out, n);
        }
        return true;
    }











    static bool WrapWanted()
    {
        static int s_decided = -1;
        if (s_decided < 0)
        {
            const int setting = Settings::Get().wrapSwapChain;
            const bool streamline = GetModuleHandleA("sl.interposer.dll") != nullptr;
            const bool wrap = setting >= 0 ? setting != 0 : streamline;
            s_decided = wrap;
            if (setting < 0)
                LOG("WrapSwapChain=auto: Streamline (DLSS frame generation) is %s, so the swapchain is %s.",
                    streamline ? "loaded" : "not loaded",
                    wrap ? "wrapped and the menu draws before frame generation"
                         : "left alone and the menu draws through the present hook");
            else
                LOG("WrapSwapChain=%d: the swapchain is %s.", setting,
                    wrap ? "wrapped" : "left alone. The overlay draws through the present hook, which does not show under DLSS frame generation");
        }
        return s_decided != 0;
    }







    static bool BenignPresentOwner(const char* mod)
    {
        if (!mod || !mod[0] || mod[0] == '?') return false;
        if (IsSystemOwner(mod)) return true;
        if (_stricmp(mod, "gameoverlayrenderer64.dll") == 0) return true;
        if (_stricmp(mod, OwnModuleName()) == 0) return true;
        if (_strnicmp(mod, "sl.", 3) == 0 || _strnicmp(mod, "nvngx", 5) == 0) return true;
        return false;
    }




    static const char* KeyingOverlayLoaded()
    {
        static const char* const kMods[] = { "CrimsonRoute.asi" };
        for (const char* m : kMods) if (GetModuleHandleA(m)) return m;
        return nullptr;
    }

    static bool LogHookTarget(const char* what, void* addr, bool stack = false, bool allowProxy = false)
    {
        char mod[64];
        OwningModule(addr, mod, sizeof mod);



        uint8_t b[16] = {};
        char bytes[64] = "";
        const char* detour = nullptr;
        if (addr && !IsBadReadPtr(addr, sizeof b))
        {
            memcpy(b, addr, sizeof b);
            int w = 0;
            for (int i = 0; i < 8; ++i) w += snprintf(bytes + w, sizeof bytes - w, "%02X ", b[i]);
            detour = ExistingDetour(b);
        }

        if (detour)
        {
            char into[96];
            DescribeDetour(b, addr, into, sizeof into);
            if (stack)
            {



                LOG("[hook] %s @ %p in %s starts with %s (%s)%s: another mod detoured it first; stacking this mod's detour on top, so ours runs and then theirs",
                    what, addr, mod, detour, bytes, into);
            }
            else
            {
                LOG("[hook] %s @ %p in %s starts with %s (%s)%s: another mod detoured it first, so this one is left alone",
                    what, addr, mod, detour, bytes, into);
                return false;
            }
        }
        if (!IsSystemOwner(mod))
        {














            if (!allowProxy)
            {
                LOG("[hook] %s @ %p in %s (%s), which is not a system DLL: another mod owns it, so this one is left alone", what, addr, mod, bytes);
                return false;
            }
            LOG("[hook] %s @ %p in %s (%s) belongs to another mod outright, and it owns every present call there is: "
                "stacking on it, because the alternative is a menu that never draws", what, addr, mod, bytes);
            return true;
        }







        if (!ModuleInSystemDir(addr) && _stricmp(mod, "d3d12core.dll") != 0)
        {











            if (!allowProxy)
            {
                LOG("[hook] %s @ %p in %s (%s), a module named like a system DLL but living in the game folder, "
                    "so it is another mod's proxy: left alone", what, addr, mod, bytes);
                return false;
            }
            LOG("[hook] %s @ %p in %s (%s) is another mod's proxy in the game folder, and it owns every present "
                "call there is: stacking on it, because the alternative is a menu that never draws", what, addr, mod, bytes);
            return true;
        }
        LOG("[hook] %s @ %p in %s (%s)", what, addr, mod, bytes);
        return true;
    }
