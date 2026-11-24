











    static void PublishPresentQueue(ID3D12CommandQueue* q, bool authoritative)
    {
        if (!q || !g_queueLockReady) return;
        EnterCriticalSection(&g_queueLock);
        if (authoritative || !g_presentQueuePinned)
        {
            if (q != g_presentQueue)
            {
                q->AddRef();
                if (g_presentQueue) g_presentQueue->Release();
                g_presentQueue = q;
            }
            if (authoritative && !g_presentQueuePinned)
            {
                g_presentQueuePinned = true;
                LOG("Present queue pinned from swapchain creation - owns the back buffers.");
            }
        }
        LeaveCriticalSection(&g_queueLock);
    }

    static void ObservePresentQueue(ID3D12CommandQueue* queue)
    {
        const std::unique_lock<std::recursive_mutex> render(g_renderMutex,std::try_to_lock);
        if (render && g_queueLockReady && g_device && queue)
        {
            EnterCriticalSection(&g_queueLock);
            if (!g_presentQueuePinned && queue != g_presentQueue &&
                queue->GetDesc().Type == D3D12_COMMAND_LIST_TYPE_DIRECT)
            {
                ID3D12Device* d = nullptr;
                if (SUCCEEDED(queue->GetDevice(IID_PPV_ARGS(&d))))
                {
                    if (d == g_device)
                    {


                        queue->AddRef();
                        if (g_presentQueue)
                            g_presentQueue->Release();
                        g_presentQueue = queue;

                        static bool s_logged = false;
                        if (!s_logged)
                        {
                            s_logged = true;
                            LOG("Present queue captured (heuristic - creation queue not yet seen).");
                        }
                    }
                    d->Release();
                }
            }
            LeaveCriticalSection(&g_queueLock);
        }

    }

    static void WINAPI hkExecuteCommandLists(
        ID3D12CommandQueue* queue, UINT numLists, ID3D12CommandList* const* lists)
    {
        ObservePresentQueue(queue);
        oExecuteCommandLists(queue, numLists, lists);
    }


    struct FrameStats
    {
        std::atomic<unsigned> presents{0}, notGameWindow{0}, notGameFacing{0}, busy{0},
            closed{0}, drawn{0}, notDrawn{0}, chainSwitches{0}, threadSwitches{0}, gpuWaits{0};
    };
    static FrameStats g_frameStats;



    static bool DrawOverlay(IDXGISwapChain3* swapChain)
    {



        if (!ReconcileSwapChain(swapChain) || !g_offscreenTex || !g_rtvHeap)
            return false;

        if (!EnsureCompositePipeline(g_scFormat))
            return false;





        ID3D12CommandQueue* submitQueue = nullptr;
        EnterCriticalSection(&g_queueLock);
        if (g_presentQueue)
        {
            submitQueue = g_presentQueue;
            submitQueue->AddRef();
        }
        LeaveCriticalSection(&g_queueLock);
        if (!submitQueue)
            return false;
        ID3D12Device* queueDevice = nullptr;
        const bool sameDevice = SUCCEEDED(submitQueue->GetDevice(IID_PPV_ARGS(&queueDevice))) &&
            queueDevice == g_device;
        if (queueDevice) queueDevice->Release();
        if (!sameDevice)
        {
            submitQueue->Release();
            return false;
        }







        gui::SetDisplayHeight(static_cast<float>(g_scHeight));



        State::Get().renderTid = GetCurrentThreadId();

        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();











        if (g_scWidth && g_scHeight)
        {
            ImGuiIO& io = ImGui::GetIO();
            const float tw = static_cast<float>(g_scWidth), th = static_cast<float>(g_scHeight);
            if (io.DisplaySize.x != tw || io.DisplaySize.y != th)
            {
                static UINT s_saidW = 0, s_saidH = 0;
                if (s_saidW != g_scWidth || s_saidH != g_scHeight)
                {
                    s_saidW = g_scWidth; s_saidH = g_scHeight;
                    LOG("[overlay] the window is %.0fx%.0f and the target this mod draws into is %ux%u, so the menu is built at the target's size and stretched with the rest of the frame.",
                        io.DisplaySize.x, io.DisplaySize.y, g_scWidth, g_scHeight);
                }
                io.DisplaySize = ImVec2(tw, th);
            }
            input::SetOverlaySize(tw, th);
        }
        gui::FeedInput();
        ImGui::NewFrame();

        gui::Render();

        ImGui::Render();

        const UINT idx = swapChain->GetCurrentBackBufferIndex();
        if (idx >= g_frames.size() || !g_frames[idx].renderTarget)
        {



            submitQueue->Release();
            return false;
        }
        FrameContext& frame = g_frames[idx];









        auto completed = g_fence->GetCompletedValue();
        if (completed != UINT64_MAX && completed < frame.fenceValue &&
            SUCCEEDED(g_fence->SetEventOnCompletion(frame.fenceValue, g_frameFenceEvent)))
        {
            g_frameStats.gpuWaits.fetch_add(1, std::memory_order_relaxed);
            WaitForSingleObject(g_frameFenceEvent, 100);
            completed = g_fence->GetCompletedValue();
        }
        if (completed < frame.fenceValue || completed == UINT64_MAX ||
            FAILED(frame.commandAllocator->Reset()) ||
            FAILED(g_commandList->Reset(frame.commandAllocator, nullptr)))
        {
            submitQueue->Release();
            return false;
        }


        D3D12_RESOURCE_BARRIER offscreenBarrier = {};
        offscreenBarrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        offscreenBarrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        offscreenBarrier.Transition.pResource   = g_offscreenTex;
        offscreenBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        offscreenBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        offscreenBarrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
        g_commandList->ResourceBarrier(1, &offscreenBarrier);

        const float transparent[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        g_commandList->OMSetRenderTargets(1, &g_offscreenRtv, FALSE, nullptr);
        g_commandList->ClearRenderTargetView(g_offscreenRtv, transparent, 0, nullptr);
        g_commandList->SetDescriptorHeaps(1, &g_srvHeap);


        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), g_commandList);

        offscreenBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        offscreenBarrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        g_commandList->ResourceBarrier(1, &offscreenBarrier);



        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource   = frame.renderTarget;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
        g_commandList->ResourceBarrier(1, &barrier);
        g_commandList->OMSetRenderTargets(1, &frame.rtvHandle, FALSE, nullptr);

        const D3D12_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<float>(g_scWidth), static_cast<float>(g_scHeight), 0.0f, 1.0f };
        const D3D12_RECT     scissor  = { 0, 0, static_cast<LONG>(g_scWidth), static_cast<LONG>(g_scHeight) };
        g_commandList->RSSetViewports(1, &viewport);
        g_commandList->RSSetScissorRects(1, &scissor);

        g_commandList->SetGraphicsRootSignature(g_compositeRootSig);
        g_commandList->SetPipelineState(g_compositePSO);
        g_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        const struct { UINT mode; float paperWhiteNits; float pad[2]; } params =
            { CompositeModeForColorSpace(g_colorSpace), kPaperWhiteNits, { 0.0f, 0.0f } };
        g_commandList->SetGraphicsRoot32BitConstants(0, 4, &params, 0);

        D3D12_GPU_DESCRIPTOR_HANDLE offscreenSrvGpu = g_srvHeap->GetGPUDescriptorHandleForHeapStart();
        offscreenSrvGpu.ptr += static_cast<UINT64>(kOffscreenSrvSlot) *
            g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        g_commandList->SetGraphicsRootDescriptorTable(1, offscreenSrvGpu);

        g_commandList->DrawInstanced(3, 1, 0, 0);

        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
        g_commandList->ResourceBarrier(1, &barrier);

        if (FAILED(g_commandList->Close()))
        {
            submitQueue->Release();
            return false;
        }

        ID3D12CommandList* toExec[] = { g_commandList };
        g_unfencedSubmit = true;
        submitQueue->ExecuteCommandLists(1, toExec);
        const auto signal = submitQueue->Signal(g_fence, g_fenceValue + 1);
        submitQueue->Release();
        if (FAILED(signal))
        {

            LOG_ERR("Overlay fence signal failed (0x%08X) - overlay disabled.", signal);
            g_renderDisabled = true;
            State::Get().lastPresentAt.store(0);
            State::Get().lastDrawAt.store(0);
            gui::OverlayInterrupted();
            return false;
        }
        frame.fenceValue = ++g_fenceValue;
        g_unfencedSubmit = false;
        return true;
    }













    static void CountNotGameWindow() { g_frameStats.notGameWindow.fetch_add(1, std::memory_order_relaxed); }

    static void FrameReport(bool open)
    {
        static bool s_wasOpen = false;
        static DWORD s_until = 0, s_last = 0;
        static unsigned s_lines = 0;
        const DWORD now = GetTickCount();
        auto& f = g_frameStats;
        const auto take = [](std::atomic<unsigned>& n) { return n.exchange(0, std::memory_order_relaxed); };
        if (open && !s_wasOpen)
        {
            if (!s_until)
            {


                for (auto* n : { &f.presents, &f.notGameWindow, &f.notGameFacing, &f.busy, &f.closed,
                                 &f.drawn, &f.notDrawn, &f.chainSwitches, &f.threadSwitches, &f.gpuWaits })
                    take(*n);
                s_last = now;
            }
            s_until = now + 20000;
        }
        s_wasOpen = open;
        if (!s_until || static_cast<LONG>(now - s_last) < 1000)
            return;
        const unsigned presents = take(f.presents), notGameWindow = take(f.notGameWindow),
                       notGameFacing = take(f.notGameFacing), busy = take(f.busy),
                       closed = take(f.closed), drawn = take(f.drawn), notDrawn = take(f.notDrawn),
                       chains = take(f.chainSwitches), threads = take(f.threadSwitches),
                       waits = take(f.gpuWaits);
        if (s_lines < 120)
        {
            ++s_lines;
            LOG("[frames] %.1f s: %u presents; menu drawn on %u, draw failed on %u, menu closed on %u, "
                "skipped: %u another window, %u not the game's frames, %u still drawing the last one; "
                "%u swapchain switches, %u thread switches; waited for the GPU on %u.%s",
                (now - s_last) / 1000.0, presents, drawn, notDrawn, closed, notGameWindow, notGameFacing, busy,
                chains, threads, waits, s_lines == 120 ? " No more of these lines this session." : "");
        }
        s_last = now;
        if (static_cast<LONG>(now - s_until) >= 0)
            s_until = 0;
    }





    static bool PrepareOverlayQueue(IDXGISwapChain3* swapChain, reshade_api::CommandQueue* effectsQueue,
                                    bool querySwapchainQueue)
    {
        if (effectsQueue)
        {
            auto* queue = reinterpret_cast<ID3D12CommandQueue*>(effectsQueue->get_native());
            if (!queue) return false;
            effectsQueue->flush_immediate_command_list();
            PublishPresentQueue(queue, true);
        }
        else if (querySwapchainQueue)
        {
            Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue;
            if (SUCCEEDED(swapChain->GetDevice(IID_PPV_ARGS(queue.GetAddressOf()))))
                PublishPresentQueue(queue.Get(), true);
        }
        return true;
    }

    static bool RenderOverlayFrame(IDXGISwapChain3* swapChain, bool isGameFacing, UINT flags,
                                   reshade_api::CommandQueue* effectsQueue, bool querySwapchainQueue)
    {






        auto& f = g_frameStats;
        f.presents.fetch_add(1, std::memory_order_relaxed);
        static IDXGISwapChain3* s_chain = nullptr;
        static DWORD s_thread = 0;
        const DWORD thread = GetCurrentThreadId();
        if (s_chain && s_chain != swapChain)
            f.chainSwitches.fetch_add(1, std::memory_order_relaxed);
        if (s_thread && s_thread != thread)
            f.threadSwitches.fetch_add(1, std::memory_order_relaxed);
        s_chain = swapChain;
        s_thread = thread;

        if (!isGameFacing || (flags & DXGI_PRESENT_TEST))
        {
            g_frameStats.notGameFacing.fetch_add(1, std::memory_order_relaxed);
            FrameReport(gui::WantsDraw());
            return false;
        }

        if (!g_renderDisabled)
            State::Get().lastPresentAt.store(GetTickCount64());




        gui::PollToggle();
        const bool open = gui::WantsDraw();
        FrameReport(open);
        if (!open)
        {
            g_frameStats.closed.fetch_add(1, std::memory_order_relaxed);
            return false;
        }

        bool drew = false;
        __try
        {
            if (g_renderDisabled && GetTickCount64() < g_overlayInitRetryAt)
                return false;
            if (g_renderDisabled || (g_device && swapChain != g_swapChain))
            {
                DXGI_SWAP_CHAIN_DESC desc{};
                if (FAILED(swapChain->GetDesc(&desc)) || !desc.OutputWindow) return false;
                ID3D12Device* device = nullptr;
                if (FAILED(swapChain->GetDevice(IID_PPV_ARGS(&device))) || !device) return false;
                const bool changed = device != g_device || desc.OutputWindow != g_hwnd;
                device->Release();
                if (changed || g_renderDisabled)
                {
                    const bool removed = g_device && g_device->GetDeviceRemovedReason() != S_OK;
                    if (removed && !changed) return false;
                    if (!removed && (g_unfencedSubmit || (g_fence && g_fence->GetCompletedValue() < g_fenceValue)))
                        return false;
                    ResetOverlayInitialization();
                    g_renderDisabled = false;
                    g_unfencedSubmit = false;
                    g_overlayInitRetryAt = 0;
                    LOG("[overlay] Rebuilding Hex Appeal's renderer after a presentation change or interruption.");
                }
            }
            if (g_renderDisabled) return false;
            if (!g_imguiReady)
            {
                if (GetTickCount64() < g_overlayInitRetryAt)
                    return false;
                DXGI_SWAP_CHAIN_DESC desc = {};
                if (FAILED(swapChain->GetDesc(&desc)) || !desc.OutputWindow ||
                    !desc.BufferCount ||
                    GetAncestor(desc.OutputWindow, GA_ROOT) != GetForegroundWindow())
                    return false;

                LOG("[overlay] Menu requested; initialising the renderer.");
                if (!InitImGui(swapChain))
                {
                    ResetOverlayInitialization();
                    g_overlayInitRetryAt = GetTickCount64() + 2000;
                    LOG_ERR("Hex Appeal overlay could not start. Retrying on a later frame.");
                    return false;
                }
                g_imguiReady = true;
                g_gameHwnd.store(desc.OutputWindow);
            }

            hooks::EnsureXInputHooks();
            if (!PrepareOverlayQueue(swapChain, effectsQueue, querySwapchainQueue))
                return false;
            drew = DrawOverlay(swapChain);
            (drew ? g_frameStats.drawn : g_frameStats.notDrawn).fetch_add(1, std::memory_order_relaxed);



            if (drew)
            {
                if (State::Get().lastDrawAt.exchange(GetTickCount()) == 0)
                    LOG("[overlay] First Hex Appeal frame submitted.");
                gui::NoteNativeWardrobeDrawn();
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            LOG_ERR("Overlay crashed (exception 0x%08X) - overlay disabled, game continues.",
                    GetExceptionCode());
            g_renderDisabled = true;
            g_overlayInitRetryAt = GetTickCount64() + 2000;
            State::Get().lastPresentAt.store(0);
            State::Get().lastDrawAt.store(0);
            gui::OverlayInterrupted();
        }
        return drew;
    }

    static bool RenderOverlay(IDXGISwapChain3* swapChain, bool isGameFacing, UINT flags,
                              reshade_api::CommandQueue* effectsQueue = nullptr, bool querySwapchainQueue = false)
    {


        const std::unique_lock<std::recursive_mutex> render(g_renderMutex,std::try_to_lock);
        static std::atomic_flag busy = ATOMIC_FLAG_INIT;
        if (!render || busy.test_and_set(std::memory_order_acquire))
        {
            g_frameStats.busy.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        struct UnlockOnReturn
        {
            std::atomic_flag& flag;
            ~UnlockOnReturn() { flag.clear(std::memory_order_release); }
        } unlock{busy};
        return RenderOverlayFrame(swapChain, isGameFacing, flags, effectsQueue, querySwapchainQueue);
    }
