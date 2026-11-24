






    static void CleanupRenderTargets()
    {
        for (auto& f : g_frames)
        {
            if (f.renderTarget) { f.renderTarget->Release(); f.renderTarget = nullptr; }
        }
    }

    static bool CreateRenderTargets(IDXGISwapChain3* swapChain)
    {


        if (!g_device || !g_rtvHeap || g_frames.size() < g_bufferCount)
            return false;
        const UINT rtvSize = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        D3D12_CPU_DESCRIPTOR_HANDLE handle = g_rtvHeap->GetCPUDescriptorHandleForHeapStart();

        for (UINT i = 0; i < g_bufferCount; ++i)
        {
            ID3D12Resource* buffer = nullptr;
            if (FAILED(swapChain->GetBuffer(i, IID_PPV_ARGS(&buffer))))
                return false;

            g_device->CreateRenderTargetView(buffer, nullptr, handle);
            g_frames[i].renderTarget = buffer;
            g_frames[i].rtvHandle    = handle;
            handle.ptr += rtvSize;
        }
        return true;
    }

    static void ReleaseOffscreenTarget()
    {
        if (g_offscreenTex)     { g_offscreenTex->Release();     g_offscreenTex     = nullptr; }
        if (g_offscreenRtvHeap) { g_offscreenRtvHeap->Release(); g_offscreenRtvHeap = nullptr; }
        g_offscreenW = 0;
        g_offscreenH = 0;
    }




    static bool CreateOffscreenTarget(UINT width, UINT height)
    {
        if (g_offscreenTex && width == g_offscreenW && height == g_offscreenH)
            return true;

        ReleaseOffscreenTarget();
        if (width == 0 || height == 0)
            return false;

        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width            = width;
        desc.Height           = height;
        desc.DepthOrArraySize = 1;
        desc.MipLevels        = 1;
        desc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Flags            = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        D3D12_CLEAR_VALUE clear = {};
        clear.Format = desc.Format;



        if (FAILED(g_device->CreateCommittedResource(
                &heapProps, D3D12_HEAP_FLAG_NONE, &desc,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clear,
                IID_PPV_ARGS(&g_offscreenTex))))
            return false;
        g_offscreenTex->SetName(L"ModActivatorOverlayOffscreen");

        D3D12_DESCRIPTOR_HEAP_DESC rtvDesc = {};
        rtvDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvDesc.NumDescriptors = 1;
        if (FAILED(g_device->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&g_offscreenRtvHeap))))
        {
            ReleaseOffscreenTarget();
            return false;
        }
        g_offscreenRtv = g_offscreenRtvHeap->GetCPUDescriptorHandleForHeapStart();
        g_device->CreateRenderTargetView(g_offscreenTex, nullptr, g_offscreenRtv);

        const UINT inc = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        D3D12_CPU_DESCRIPTOR_HANDLE srv = g_srvHeap->GetCPUDescriptorHandleForHeapStart();
        srv.ptr += static_cast<SIZE_T>(kOffscreenSrvSlot) * inc;
        g_device->CreateShaderResourceView(g_offscreenTex, nullptr, srv);

        g_offscreenW = width;
        g_offscreenH = height;
        return true;
    }






    static bool CreateCompositePipeline(DXGI_FORMAT rtvFormat)
    {
        if (!g_compositeRootSig)
        {
            D3D12_DESCRIPTOR_RANGE srvRange = {};
            srvRange.RangeType                         = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
            srvRange.NumDescriptors                     = 1;
            srvRange.BaseShaderRegister                 = 0;
            srvRange.OffsetInDescriptorsFromTableStart  = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

            D3D12_ROOT_PARAMETER params[2] = {};
            params[0].ParameterType            = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
            params[0].Constants.ShaderRegister = 0;
            params[0].Constants.Num32BitValues  = 4;
            params[0].ShaderVisibility          = D3D12_SHADER_VISIBILITY_PIXEL;

            params[1].ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            params[1].DescriptorTable.NumDescriptorRanges  = 1;
            params[1].DescriptorTable.pDescriptorRanges    = &srvRange;
            params[1].ShaderVisibility                     = D3D12_SHADER_VISIBILITY_PIXEL;

            D3D12_STATIC_SAMPLER_DESC sampler = {};
            sampler.Filter           = D3D12_FILTER_MIN_MAG_MIP_POINT;
            sampler.AddressU         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            sampler.AddressV         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            sampler.AddressW         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            sampler.ComparisonFunc   = D3D12_COMPARISON_FUNC_ALWAYS;
            sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

            D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
            rsDesc.NumParameters     = 2;
            rsDesc.pParameters       = params;
            rsDesc.NumStaticSamplers = 1;
            rsDesc.pStaticSamplers   = &sampler;

            ID3DBlob* sig = nullptr;
            ID3DBlob* err = nullptr;
            const HRESULT serialized = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err);
            if (err) err->Release();
            if (FAILED(serialized))
                return false;

            const HRESULT created = g_device->CreateRootSignature(
                0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&g_compositeRootSig));
            sig->Release();
            if (FAILED(created))
                return false;
        }

        if (g_compositePSO) { g_compositePSO->Release(); g_compositePSO = nullptr; }

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
        pso.pRootSignature = g_compositeRootSig;
        pso.VS = { g_hdrCompositeVS, sizeof(g_hdrCompositeVS) };
        pso.PS = { g_hdrCompositePS, sizeof(g_hdrCompositePS) };



        pso.BlendState.RenderTarget[0].BlendEnable    = TRUE;
        pso.BlendState.RenderTarget[0].SrcBlend       = D3D12_BLEND_SRC_ALPHA;
        pso.BlendState.RenderTarget[0].DestBlend      = D3D12_BLEND_INV_SRC_ALPHA;
        pso.BlendState.RenderTarget[0].BlendOp        = D3D12_BLEND_OP_ADD;
        pso.BlendState.RenderTarget[0].SrcBlendAlpha  = D3D12_BLEND_ONE;
        pso.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
        pso.BlendState.RenderTarget[0].BlendOpAlpha   = D3D12_BLEND_OP_ADD;
        pso.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        pso.SampleMask                      = 0xFFFFFFFFu;
        pso.RasterizerState.FillMode        = D3D12_FILL_MODE_SOLID;
        pso.RasterizerState.CullMode        = D3D12_CULL_MODE_NONE;
        pso.RasterizerState.DepthClipEnable = TRUE;
        pso.DepthStencilState.DepthEnable   = FALSE;
        pso.DepthStencilState.StencilEnable = FALSE;
        pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pso.NumRenderTargets      = 1;
        pso.RTVFormats[0]         = rtvFormat;
        pso.SampleDesc.Count      = 1;

        return SUCCEEDED(g_device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&g_compositePSO)));
    }

    static bool EnsureCompositePipeline(DXGI_FORMAT rtvFormat)
    {
        if (g_compositePSO && rtvFormat == g_compositeFormat)
            return true;
        if (!CreateCompositePipeline(rtvFormat))
            return false;
        g_compositeFormat = rtvFormat;
        return true;
    }



    static void WaitForOverlayIdle()
    {
        if (g_fence && g_fenceValue && g_fenceEvent &&
            g_fence->GetCompletedValue() < g_fenceValue)
        {
            g_fence->SetEventOnCompletion(g_fenceValue, g_fenceEvent);
            WaitForSingleObject(g_fenceEvent, 1000);
        }
    }




    static bool ResizeFrameResources(UINT newCount)
    {
        CleanupRenderTargets();
        for (auto& f : g_frames)
            if (f.commandAllocator) { f.commandAllocator->Release(); f.commandAllocator = nullptr; }
        if (g_rtvHeap) { g_rtvHeap->Release(); g_rtvHeap = nullptr; }



        g_bufferCount = 0;
        g_frames.clear();
        g_frames.resize(newCount);

        D3D12_DESCRIPTOR_HEAP_DESC d = {};
        d.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        d.NumDescriptors = newCount;
        d.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        if (FAILED(g_device->CreateDescriptorHeap(&d, IID_PPV_ARGS(&g_rtvHeap))))
        {
            g_rtvHeap = nullptr;
            LOG_ERR("ResizeFrameResources: RTV heap (%u) creation failed.", newCount);
            return false;
        }

        for (UINT i = 0; i < newCount; ++i)
        {
            if (FAILED(g_device->CreateCommandAllocator(
                    D3D12_COMMAND_LIST_TYPE_DIRECT,
                    IID_PPV_ARGS(&g_frames[i].commandAllocator))))
            {
                g_frames[i].commandAllocator = nullptr;
                LOG_ERR("ResizeFrameResources: command allocator %u creation failed.", i);
                return false;
            }
        }
        g_bufferCount = newCount;
        return true;
    }









    static bool ReconcileSwapChain(IDXGISwapChain3* swapChain)
    {
        DXGI_SWAP_CHAIN_DESC desc = {};
        if (FAILED(swapChain->GetDesc(&desc)))
            return false;

        const UINT        newW   = desc.BufferDesc.Width;
        const UINT        newH   = desc.BufferDesc.Height;
        const DXGI_FORMAT newFmt = desc.BufferDesc.Format;






        if (swapChain == g_swapChain && desc.BufferCount == g_bufferCount &&
            newW == g_scWidth && newH == g_scHeight && newFmt == g_scFormat)
            return true;




        ID3D12Device* dev = nullptr;
        if (SUCCEEDED(swapChain->GetDevice(IID_PPV_ARGS(&dev))))
        {
            const bool sameDevice = (dev == g_device);
            dev->Release();
            if (!sameDevice)
            {
                static bool s_warned = false;
                if (!s_warned) { s_warned = true; LOG_ERR("Swapchain device changed - overlay paused this frame."); }
                return false;
            }
        }


        WaitForOverlayIdle();







        static UINT s_failedW = 0, s_failedH = 0, s_failedCount = 0;
        const auto fail = [&](const char* what) {
            CleanupRenderTargets();
            g_swapChain = nullptr;
            if (s_failedW != newW || s_failedH != newH || s_failedCount != desc.BufferCount)
            {
                s_failedW = newW; s_failedH = newH; s_failedCount = desc.BufferCount;
                LOG_ERR("ReconcileSwapChain: %s for %ux%u, %u buffers; the overlay retries next frame.",
                        what, newW, newH, desc.BufferCount);
            }
            return false;
        };

        if (desc.BufferCount != g_bufferCount)
        {
            if (!ResizeFrameResources(desc.BufferCount))
                return fail("frame resources could not be rebuilt");
        }
        else
        {
            CleanupRenderTargets();
        }

        g_hwnd = desc.OutputWindow;
        if (!CreateRenderTargets(swapChain))
            return fail("render targets could not be rebuilt");




        if (!CreateOffscreenTarget(newW, newH))
            return fail("the offscreen target could not be rebuilt");

        g_swapChain = swapChain;
        g_scWidth   = newW;
        g_scHeight  = newH;
        g_scFormat  = newFmt;
        s_failedW = s_failedH = s_failedCount = 0;

        LOG("Swapchain reconciled - in-place reconfigure.");
        return true;
    }

    static void ResetOverlayInitialization()
    {
        if (ImGui::GetCurrentContext())
        {
            const auto& io = ImGui::GetIO();
            if (io.BackendRendererUserData) ImGui_ImplDX12_Shutdown();
            if (io.BackendPlatformUserData) ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
        }
        CleanupRenderTargets();
        ReleaseOffscreenTarget();
        const auto release = [](auto*& value) { if (value) { value->Release(); value = nullptr; } };
        for (auto& frame : g_frames) release(frame.commandAllocator);
        g_frames.clear();
        release(g_commandList);
        release(g_srvHeap);
        release(g_rtvHeap);
        release(g_fence);
        release(g_compositePSO);
        release(g_compositeRootSig);
        release(g_device);
        g_compositeFormat = DXGI_FORMAT_UNKNOWN;
        g_fenceValue = 0;
        g_swapChain = nullptr;
        g_imguiReady = false;
        if (g_fenceEvent) { CloseHandle(g_fenceEvent); g_fenceEvent = nullptr; }
        if (g_frameFenceEvent) { CloseHandle(g_frameFenceEvent); g_frameFenceEvent = nullptr; }
        g_bufferCount = 0;
    }

    static bool InitImGui(IDXGISwapChain3* swapChain)
    {
        if (FAILED(swapChain->GetDevice(IID_PPV_ARGS(&g_device))))
        {
            LOG_ERR("InitImGui: swapChain->GetDevice failed.");
            return false;
        }

        DXGI_SWAP_CHAIN_DESC desc = {};
        if (FAILED(swapChain->GetDesc(&desc)) || !desc.OutputWindow || !desc.BufferCount)
            return false;
        g_hwnd        = desc.OutputWindow;
        g_bufferCount = desc.BufferCount;
        g_scWidth     = desc.BufferDesc.Width;
        g_scHeight    = desc.BufferDesc.Height;
        g_scFormat    = desc.BufferDesc.Format;
        g_frames.clear();
        g_frames.resize(g_bufferCount);








        {
            D3D12_DESCRIPTOR_HEAP_DESC d = {};
            d.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            d.NumDescriptors = kSrvHeapSlots;
            d.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            if (FAILED(g_device->CreateDescriptorHeap(&d, IID_PPV_ARGS(&g_srvHeap))))
            {
                LOG_ERR("InitImGui: SRV heap creation failed.");
                return false;
            }
        }


        {
            D3D12_DESCRIPTOR_HEAP_DESC d = {};
            d.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            d.NumDescriptors = g_bufferCount;
            d.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            if (FAILED(g_device->CreateDescriptorHeap(&d, IID_PPV_ARGS(&g_rtvHeap))))
            {
                LOG_ERR("InitImGui: RTV heap creation failed.");
                return false;
            }
        }


        for (UINT i = 0; i < g_bufferCount; ++i)
        {
            if (FAILED(g_device->CreateCommandAllocator(
                    D3D12_COMMAND_LIST_TYPE_DIRECT,
                    IID_PPV_ARGS(&g_frames[i].commandAllocator))))
            {
                LOG_ERR("InitImGui: command allocator %u creation failed.", i);
                return false;
            }
        }

        if (FAILED(g_device->CreateCommandList(
                0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                g_frames[0].commandAllocator, nullptr,
                IID_PPV_ARGS(&g_commandList))))
        {
            LOG_ERR("InitImGui: command list creation failed.");
            return false;
        }
        if (FAILED(g_commandList->Close()))
            return false;


        g_commandList->SetName(L"ModActivatorOverlayCmdList");


        if (FAILED(g_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence))))
        {
            LOG_ERR("InitImGui: fence creation failed.");
            return false;
        }
        g_fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        g_frameFenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!g_fenceEvent || !g_frameFenceEvent)
        {
            LOG_ERR("InitImGui: fence event creation failed.");
            return false;
        }

        if (!CreateRenderTargets(swapChain))
        {
            LOG_ERR("InitImGui: render target creation failed.");
            return false;
        }

        if (!CreateOffscreenTarget(g_scWidth, g_scHeight))
        {
            LOG_ERR("InitImGui: offscreen target creation failed.");
            return false;
        }


        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;




        io.ConfigErrorRecoveryEnableTooltip = false;
        io.ConfigErrorRecoveryEnableAssert  = false;

        gui::InitStyle(static_cast<float>(g_scHeight));

        if (!ImGui_ImplWin32_Init(g_hwnd))
            return false;




        if (!ImGui_ImplDX12_Init(
            g_device, g_bufferCount, DXGI_FORMAT_R8G8B8A8_UNORM, g_srvHeap,
            g_srvHeap->GetCPUDescriptorHandleForHeapStart(),
            g_srvHeap->GetGPUDescriptorHandleForHeapStart()))
            return false;


        input::Init(g_hwnd);

        g_swapChain = swapChain;

        LOG_OK("Overlay ready.");
        return true;
    }
