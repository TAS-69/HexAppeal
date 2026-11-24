    using CreateFactory = HRESULT (WINAPI*)(REFIID, void**);
    using CreateFactory2 = HRESULT (WINAPI*)(UINT, REFIID, void**);
    using ResolveExport = FARPROC (WINAPI*)(HMODULE, LPCSTR);
    using UpgradeInterface = int (WINAPI*)(void**);

    static std::atomic<ResolveExport> resolveExport{nullptr};
    static std::atomic<HMODULE> gameDxgi{nullptr};
    static std::atomic<HMODULE> gameStreamline{nullptr};
    static std::atomic<unsigned> gameFactoriesWrapped{0};
    static thread_local bool creatingGameFactory = false;

    static GameDXGIFactory* AsGameFactory(IUnknown* object)
    {
        GameDXGIFactory* wrapper = nullptr;
        if (object && SUCCEEDED(object->QueryInterface(GameDXGIFactory::Identity,
                reinterpret_cast<void**>(&wrapper))) && wrapper)
        {
            const bool sameObject = static_cast<IUnknown*>(wrapper) == object;
            wrapper->Release();
            if (sameObject) return wrapper;
        }
        return nullptr;
    }

    static void WrapGameFactory(REFIID iid, void** result)
    {
        if (!result || !*result || !GameDXGIFactory::Supports(iid)) return;
        auto* original = static_cast<IUnknown*>(*result);
        if (AsGameFactory(original)) return;
        IDXGIFactory7* factory = nullptr;
        if (FAILED(original->QueryInterface(IID_PPV_ARGS(&factory))) || !factory) return;
        ObserveStreamlineFactory(factory);
        auto* wrapper = new (std::nothrow) GameDXGIFactory(factory);
        if (!wrapper)
        {
            factory->Release();
            return;
        }
        original->Release();
        *result = static_cast<IDXGIFactory7*>(wrapper);
        if (gameFactoriesWrapped.fetch_add(1, std::memory_order_relaxed) == 0)
            LOG("Hex Appeal attached to a DXGI factory returned to Crimson Desert.");
    }

    template<class Call>
    static HRESULT CreateGameFactory(Call&& create, REFIID iid, void** result)
    {
        if (creatingGameFactory) return DXGI_ERROR_INVALID_CALL;
        struct Reset
        {
            ~Reset() { creatingGameFactory = false; }
        } reset;
        creatingGameFactory = true;

        const HRESULT hr = create();
        if (SUCCEEDED(hr)) WrapGameFactory(iid, result);
        return hr;
    }

    enum class FactorySource { DxgiImport, DxgiExport, StreamlineImport, StreamlineExport };
    template<FactorySource Source>
    struct FactoryCallbacks
    {
        inline static std::atomic<CreateFactory> create{nullptr};
        inline static std::atomic<CreateFactory> create1{nullptr};
        inline static std::atomic<CreateFactory2> create2{nullptr};
        inline static std::atomic<UpgradeInterface> upgrade{nullptr};

        static HRESULT WINAPI Create(REFIID iid, void** result)
        {
            return CreateGameFactory([&] { return create.load(std::memory_order_acquire)(iid, result); }, iid, result);
        }

        static HRESULT WINAPI Create1(REFIID iid, void** result)
        {
            return CreateGameFactory([&] { return create1.load(std::memory_order_acquire)(iid, result); }, iid, result);
        }

        static HRESULT WINAPI Create2(UINT flags, REFIID iid, void** result)
        {
            return CreateGameFactory([&] { return create2.load(std::memory_order_acquire)(flags, iid, result); }, iid, result);
        }

        static int WINAPI Upgrade(void** result)
        {
            const auto forward = upgrade.load(std::memory_order_acquire);
            auto* wrapper = result ? AsGameFactory(static_cast<IUnknown*>(*result)) : nullptr;
            if (!wrapper)
            {
                const auto status = forward(result);
                if (status == 0) WrapGameFactory(__uuidof(IUnknown), result);
                return status;
            }

            auto* native = wrapper->Inner();
            native->AddRef();
            void* upgraded = native;
            const auto status = forward(&upgraded);
            if (status == 0)
            {
                wrapper->Release();
                *result = upgraded;
                WrapGameFactory(__uuidof(IUnknown), result);
            }
            else
            {
                native->Release();
            }
            return status;
        }
    };

    template<class Function>
    static FARPROC BindResolvedFactory(FARPROC entry, Function callback, std::atomic<Function>& original)
    {
        auto next = reinterpret_cast<Function>(entry);
        if (hex::creation::Observe(reinterpret_cast<void*>(next), reinterpret_cast<void*>(callback), true).state !=
            hex::creation::EntryState::Other) return entry;
        Function previous = nullptr;
        if (original.compare_exchange_strong(previous, next))
            LOG("Hex Appeal attached to a DXGI factory export resolved by Crimson Desert.");
        else if (previous != next)
            return entry;
        return reinterpret_cast<FARPROC>(callback);
    }

    template<FactorySource Source>
    static FARPROC BindFactoryExport(FARPROC entry, LPCSTR name)
    {
        using Callbacks = FactoryCallbacks<Source>;
        if (std::strcmp(name, "CreateDXGIFactory") == 0)
            return BindResolvedFactory(entry, &Callbacks::Create, Callbacks::create);
        if (std::strcmp(name, "CreateDXGIFactory1") == 0)
            return BindResolvedFactory(entry, &Callbacks::Create1, Callbacks::create1);
        if (std::strcmp(name, "CreateDXGIFactory2") == 0)
            return BindResolvedFactory(entry, &Callbacks::Create2, Callbacks::create2);
        if constexpr (Source == FactorySource::StreamlineExport)
            if (std::strcmp(name, "slUpgradeInterface") == 0)
                return BindResolvedFactory(entry, &Callbacks::Upgrade, Callbacks::upgrade);
        return entry;
    }

    static FARPROC WINAPI OnResolveExport(HMODULE library, LPCSTR name)
    {
        const auto entry = resolveExport.load(std::memory_order_acquire)(library, name);
        if (!entry || !name || IS_INTRESOURCE(name)) return entry;
        const auto error = GetLastError();
        auto result = entry;
        if (library == gameDxgi.load())
            result = BindFactoryExport<FactorySource::DxgiExport>(entry, name);
        else if (library == gameStreamline.load())
            result = BindFactoryExport<FactorySource::StreamlineExport>(entry, name);
        SetLastError(error);
        return result;
    }

    template<class Function>
    static bool InstallGameImport(const MODULEINFO& module, const char* library, const char* name,
                                  Function callback, std::atomic<Function>& original)
    {
        auto** slot = hex::creation::FindNamedImport(module.lpBaseOfDll, module.SizeOfImage, library, name);
        if (!slot) return false;

        void* entry = nullptr;
        if (!hex::creation::Read(slot, &entry, sizeof entry)) return false;
        if (entry == reinterpret_cast<void*>(callback)) return true;
        if (hex::creation::Observe(entry, reinterpret_cast<void*>(callback), true).state != hex::creation::EntryState::Other)
        {
            LOG_ERR("The game import for %s does not have a callable target.", name);
            return false;
        }

        Function previous = nullptr;
        if (!original.compare_exchange_strong(previous, reinterpret_cast<Function>(entry)) &&
            previous != reinterpret_cast<Function>(entry))
            return false;  // Cached callbacks must keep their original target.
        DWORD protection = 0;
        if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &protection))
            return false;
        const bool installed = InterlockedCompareExchangePointer(slot, reinterpret_cast<void*>(callback), entry) == entry;
        DWORD discarded = 0;
        VirtualProtect(slot, sizeof(void*), protection, &discarded);
        if (installed) LOG("Hex Appeal attached to the game's %s!%s import.", library, name);
        else LOG_ERR("The game import for %s changed during installation.", name);
        return installed;
    }

    template<FactorySource Source>
    static bool InstallFactoryLibrary(const MODULEINFO& game, const char* library)
    {
        using Callbacks = FactoryCallbacks<Source>;
        const bool base = InstallGameImport(game, library, "CreateDXGIFactory", &Callbacks::Create, Callbacks::create);
        const bool first = InstallGameImport(game, library, "CreateDXGIFactory1", &Callbacks::Create1, Callbacks::create1);
        const bool second = InstallGameImport(game, library, "CreateDXGIFactory2", &Callbacks::Create2, Callbacks::create2);
        bool upgrade = false;
        if constexpr (Source == FactorySource::StreamlineImport)
            upgrade = InstallGameImport(game, library, "slUpgradeInterface", &Callbacks::Upgrade, Callbacks::upgrade);
        return base || first || second || upgrade;
    }

#include "xess_handoff.h"

    static bool InstallFactoryImports()
    {
        MODULEINFO game = {};
        if (!K32GetModuleInformation(GetCurrentProcess(), GetModuleHandleW(nullptr), &game, sizeof game))
            return false;
        gameDxgi = GetModuleHandleW(L"dxgi.dll");
        gameStreamline = GetModuleHandleW(L"sl.interposer.dll");
        const bool dxgi = InstallFactoryLibrary<FactorySource::DxgiImport>(game, "dxgi.dll");
        const bool streamline = InstallFactoryLibrary<FactorySource::StreamlineImport>(game, "sl.interposer.dll");
        const bool dynamic = InstallGameImport(game, "kernel32.dll", "GetProcAddress", &OnResolveExport, resolveExport);
        const bool xess = InstallXeSSImports(game);
        return dxgi || streamline || dynamic || xess;
    }

    void LogSwapChainCreationPath()
    {
        LOG("Hex Appeal observed %u game DXGI factories and %ld swapchain creation calls. The shared factory table stays untouched.",
            gameFactoriesWrapped.load(std::memory_order_relaxed), g_factoryCalls.load(std::memory_order_relaxed));
        LOG("Renderer handoffs observed: Streamline %u, XeSS-FG %u.",
            streamlineFactoryHandoffs.load(std::memory_order_relaxed), xessHandoffs.load(std::memory_order_relaxed));
    }
