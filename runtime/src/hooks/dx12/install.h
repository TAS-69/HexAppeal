















    struct HookSlot
    {
        const char* name;
        void*       addr;
        void*       detour;
        void**      orig;
        bool        mayStack;
        bool        draws;
        bool        installed;
    };
    static HookSlot g_targets[6] = {};
    static std::mutex g_targetMutex;




    static void InstallPass(bool stack)
    {
        int added = 0, skipped = 0;
        for (HookSlot& t : g_targets)
        {
            if (t.installed || !t.addr) { if (!t.installed) ++skipped; continue; }
            if (!LogHookTarget(t.name, t.addr, stack && t.mayStack)) { ++skipped; continue; }
            if (MH_CreateHook(t.addr, t.detour, t.orig) != MH_OK) { LOG_ERR("MH_CreateHook failed for %s.", t.name); ++skipped; continue; }
            if (MH_EnableHook(t.addr) != MH_OK)
            {
                MH_RemoveHook(t.addr);
                LOG_ERR("MH_EnableHook failed for %s.", t.name);
                ++skipped;
                continue;
            }
            t.installed = true;
            ++added;
        }
        LOG("Hex Appeal installed %d DirectX detours and left %d entries alone.", added, skipped);
    }





    static bool InstallDetoursFrom(IDXGISwapChain1* chain, IUnknown* queueUnk, bool stack)
    {
        if (!chain) return false;
        const std::lock_guard<std::mutex> lock(g_targetMutex);

        void** scVt = *reinterpret_cast<void***>(chain);
        void* presentAddr  = scVt[8];
        void* present1Addr = scVt[22];
        void* resizeAddr   = scVt[13];
        void* colorSpaceAddr = nullptr;
        void* resize1Addr    = nullptr;
        IDXGISwapChain3* sc3 = nullptr;
        if (SUCCEEDED(chain->QueryInterface(IID_PPV_ARGS(&sc3))) && sc3)
        {
            colorSpaceAddr = (*reinterpret_cast<void***>(sc3))[38];
            resize1Addr    = (*reinterpret_cast<void***>(sc3))[39];
            sc3->Release();
        }
        void* execAddr = nullptr;
        ID3D12CommandQueue* q = nullptr;
        if (queueUnk && SUCCEEDED(queueUnk->QueryInterface(IID_PPV_ARGS(&q))) && q)
        {
            execAddr = (*reinterpret_cast<void***>(q))[10];
            q->Release();
        }




        const HookSlot targets[6] = {
            { "Present",             presentAddr,    reinterpret_cast<void*>(&hkPresent),             reinterpret_cast<void**>(&oPresent),             true,  true,  false },
            { "Present1",            present1Addr,   reinterpret_cast<void*>(&hkPresent1),            reinterpret_cast<void**>(&oPresent1),            true,  true,  false },
            { "ResizeBuffers",       resizeAddr,     reinterpret_cast<void*>(&hkResizeBuffers),       reinterpret_cast<void**>(&oResizeBuffers),       true,  false, false },
            { "ResizeBuffers1",      resize1Addr,    reinterpret_cast<void*>(&hkResizeBuffers1),      reinterpret_cast<void**>(&oResizeBuffers1),      true,  false, false },
            { "ExecuteCommandLists", execAddr,       reinterpret_cast<void*>(&hkExecuteCommandLists), reinterpret_cast<void**>(&oExecuteCommandLists), false, false, false },
            { "SetColorSpace1",      colorSpaceAddr, reinterpret_cast<void*>(&hkSetColorSpace1),      reinterpret_cast<void**>(&oSetColorSpace1),      true,  false, false },
        };
        for (int i = 0; i < 6; ++i)
            if (!g_targets[i].installed) g_targets[i] = targets[i];




        InstallPass(stack);
        for (int i = 0; i < 6; ++i)
            if (g_targets[i].installed && targets[i].draws && g_targets[i].addr == targets[i].addr)
                return true;
        return false;
    }

    static void PrepareGameSwapChain(HWND hwnd, UINT width, UINT height)
    {
        const std::lock_guard<std::recursive_mutex> lock(g_renderMutex);
        if (!g_wrappedChains.load() && g_imguiReady && g_swapChain &&
            !IsProbeChain(width, height) && (!g_gameHwnd || hwnd == g_gameHwnd))
        {
            LOG("Releasing Hex Appeal's references before the game replaces its swapchain.");
            WaitForOverlayIdle();
            CleanupRenderTargets();
            ReleaseOffscreenTarget();
            g_swapChain = nullptr;
        }
    }

    static bool AttachGameSwapChain(IUnknown* device, HWND hwnd, IDXGISwapChain1** ppSwapChain)
    {
        const auto gameWindow = g_gameHwnd.load();
        if (!ppSwapChain || !*ppSwapChain || !hwnd ||
            (gameWindow && hwnd != gameWindow && IsWindow(gameWindow) && !IsGameWindow(hwnd)))
            return false;
        DWORD pid = 0;
        if (!GetWindowThreadProcessId(hwnd, &pid) || pid != GetCurrentProcessId())
            return false;
        DXGI_SWAP_CHAIN_DESC1 desc = {};
        if (FAILED((*ppSwapChain)->GetDesc1(&desc)) || IsProbeChain(desc.Width, desc.Height))
            return false;
        g_gameHwnd = hwnd;
        g_gameSwapChainSeen = true;
        ::ml::Log::MarkStartupComplete();

        bool s_stack = false;
        {
            void** vt = *reinterpret_cast<void***>(*ppSwapChain);
            char owner[64] = "";
            if (DetourOwner(vt[8], owner, sizeof owner) && !BenignPresentOwner(owner))
            {




                if (Settings::Get().wrapSwapChain == 1 && !KeyingOverlayLoaded())
                    LOG("[hook] %s detoured Present before this mod, which would normally mean stacking instead of wrapping. "
                        "WrapSwapChain=1 asks for the wrapper, so it is used anyway. If %s stops working, set WrapSwapChain back to auto.",
                        owner, owner);
                else
                {
                    s_stack = true;
                    LOG("[hook] %s detoured Present before this mod and would be handed the wrapper: not wrapping; stacking this mod's detours on top of its instead%s",
                        owner, Settings::Get().wrapSwapChain == 1 ? "" : ". WrapSwapChain=1 would wrap anyway");
                }
            }
            else if (const char* keying = KeyingOverlayLoaded())
            {
                s_stack = true;
                LOG("[hook] %s is loaded and keys its work on the swapchain object it is handed: not wrapping; stacking this mod's detours instead", keying);
            }


            if (s_stack)
                LOG("[hook] so WrapSwapChain does not apply: the menu draws on the finished frames%s.",
                    GetModuleHandleA("sl.interposer.dll")
                        ? "; the Streamline library is loaded"
                        : "; the Streamline library is not loaded");
        }
        const bool detoured = InstallDetoursFrom(*ppSwapChain, device, s_stack);















        const auto baseDevice = StreamlineBaseInterface(device);
        auto* queueSource = baseDevice ? baseDevice.Get() : device;
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> presentQueue;
        if (queueSource && SUCCEEDED(queueSource->QueryInterface(IID_PPV_ARGS(presentQueue.GetAddressOf()))) && presentQueue)
            PublishPresentQueue(presentQueue.Get(), true);
        {
            Microsoft::WRL::ComPtr<ID3D12Device> chainDevice, queueDevice;
            (*ppSwapChain)->GetDevice(IID_PPV_ARGS(chainDevice.GetAddressOf()));
            if (presentQueue) presentQueue->GetDevice(IID_PPV_ARGS(queueDevice.GetAddressOf()));
            LOG("[hook] Game swapchain device %p; present queue %p (%s) on device %p.",
                static_cast<void*>(chainDevice.Get()), static_cast<void*>(presentQueue.Get()),
                baseDevice ? "Streamline's native queue" : "as created", static_cast<void*>(queueDevice.Get()));
        }
        if ((!s_stack && WrapWanted()) || !detoured)
        {
            auto* original = *ppSwapChain;
            WrapSwapChain(ppSwapChain, hwnd);
            if (*ppSwapChain == original && !detoured)
                LOG_ERR("Hex Appeal could not attach to this DirectX presentation entry or wrap the game's swapchain.");
        }
        return true;
    }

    static bool AttachLegacyGameSwapChain(IUnknown* device, HWND window, IDXGISwapChain** chain)
    {
        IDXGISwapChain1* swapChain = nullptr;
        if (FAILED((*chain)->QueryInterface(IID_PPV_ARGS(&swapChain))) || !swapChain) return false;
        auto* original = swapChain;
        const bool observed = AttachGameSwapChain(device, window, &swapChain);
        if (swapChain != original)
        {
            (*chain)->Release();
            *chain = swapChain;
        }
        else
        {
            swapChain->Release();
        }
        return observed;
    }
