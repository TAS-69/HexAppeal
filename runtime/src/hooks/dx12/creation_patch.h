







    static bool PatchVtableSlot(void** vtable, int index, void* newFn, void** origOut)
    {
        DWORD old = 0;
        if (!VirtualProtect(&vtable[index], sizeof(void*), PAGE_EXECUTE_READWRITE, &old))
            return false;
        *origOut = vtable[index];
        vtable[index] = newFn;
        VirtualProtect(&vtable[index], sizeof(void*), old, &old);
        return true;
    }




    static void WatchCreationDetour()
    {
        HANDLE thread = CreateThread(nullptr, 0, [](LPVOID) -> DWORD {
            int displaced = 0;
            for (int i = 0; i < 600; ++i)
            {
                Sleep(50);
                if (g_factoryCalls.load(std::memory_order_relaxed) > 0) break;
                const auto observed = hex::creation::Observe(
                    g_factoryFn, reinterpret_cast<void*>(&hkFactoryCreateSwapChainForHwnd));
                if (observed.state == hex::creation::EntryState::Ours) { displaced = 0; continue; }
                if (++displaced < 10) continue;
                char owner[64] = "?";
                if (observed.destination) OwningModule(observed.destination, owner, sizeof owner);
                LOG("[hook] CreateSwapChainForHwnd entry now leads to %s at %p (inspection=%u). "
                    "Keeping the captured original and foreign relays unchanged; another hook may still call ours.",
                    owner, observed.destination, static_cast<unsigned>(observed.state));
                break;
            }
            return 0;
        }, nullptr, 0, nullptr);
        if (thread) CloseHandle(thread);
    }

    static void InstallSwapChainCreationPatch()
    {
        IDXGIFactory2* factory = nullptr;
        if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory))) || !factory)
        {
            if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))) || !factory)
            {
                LOG_ERR("FG: no factory to patch - overlay-over-FrameGen disabled.");
                return;
            }
        }


















        void** vt = *reinterpret_cast<void***>(factory);
        const auto began=GetTickCount64();hex::creation::FactorySettle settle(began);
        uintptr_t first=0,current=0;
        for(;;){
            if(!hex::creation::Read(vt+15,&current,sizeof current)){factory->Release();LOG_ERR("FG: factory dispatch became unreadable before installation.");return;}
            if(!first)first=current;
            if(settle.observe(GetTickCount64(),current))break;
            Sleep(50);
        }
        LOG("FG: factory dispatch settled after %llu ms (%p -> %p). No previous hook or relay was redirected.",GetTickCount64()-began,reinterpret_cast<void*>(first),reinterpret_cast<void*>(current));
        void* fn = vt[15];
        g_factoryVt = vt;
        g_factoryFn = fn;
        if (LogHookTarget("Factory CreateSwapChainForHwnd", fn, true) &&
            MH_CreateHook(fn, reinterpret_cast<void*>(&hkFactoryCreateSwapChainForHwnd),
                          reinterpret_cast<void**>(&oFactoryCreateSwapChainForHwnd)) == MH_OK &&
            MH_EnableHook(fn) == MH_OK)
        {
            g_factoryInPlace = true;
            g_factoryOwnTramp = oFactoryCreateSwapChainForHwnd;
            LOG("FG: CreateSwapChainForHwnd detoured in place - the factory slot still points into dxgi.dll, so Steam's overlay keeps its hooks.");
            WatchCreationDetour();
        }
        else
        {

            LOG_ERR("FG: could not detour CreateSwapChainForHwnd in place; replacing the vtable slot instead, which Steam's overlay may refuse to hook over.");
            if (PatchVtableSlot(vt, 15, reinterpret_cast<void*>(&hkFactoryCreateSwapChainForHwnd),
                                reinterpret_cast<void**>(&oFactoryCreateSwapChainForHwnd)))
                LOG("FG: CreateSwapChainForHwnd slot patched.");
            else
                LOG_ERR("FG: VirtualProtect of CreateSwapChainForHwnd slot failed.");
        }

        factory->Release();
    }










    static void LogEntry(const char* what, void* fn)
    {
        char mod[64] = "?", bytes[40] = "", into[96] = "";
        if (!fn || IsBadReadPtr(fn, 16)) { LOG("[creation] %s @ %p: unreadable", what, fn); return; }
        OwningModule(fn, mod, sizeof mod);
        uint8_t b[16];
        memcpy(b, fn, sizeof b);
        int w = 0;
        for (int i = 0; i < 8; ++i) w += snprintf(bytes + w, sizeof bytes - w, "%02X ", b[i]);
        const char* detour = ExistingDetour(b);
        if (detour) DescribeDetour(b, fn, into, sizeof into);
        if (w > 0) bytes[w - 1] = 0;
        LOG("[creation] %s @ %p in %s (%s) %s%s%s", what, fn, mod, bytes,
            detour ? "starts with " : "not detoured", detour ? detour : "", into);
    }

    static void ReportCreationPath()
    {
        void** vt = g_factoryVt;
        if (vt && !IsBadReadPtr(vt, 25 * sizeof(void*)))
        {
            LOG("[creation] this mod's CreateSwapChainForHwnd detour has run %ld time(s); the slot %s the function it was read from",
                g_factoryCalls.load(std::memory_order_relaxed), vt[15] == g_factoryFn ? "still points at" : "no longer points at");
            char owner[64] = "";
            if (!g_factoryInPlace)
                LOG("[creation] the in-place detour never went in, so this mod relies on the slot patch");
            else if (DetourOwner(g_factoryFn, owner, sizeof owner))
                LOG("[creation] the entry it was installed on jumps into %s%s", owner[0] == '?' ? "an allocated stub" : owner,
                    _stricmp(owner, OwnModuleName()) == 0 ? ", so the detour is still first in line"
                                                              : ", so something detoured it after this mod did");
            else
                LOG("[creation] the entry it was installed on is not detoured any more: something put the original bytes back");
            LogEntry("slot 15 CreateSwapChainForHwnd", vt[15]);
            LogEntry("slot 10 CreateSwapChain", vt[10]);
            LogEntry("slot 16 CreateSwapChainForCoreWindow", vt[16]);
            LogEntry("slot 24 CreateSwapChainForComposition", vt[24]);
        }
        else
            LOG("[creation] the factory's CreateSwapChainForHwnd was never looked at, so there is no detour to compare");
        if (HMODULE dxgi = GetModuleHandleW(L"dxgi.dll"))
        {
            LogEntry("CreateDXGIFactory1", reinterpret_cast<void*>(GetProcAddress(dxgi, "CreateDXGIFactory1")));
            LogEntry("CreateDXGIFactory2", reinterpret_cast<void*>(GetProcAddress(dxgi, "CreateDXGIFactory2")));
        }
        if (HMODULE d3d = GetModuleHandleW(L"d3d12.dll"))
            LogEntry("D3D12CreateDevice", reinterpret_cast<void*>(GetProcAddress(d3d, "D3D12CreateDevice")));
    }




    void LogSwapChainCreationPath()
    {
        __try { ReportCreationPath(); }
        __except (EXCEPTION_EXECUTE_HANDLER) { LOG("[creation] a read faulted part way through, so the lines above are all there is"); }
    }
