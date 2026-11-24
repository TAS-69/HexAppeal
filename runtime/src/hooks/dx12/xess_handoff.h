    // XeSS-FG can create the swapchain inside the SDK and return it through a
    // separate game import. Keep its queue until the game requests that chain.
    using XeSSInit = int (WINAPI*)(void*, HWND, const DXGI_SWAP_CHAIN_DESC1*,
        const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*, ID3D12CommandQueue*, IDXGIFactory2*, const void*);
    using XeSSGet = int (WINAPI*)(void*, REFIID, void**);
    using XeSSDestroy = int (WINAPI*)(void*);
    static std::atomic<XeSSInit> xessInit{nullptr};
    static std::atomic<XeSSGet> xessGet{nullptr};
    static std::atomic<XeSSDestroy> xessDestroy{nullptr};
    static std::atomic<unsigned> xessHandoffs{0};

    struct XeSSCreation
    {
        void* context = nullptr;
        HWND window = nullptr;
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue;
    };

    static std::mutex xessCreationMutex;
    static XeSSCreation pendingXeSSCreation;

    static int WINAPI OnXeSSInit(void* context, HWND window, const DXGI_SWAP_CHAIN_DESC1* desc,
        const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* fullscreen, ID3D12CommandQueue* queue,
        IDXGIFactory2* factory, const void* params)
    {
        if (desc) PrepareGameSwapChain(window, desc->Width, desc->Height);
        if (const auto wrapper = AsGameFactory(factory)) factory = wrapper->Inner();
        const ProviderCreation creationScope;
        const auto result = xessInit.load(std::memory_order_acquire)(context, window, desc, fullscreen, queue, factory, params);
        // The SDK allows a single initialized XeSS-FG swapchain per process.
        if (result >= 0 && context && window && queue)
        {
            XeSSCreation creation{context, window, Microsoft::WRL::ComPtr<ID3D12CommandQueue>(queue)};
            const std::lock_guard<std::mutex> lock(xessCreationMutex);
            std::swap(pendingXeSSCreation, creation);
        }
        return result;
    }

    static XeSSCreation TakeXeSSCreation(void* context)
    {
        XeSSCreation creation;
        const std::lock_guard<std::mutex> lock(xessCreationMutex);
        if (pendingXeSSCreation.context == context) std::swap(creation, pendingXeSSCreation);
        return creation;
    }

    static int WINAPI OnXeSSGet(void* context, REFIID iid, void** result)
    {
        const auto status = xessGet.load(std::memory_order_acquire)(context, iid, result);
        if (status < 0 || !result || !*result || !WrappedIDXGISwapChain::Supports(iid)) return status;

        auto creation = TakeXeSSCreation(context);
        if (!creation.queue) return status;
        auto* original = static_cast<IUnknown*>(*result);
        IDXGISwapChain1* chain = nullptr;
        if (FAILED(original->QueryInterface(IID_PPV_ARGS(&chain))) || !chain) return status;
        HWND window = nullptr;
        if (SUCCEEDED(chain->GetHwnd(&window)) && window == creation.window)
        {
            auto* previous = chain;
            if (AttachGameSwapChain(creation.queue.Get(), window, &chain))
            {
                xessHandoffs.fetch_add(1, std::memory_order_relaxed);
                LOG("Hex Appeal received the game's XeSS-FG swapchain and its presentation queue.");
            }
            if (chain != previous)
            {
                original->Release();
                *result = chain;
                return status;
            }
        }
        chain->Release();
        return status;
    }

    static int WINAPI OnXeSSDestroy(void* context)
    {
        const auto result = xessDestroy.load(std::memory_order_acquire)(context);
        if (result >= 0) TakeXeSSCreation(context);
        return result;
    }

    static bool InstallXeSSImports(const MODULEINFO& game)
    {
        constexpr auto library = "libxess_fg.dll";
        const bool get = InstallGameImport(game, library, "xefgSwapChainD3D12GetSwapChainPtr", &OnXeSSGet, xessGet);
        if (!get) return false;
        InstallGameImport(game, library, "xefgSwapChainDestroy", &OnXeSSDestroy, xessDestroy);
        return InstallGameImport(game, library, "xefgSwapChainD3D12InitFromSwapChainDesc", &OnXeSSInit, xessInit);
    }
