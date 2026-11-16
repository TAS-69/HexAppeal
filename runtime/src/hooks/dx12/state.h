






    using Present_t         = HRESULT (WINAPI*)(IDXGISwapChain3*, UINT, UINT);
    using ResizeBuffers_t   = HRESULT (WINAPI*)(IDXGISwapChain3*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
    using ExecuteCmdLists_t = void    (WINAPI*)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);
    using SetColorSpace1_t  = HRESULT (WINAPI*)(IDXGISwapChain3*, DXGI_COLOR_SPACE_TYPE);

    static Present_t         oPresent             = nullptr;
    static ResizeBuffers_t   oResizeBuffers       = nullptr;
    static ExecuteCmdLists_t oExecuteCommandLists = nullptr;
    static SetColorSpace1_t  oSetColorSpace1      = nullptr;














    using CSFH_t = HRESULT (STDMETHODCALLTYPE*)(IDXGIFactory2*, IUnknown*, HWND, const DXGI_SWAP_CHAIN_DESC1*, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*, IDXGIOutput*, IDXGISwapChain1**);

    static CSFH_t oFactoryCreateSwapChainForHwnd = nullptr;


    static CSFH_t g_factoryOwnTramp = nullptr;



    static void** g_factoryVt = nullptr;
    static void*  g_factoryFn = nullptr;
    static bool   g_factoryInPlace = false;
    static std::atomic<long> g_factoryCalls{0};




    static bool g_wrapperActive = false;





    static std::atomic<unsigned> g_presentSeen{0};











    static thread_local bool t_inSwapChainCreate = false;




    struct CreateGuard
    {
        bool wasNested;
        CreateGuard() : wasNested(t_inSwapChainCreate) { t_inSwapChainCreate = true; }
        ~CreateGuard() { if (!wasNested) t_inSwapChainCreate = false; }
    };





    static HWND g_wrappedHwnd = nullptr;




    static HWND g_gameHwnd = nullptr;


    struct FrameContext
    {
        ID3D12CommandAllocator*     commandAllocator = nullptr;
        ID3D12Resource*             renderTarget     = nullptr;
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle        = {};
        UINT64                      fenceValue       = 0;
    };

    static ID3D12Device*              g_device      = nullptr;
    static ID3D12DescriptorHeap*      g_rtvHeap     = nullptr;
    static ID3D12DescriptorHeap*      g_srvHeap     = nullptr;
    static ID3D12GraphicsCommandList* g_commandList = nullptr;


    static constexpr UINT             kSrvHeapSlots = 8;





    static ID3D12CommandQueue*        g_presentQueue   = nullptr;
    static CRITICAL_SECTION           g_queueLock;
    static bool                       g_queueLockReady = false;






    static bool                       g_presentQueuePinned = false;
    static ID3D12Fence*               g_fence          = nullptr;
    static HANDLE                     g_fenceEvent     = nullptr;



    static HANDLE                     g_frameFenceEvent = nullptr;
    static UINT64                     g_fenceValue     = 0;
    static std::vector<FrameContext>  g_frames;
    static UINT                       g_bufferCount    = 0;





    static IDXGISwapChain3*           g_swapChain      = nullptr;
    static HWND                       g_hwnd           = nullptr;





    static UINT                       g_scWidth        = 0;
    static UINT                       g_scHeight       = 0;
    static DXGI_FORMAT                g_scFormat       = DXGI_FORMAT_UNKNOWN;
    static bool                       g_imguiReady     = false;
    static bool                       g_initFailed     = false;
    static bool                       g_renderDisabled = false;










    static ID3D12Resource*             g_offscreenTex     = nullptr;
    static ID3D12DescriptorHeap*       g_offscreenRtvHeap = nullptr;
    static D3D12_CPU_DESCRIPTOR_HANDLE g_offscreenRtv     = {};
    static UINT                        g_offscreenW       = 0;
    static UINT                        g_offscreenH       = 0;


    static constexpr UINT kOffscreenSrvSlot = 1;

    static ID3D12RootSignature* g_compositeRootSig = nullptr;
    static ID3D12PipelineState* g_compositePSO     = nullptr;
    static DXGI_FORMAT          g_compositeFormat  = DXGI_FORMAT_UNKNOWN;





    static constexpr float kPaperWhiteNits = 203.0f;







    static DXGI_COLOR_SPACE_TYPE g_colorSpace = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;




    static UINT CompositeModeForColorSpace(DXGI_COLOR_SPACE_TYPE cs)
    {
        switch (cs)
        {
            case DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709:    return 1;
            case DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020: return 2;
            default:                                         return 0;
        }
    }

    static void OnColorSpaceChanged(DXGI_COLOR_SPACE_TYPE cs)
    {
        if (cs == g_colorSpace) return;
        g_colorSpace = cs;
        const char* name = CompositeModeForColorSpace(cs) == 2 ? "HDR10 (PQ)"
                          : CompositeModeForColorSpace(cs) == 1 ? "scRGB (linear)"
                          : "SDR";
        LOG("Swapchain color space changed to %s (%d) - overlay compositing %s.",
            name, static_cast<int>(cs),
            CompositeModeForColorSpace(cs) != 0 ? "HDR-adjusted" : "unadjusted");
    }
