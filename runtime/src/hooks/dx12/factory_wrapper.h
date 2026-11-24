    class GameDXGIFactory final : public IDXGIFactory7
    {
        IDXGIFactory7* const inner;
        std::atomic<ULONG> references{1};

    public:
        explicit GameDXGIFactory(IDXGIFactory7* factory) : inner(factory) {}
        // Identify our wrapper without relying on the layout of another DXGI object.
        static constexpr GUID Identity{0x32a8937e, 0x44f3, 0x41c7, {0x88, 0x6d, 0x3e, 0xdd, 0xfd, 0x17, 0x49, 0x90}};
        IDXGIFactory7* Inner() const { return inner; }

        static bool Supports(REFIID iid)
        {
            return iid == __uuidof(IUnknown) || iid == __uuidof(IDXGIObject) ||
                iid == __uuidof(IDXGIFactory) || iid == __uuidof(IDXGIFactory1) ||
                iid == __uuidof(IDXGIFactory2) || iid == __uuidof(IDXGIFactory3) ||
                iid == __uuidof(IDXGIFactory4) || iid == __uuidof(IDXGIFactory5) ||
                iid == __uuidof(IDXGIFactory6) || iid == __uuidof(IDXGIFactory7);
        }

        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** result) override
        {
            if (!result) return E_POINTER;
            if (iid == Identity)
            {
                *result = this;
                AddRef();
                return S_OK;
            }
            if (!Supports(iid)) return inner->QueryInterface(iid, result);
            *result = static_cast<IDXGIFactory7*>(this);
            AddRef();
            return S_OK;
        }
        ULONG STDMETHODCALLTYPE AddRef() override { return ++references; }
        ULONG STDMETHODCALLTYPE Release() override
        {
            const auto count = --references;
            if (!count)
            {
                inner->Release();
                delete this;
            }
            return count;
        }
        HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID key, UINT size, const void* data) override { return inner->SetPrivateData(key, size, data); }
        HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID key, const IUnknown* object) override { return inner->SetPrivateDataInterface(key, object); }
        HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID key, UINT* size, void* data) override { return inner->GetPrivateData(key, size, data); }
        HRESULT STDMETHODCALLTYPE GetParent(REFIID iid, void** result) override { return inner->GetParent(iid, result); }
        HRESULT STDMETHODCALLTYPE EnumAdapters(UINT index, IDXGIAdapter** result) override { return inner->EnumAdapters(index, result); }
        HRESULT STDMETHODCALLTYPE MakeWindowAssociation(HWND window, UINT flags) override { return inner->MakeWindowAssociation(window, flags); }
        HRESULT STDMETHODCALLTYPE GetWindowAssociation(HWND* window) override { return inner->GetWindowAssociation(window); }
        HRESULT STDMETHODCALLTYPE CreateSoftwareAdapter(HMODULE module, IDXGIAdapter** result) override { return inner->CreateSoftwareAdapter(module, result); }
        HRESULT STDMETHODCALLTYPE EnumAdapters1(UINT index, IDXGIAdapter1** result) override { return inner->EnumAdapters1(index, result); }
        BOOL STDMETHODCALLTYPE IsCurrent() override { return inner->IsCurrent(); }
        BOOL STDMETHODCALLTYPE IsWindowedStereoEnabled() override { return inner->IsWindowedStereoEnabled(); }

        HRESULT STDMETHODCALLTYPE CreateSwapChain(IUnknown* device, DXGI_SWAP_CHAIN_DESC* desc, IDXGISwapChain** chain) override
        {
            const HWND window = desc ? desc->OutputWindow : nullptr;
            const hex::creation::FactoryCall call(window);
            if (!call) return DXGI_ERROR_INVALID_CALL;
            if (!call.nested() && desc) PrepareGameSwapChain(window, desc->BufferDesc.Width, desc->BufferDesc.Height);
            const HRESULT result = inner->CreateSwapChain(device, desc, chain);
            g_factoryCalls.fetch_add(1, std::memory_order_relaxed);
            if (!call.nested() && SUCCEEDED(result) && chain && *chain)
                AttachLegacyGameSwapChain(device, window, chain);
            return result;
        }
        HRESULT STDMETHODCALLTYPE CreateSwapChainForHwnd(IUnknown* device, HWND window, const DXGI_SWAP_CHAIN_DESC1* desc,
            const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* fullscreen, IDXGIOutput* output, IDXGISwapChain1** chain) override
        {
            const hex::creation::FactoryCall call(window);
            if (!call) return DXGI_ERROR_INVALID_CALL;
            if (!call.nested() && desc) PrepareGameSwapChain(window, desc->Width, desc->Height);
            const HRESULT result = inner->CreateSwapChainForHwnd(device, window, desc, fullscreen, output, chain);
            g_factoryCalls.fetch_add(1, std::memory_order_relaxed);
            if (!call.nested() && SUCCEEDED(result)) AttachGameSwapChain(device, window, chain);
            return result;
        }
        HRESULT STDMETHODCALLTYPE CreateSwapChainForCoreWindow(IUnknown* device, IUnknown* window,
            const DXGI_SWAP_CHAIN_DESC1* desc, IDXGIOutput* output, IDXGISwapChain1** chain) override
        {
            return inner->CreateSwapChainForCoreWindow(device, window, desc, output, chain);
        }
        HRESULT STDMETHODCALLTYPE CreateSwapChainForComposition(IUnknown* device, const DXGI_SWAP_CHAIN_DESC1* desc,
            IDXGIOutput* output, IDXGISwapChain1** chain) override
        {
            return inner->CreateSwapChainForComposition(device, desc, output, chain);
        }

        HRESULT STDMETHODCALLTYPE GetSharedResourceAdapterLuid(HANDLE resource, LUID* luid) override { return inner->GetSharedResourceAdapterLuid(resource, luid); }
        HRESULT STDMETHODCALLTYPE RegisterStereoStatusWindow(HWND window, UINT message, DWORD* cookie) override { return inner->RegisterStereoStatusWindow(window, message, cookie); }
        HRESULT STDMETHODCALLTYPE RegisterStereoStatusEvent(HANDLE event, DWORD* cookie) override { return inner->RegisterStereoStatusEvent(event, cookie); }
        void STDMETHODCALLTYPE UnregisterStereoStatus(DWORD cookie) override { inner->UnregisterStereoStatus(cookie); }
        HRESULT STDMETHODCALLTYPE RegisterOcclusionStatusWindow(HWND window, UINT message, DWORD* cookie) override { return inner->RegisterOcclusionStatusWindow(window, message, cookie); }
        HRESULT STDMETHODCALLTYPE RegisterOcclusionStatusEvent(HANDLE event, DWORD* cookie) override { return inner->RegisterOcclusionStatusEvent(event, cookie); }
        void STDMETHODCALLTYPE UnregisterOcclusionStatus(DWORD cookie) override { inner->UnregisterOcclusionStatus(cookie); }
        UINT STDMETHODCALLTYPE GetCreationFlags() override { return inner->GetCreationFlags(); }
        HRESULT STDMETHODCALLTYPE EnumAdapterByLuid(LUID luid, REFIID iid, void** result) override { return inner->EnumAdapterByLuid(luid, iid, result); }
        HRESULT STDMETHODCALLTYPE EnumWarpAdapter(REFIID iid, void** result) override { return inner->EnumWarpAdapter(iid, result); }
        HRESULT STDMETHODCALLTYPE CheckFeatureSupport(DXGI_FEATURE feature, void* data, UINT size) override { return inner->CheckFeatureSupport(feature, data, size); }
        HRESULT STDMETHODCALLTYPE EnumAdapterByGpuPreference(UINT index, DXGI_GPU_PREFERENCE preference, REFIID iid, void** result) override
        {
            return inner->EnumAdapterByGpuPreference(index, preference, iid, result);
        }
        HRESULT STDMETHODCALLTYPE RegisterAdaptersChangedEvent(HANDLE event, DWORD* cookie) override { return inner->RegisterAdaptersChangedEvent(event, cookie); }
        HRESULT STDMETHODCALLTYPE UnregisterAdaptersChangedEvent(DWORD cookie) override { return inner->UnregisterAdaptersChangedEvent(cookie); }
    };
