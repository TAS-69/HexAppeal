    // Streamline may route creation through a factory interface obtained
    // separately from the one returned by the game's DXGI import.
    using CreateWindowSwapChain = HRESULT (STDMETHODCALLTYPE*)(IDXGIFactory2*, IUnknown*, HWND,
        const DXGI_SWAP_CHAIN_DESC1*, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*, IDXGIOutput*, IDXGISwapChain1**);
    using CreateLegacySwapChain = HRESULT (STDMETHODCALLTYPE*)(IDXGIFactory*, IUnknown*, DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**);
    static CreateWindowSwapChain streamlineCreateForWindow = nullptr;
    static CreateLegacySwapChain streamlineCreateLegacy = nullptr;
    static std::mutex streamlineFactoryMutex;
    static thread_local bool creatingProviderSwapChain = false;
    static std::atomic<unsigned> streamlineFactoryHandoffs{0};

    struct ProviderCreation
    {
        const bool alreadyCreating = creatingProviderSwapChain;
        ProviderCreation() { creatingProviderSwapChain = true; }
        ~ProviderCreation() { creatingProviderSwapChain = alreadyCreating; }
    };

    static HRESULT STDMETHODCALLTYPE OnStreamlineCreateForWindow(IDXGIFactory2* factory, IUnknown* queue, HWND window,
        const DXGI_SWAP_CHAIN_DESC1* desc, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* fullscreen,
        IDXGIOutput* output, IDXGISwapChain1** chain)
    {
        const bool attach = !creatingProviderSwapChain && !hex::creation::FactoryCall::Active();
        const ProviderCreation creation;
        if (attach && desc) PrepareGameSwapChain(window, desc->Width, desc->Height);
        const auto result = streamlineCreateForWindow(factory, queue, window, desc, fullscreen, output, chain);
        if (attach && SUCCEEDED(result) && AttachGameSwapChain(queue, window, chain))
            streamlineFactoryHandoffs.fetch_add(1, std::memory_order_relaxed);
        return result;
    }

    static HRESULT STDMETHODCALLTYPE OnStreamlineCreateLegacy(IDXGIFactory* factory, IUnknown* queue,
        DXGI_SWAP_CHAIN_DESC* desc, IDXGISwapChain** chain)
    {
        const bool attach = !creatingProviderSwapChain && !hex::creation::FactoryCall::Active();
        const ProviderCreation creation;
        const HWND window = desc ? desc->OutputWindow : nullptr;
        if (attach && desc) PrepareGameSwapChain(window, desc->BufferDesc.Width, desc->BufferDesc.Height);
        const auto result = streamlineCreateLegacy(factory, queue, desc, chain);
        if (attach && SUCCEEDED(result) && chain && *chain && AttachLegacyGameSwapChain(queue, window, chain))
            streamlineFactoryHandoffs.fetch_add(1, std::memory_order_relaxed);
        return result;
    }

    template<class Function>
    static void ObserveStreamlineFactoryMethod(void* address, const char* method, Function callback, Function& original)
    {
        if (original || !hex::creation::ExecutableAddress(address)) return;
        HMODULE owner = nullptr;
        const auto streamline = GetModuleHandleW(L"sl.interposer.dll");
        if (!streamline || !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(address), &owner) || owner != streamline)
            return;
        uint8_t code[16]{};
        if (!hex::creation::Read(address, code, sizeof code) || ExistingDetour(code)) return;
        void* continuation = nullptr;
        if (MH_CreateHook(address, reinterpret_cast<void*>(callback), &continuation) != MH_OK) return;
        original = reinterpret_cast<Function>(continuation);
        if (MH_EnableHook(address) != MH_OK)
        {
            MH_RemoveHook(address);
            original = nullptr;
            return;
        }
        LOG("Hex Appeal is observing Streamline factory %s (existing detours untouched).", method);
    }

    static void ObserveStreamlineFactory(IDXGIFactory2* factory)
    {
        const std::lock_guard<std::mutex> lock(streamlineFactoryMutex);
        auto** methods = *reinterpret_cast<void***>(factory);
        ObserveStreamlineFactoryMethod(methods[10], "CreateSwapChain", &OnStreamlineCreateLegacy, streamlineCreateLegacy);
        ObserveStreamlineFactoryMethod(methods[15], "CreateSwapChainForHwnd", &OnStreamlineCreateForWindow, streamlineCreateForWindow);
    }
