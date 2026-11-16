












    static void DumpDred()
    {
        if (!g_device) return;

        ID3D12DeviceRemovedExtendedData* dred = nullptr;
        if (FAILED(g_device->QueryInterface(IID_PPV_ARGS(&dred))) || !dred)
        {
            LOG_ERR("DRED: interface unavailable on this device - fault cause not captured.");
            return;
        }

        D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT bc = {};
        if (SUCCEEDED(dred->GetAutoBreadcrumbsOutput(&bc)) && bc.pHeadAutoBreadcrumbNode)
        {
            int n = 0;
            for (const D3D12_AUTO_BREADCRUMB_NODE* node = bc.pHeadAutoBreadcrumbNode;
                 node && n < 8; node = node->pNext, ++n)
            {
                const UINT last = node->pLastBreadcrumbValue ? *node->pLastBreadcrumbValue : 0;
                LOG_ERR("DRED breadcrumb[%d]: queue='%s' list='%s' completed %u/%u ops",
                        n,
                        node->pCommandQueueDebugNameA ? node->pCommandQueueDebugNameA : "?",
                        node->pCommandListDebugNameA  ? node->pCommandListDebugNameA  : "?",
                        last, node->BreadcrumbCount);
            }
        }
        else
        {



            LOG_ERR("DRED: no GPU breadcrumbs - removal was not a GPU command fault.");
        }

        D3D12_DRED_PAGE_FAULT_OUTPUT pf = {};
        if (SUCCEEDED(dred->GetPageFaultAllocationOutput(&pf)) && pf.PageFaultVA)
        {
            LOG_ERR("DRED page fault VA=0x%llx", static_cast<unsigned long long>(pf.PageFaultVA));
            for (const D3D12_DRED_ALLOCATION_NODE* a = pf.pHeadExistingAllocationNode; a; a = a->pNext)
                LOG_ERR("  live alloc at fault: '%s'", a->ObjectNameA ? a->ObjectNameA : "?");
            for (const D3D12_DRED_ALLOCATION_NODE* a = pf.pHeadRecentFreedAllocationNode; a; a = a->pNext)
                LOG_ERR("  recently-freed alloc: '%s'", a->ObjectNameA ? a->ObjectNameA : "?");
        }
        else
        {
            LOG_ERR("DRED: no page fault recorded - not a bad-address access.");
        }

        dred->Release();
    }



    static void PostPresentDeviceCheck(bool drew)
    {
        if (drew && g_device)
        {
            const HRESULT removed = g_device->GetDeviceRemovedReason();
            if (removed != S_OK)
            {
                LOG_ERR("Device removed (0x%08X) after overlay submit - overlay disabled.", removed);
                DumpDred();
                g_renderDisabled = true;
            }
        }
    }



    static void PreResizeCleanup()
    {
        if (!g_imguiReady) return;
        WaitForOverlayIdle();
        CleanupRenderTargets();
    }






    static void PostResizeRebuild(IDXGISwapChain3*)
    {
        if (!g_imguiReady) return;
        g_swapChain = nullptr;
    }



    static bool IsGameWindow(HWND window)
    {
        return window && (window == g_hwnd ||
                          GetAncestor(window, GA_ROOT) == GetForegroundWindow());
    }







    static void OnReShadePresent(reshade_api::DeviceObject *,
                                 reshade_api::Swapchain *swapChain,
                                 const RECT *, const RECT *, uint32_t, const RECT *)
    {
        if (!swapChain)
            return;
        const auto *device = swapChain->get_device();
        if (!device || device->get_api() != reshade_api::d3d12)
            return;
        if (!IsGameWindow(static_cast<HWND>(swapChain->get_hwnd())))
            return;
        switch (swapChain->get_color_space())
        {
            case reshade_api::colorSpaceSrgb:
                OnColorSpaceChanged(DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709);
                break;
            case reshade_api::colorSpaceScrgb:
                OnColorSpaceChanged(DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709);
                break;
            case reshade_api::colorSpaceHdr10Pq:
                OnColorSpaceChanged(DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020);
                break;
            default:

                break;
        }
    }




    static void OnReShadeEffectsPresent(reshade_api::EffectRuntime *runtime)
    {
        if (!runtime)
            return;
        const auto *device = runtime->get_device();
        if (!device || device->get_api() != reshade_api::d3d12)
            return;
        reshade_api::CommandQueue *queue = runtime->get_command_queue();
        if (!queue)
            return;
        auto *nativeQueue = reinterpret_cast<ID3D12CommandQueue *>(queue->get_native());
        auto *nativeSwapChain = reinterpret_cast<IDXGISwapChain *>(runtime->get_native());
        IDXGISwapChain3 *chain = nullptr;
        if (!nativeQueue || !nativeSwapChain ||
            FAILED(nativeSwapChain->QueryInterface(IID_PPV_ARGS(&chain))))
            return;

        DXGI_SWAP_CHAIN_DESC desc = {};
        if (SUCCEEDED(chain->GetDesc(&desc)) && IsGameWindow(desc.OutputWindow))
        {




            queue->flush_immediate_command_list();
            const bool drew = RenderOverlay(chain, true, 0, nativeQueue);
            PostPresentDeviceCheck(drew);
        }
        else
        {
            CountNotGameWindow();
        }
        chain->Release();
    }

    static void OnReShadeDestroySwapChain(reshade_api::DeviceObject *swapChain, bool resize)
    {
        if (!g_imguiReady || !g_swapChain || !swapChain)
            return;
        const auto *device = swapChain->get_device();
        if (!device || device->get_api() != reshade_api::d3d12)
            return;
        auto *native = reinterpret_cast<IDXGISwapChain *>(swapChain->get_native());
        IDXGISwapChain3 *chain = nullptr;
        if (!native || FAILED(native->QueryInterface(IID_PPV_ARGS(&chain))))
            return;
        const bool ours = chain == g_swapChain;
        chain->Release();
        if (!ours)
            return;

        PreResizeCleanup();
        g_swapChain = nullptr;
        if (!resize)
        {
            EnterCriticalSection(&g_queueLock);
            if (g_presentQueue)
                g_presentQueue->Release();
            g_presentQueue = nullptr;
            g_presentQueuePinned = false;
            LeaveCriticalSection(&g_queueLock);
        }
    }












    static bool IsProbeChain(UINT w, UINT h);
    static bool IsGameChain(IDXGISwapChain* swapChain)
    {
        if (!swapChain) return false;
        if (g_swapChain && static_cast<IDXGISwapChain*>(g_swapChain) == swapChain) return true;
        DXGI_SWAP_CHAIN_DESC desc = {};
        if (FAILED(swapChain->GetDesc(&desc)) || !desc.OutputWindow) return false;
        if (IsProbeChain(desc.BufferDesc.Width, desc.BufferDesc.Height)) return false;
        return g_gameHwnd ? desc.OutputWindow == g_gameHwnd : IsGameWindow(desc.OutputWindow);
    }

    static HRESULT WINAPI hkPresent(IDXGISwapChain3* swapChain, UINT syncInterval, UINT flags)
    {
        const bool drew = RenderOverlay(swapChain, !g_wrapperActive && IsGameChain(swapChain), flags);
        const HRESULT hr = oPresent(swapChain, syncInterval, flags);
        PostPresentDeviceCheck(drew);
        return hr;
    }

    typedef HRESULT (WINAPI* Present1_t)(IDXGISwapChain1*, UINT, UINT, const DXGI_PRESENT_PARAMETERS*);
    static Present1_t oPresent1 = nullptr;
    static HRESULT WINAPI hkPresent1(IDXGISwapChain1* swapChain, UINT syncInterval, UINT flags, const DXGI_PRESENT_PARAMETERS* pp)
    {


        const bool drew = RenderOverlay(static_cast<IDXGISwapChain3*>(swapChain),
                                        !g_wrapperActive && IsGameChain(swapChain), flags);
        const HRESULT hr = oPresent1(swapChain, syncInterval, flags, pp);
        PostPresentDeviceCheck(drew);
        return hr;
    }
    static HRESULT WINAPI hkResizeBuffers(
        IDXGISwapChain3* swapChain, UINT bufferCount,
        UINT width, UINT height, DXGI_FORMAT format, UINT flags)
    {
        if (!g_imguiReady || g_wrapperActive)
            return oResizeBuffers(swapChain, bufferCount, width, height, format, flags);
        PreResizeCleanup();
        const HRESULT hr = oResizeBuffers(swapChain, bufferCount, width, height, format, flags);
        PostResizeRebuild(swapChain);
        LOG("[hook] ResizeBuffers through the stacked detour: %ux%u, %u buffers, fmt %d, hr 0x%08X; views released before and rebuilt after",
            width, height, bufferCount, static_cast<int>(format), static_cast<unsigned>(hr));
        return hr;
    }





    typedef HRESULT (WINAPI* ResizeBuffers1_t)(IDXGISwapChain3*, UINT, UINT, UINT, DXGI_FORMAT, UINT, const UINT*, IUnknown* const*);
    static ResizeBuffers1_t oResizeBuffers1 = nullptr;
    static HRESULT WINAPI hkResizeBuffers1(
        IDXGISwapChain3* swapChain, UINT bufferCount,
        UINT width, UINT height, DXGI_FORMAT format, UINT flags,
        const UINT* nodeMask, IUnknown* const* queues)
    {
        if (!g_imguiReady || g_wrapperActive)
            return oResizeBuffers1(swapChain, bufferCount, width, height, format, flags, nodeMask, queues);
        PreResizeCleanup();
        const HRESULT hr = oResizeBuffers1(swapChain, bufferCount, width, height, format, flags, nodeMask, queues);
        PostResizeRebuild(swapChain);
        LOG("[hook] ResizeBuffers1 through the stacked detour: %ux%u, %u buffers, fmt %d, hr 0x%08X; views released before and rebuilt after",
            width, height, bufferCount, static_cast<int>(format), static_cast<unsigned>(hr));
        return hr;
    }



    static HRESULT WINAPI hkSetColorSpace1(IDXGISwapChain3* swapChain, DXGI_COLOR_SPACE_TYPE colorSpace)
    {
        const HRESULT hr = oSetColorSpace1(swapChain, colorSpace);
        if (SUCCEEDED(hr)) OnColorSpaceChanged(colorSpace);
        return hr;
    }
