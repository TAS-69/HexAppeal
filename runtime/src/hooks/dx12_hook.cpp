
// MIT License, Copyright (c) 2026 XeTrinityz. See runtime/licenses/UPSTREAM-NOTICES.md.

#include "dx12_hook.h"

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <psapi.h>
#include <wrl/client.h>
#include <algorithm>
#include <atomic>
#include <new>
#include <mutex>
#include <string>
#include <vector>

#include <MinHook.h>
#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>

#include "input.h"
#include "reshade_events.h"
#include "xinput_hook.h"
#include "hdr_composite_shader.h"
#include "../core/log.h"
#include "../core/settings.h"
#include "../core/state.h"
#include "../graphics_diagnostics.h"
#include "../gui/menu.h"
#include "farhook.h"
#include "dx12/creation_chain.h"
#include "dx12/factory_imports.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

namespace ml::hooks
{




#include "dx12/state.h"

#include "dx12/resources.h"

#include "dx12/draw.h"

#include "dx12/present_hooks.h"

#include "dx12/hook_owners.h"

#include "dx12/swapchain_wrapper.h"

#include "dx12/install.h"
#include "dx12/streamline_factory.h"

#include "dx12/factory_wrapper.h"

#include "dx12/creation_patch.h"






    static void EnableDredIfAvailable()
    {
        ID3D12DeviceRemovedExtendedDataSettings* dred = nullptr;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dred))) && dred)
        {
            dred->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
            dred->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
            dred->Release();
            LOG("DRED armed - a device removal will report GPU breadcrumbs + page faults.");
        }
        else
        {
            LOG("DRED unavailable on this system - device-removed causes stay opaque.");
        }
    }

    static HMODULE FindReShade()
    {
        HMODULE modules[512]{};
        DWORD bytes = 0;
        if (!K32EnumProcessModules(GetCurrentProcess(), modules, sizeof modules, &bytes))
            return nullptr;
        const auto count = (std::min)(bytes, static_cast<DWORD>(sizeof modules)) /
                           sizeof(HMODULE);
        for (size_t i = 0; i < count; ++i)
            if (GetProcAddress(modules[i], "ReShadeRegisterAddon"))
                return modules[i];
        return nullptr;
    }

    static bool RegisterReShadeEvents(HMODULE self)
    {
        const auto reshade = FindReShade();
        if (!reshade) return false;
        const auto registerAddon = reinterpret_cast<reshade_api::RegisterAddon>(
            GetProcAddress(reshade, "ReShadeRegisterAddon"));
        const auto registerEvent = reinterpret_cast<reshade_api::RegisterEvent>(
            GetProcAddress(reshade, "ReShadeRegisterEventForAddon"));
        if (!registerAddon || !registerEvent || !registerAddon(self, reshade_api::version))
            return false;
        registerEvent(self, reshade_api::destroySwapchain, reinterpret_cast<void*>(&OnReShadeDestroySwapChain));
        registerEvent(self, reshade_api::present, reinterpret_cast<void*>(&OnReShadePresent));
        registerEvent(self, reshade_api::reshadePresent, reinterpret_cast<void*>(&OnReShadeEffectsPresent));
        LOG("Optional ReShade presentation registered.");
        return true;
    }

    static bool ProbeSystemPresentation()
    {
        using Microsoft::WRL::ComPtr;
        const auto dxgi = GetModuleHandleW(L"dxgi.dll");
        const auto entry = dxgi ? GetProcAddress(dxgi, "CreateDXGIFactory1") : nullptr;
        if (!entry || !ModuleInSystemDir(reinterpret_cast<void*>(entry))) return false;
        const auto create = reinterpret_cast<CreateFactory>(entry);
        ComPtr<IDXGIFactory2> factory;
        ComPtr<ID3D12Device> device;
        ComPtr<ID3D12CommandQueue> queue;
        D3D12_COMMAND_QUEUE_DESC queueDesc{};
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        if (FAILED(create(IID_PPV_ARGS(factory.GetAddressOf())))) return false;
        ObserveStreamlineFactory(factory.Get());
        if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(device.GetAddressOf()))) ||
            FAILED(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(queue.GetAddressOf()))))
            return false;
        const auto window = CreateWindowExW(0, L"STATIC", L"", WS_POPUP, 0, 0, 32, 32,
                                           nullptr, nullptr, nullptr, nullptr);
        if (!window) return false;
        DXGI_SWAP_CHAIN_DESC1 desc{};
        desc.Width = desc.Height = 32;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = 2;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        ComPtr<IDXGISwapChain1> chain;
        bool ready = false;
        if (SUCCEEDED(factory->CreateSwapChainForHwnd(queue.Get(), window, &desc, nullptr, nullptr, chain.GetAddressOf())))
        {
            if (const auto base = StreamlineBaseInterface(chain.Get()))
            {
                ComPtr<IDXGISwapChain1> native;
                if (SUCCEEDED(base.As(&native))) ready = InstallDetoursFrom(native.Get(), queue.Get(), false);
            }
            if (!ready) ready = InstallDetoursFrom(chain.Get(), queue.Get(), false);
        }
        chain.Reset();
        DestroyWindow(window);
        return ready;
    }

    static HMODULE g_overlayModule = nullptr;
    static bool g_reShadeRequested = false;
    static bool g_reShadeRegistered = false;

    static DWORD WINAPI WatchPresentation(LPVOID)
    {
        const auto started = GetTickCount64();
        uint64_t nextProbe = started + hex::creation::firstProbeDelayMs;
        bool reported = false;
        bool probed = false;
        bool saidSkipped = false;
        for (;;)
        {
            Sleep(2000);
            const auto now = GetTickCount64();
            const bool seen = g_gameSwapChainSeen.load();
            if (g_reShadeRequested && !g_reShadeRegistered)
                g_reShadeRegistered = RegisterReShadeEvents(g_overlayModule);
            if (State::Get().HasRecentPresentation(now))
            {
                ::ml::Log::MarkStartupComplete();  // for routes that did not see the creation
                continue;
            }
            InstallFactoryImports();
            if (hex::creation::PresentationProbeDue(now, nextProbe, probed, seen))
            {
                probed = ProbeSystemPresentation();
                nextProbe = now + hex::creation::probeRetryMs;
                if (probed) LOG("[overlay] Watching system presentation for a game swapchain.");
            }
            else if (seen && !probed && !saidSkipped && now >= nextProbe)
            {
                saidSkipped = true;
                LOG("[overlay] The game's swapchain was seen at creation, so no probe swapchain is created.");
            }
            if (!reported && now - started >= 60000)
            {
                reported = true;
                LOG_ERR("[overlay] No recent game-window frame. Continuing to look for a presentation route.");
                LogSwapChainCreationPath();
            }
        }
    }

    bool InstallDX12Hooks(HMODULE self, bool useReShadeEvents)
    {














        if (Settings::Get().enableDred) EnableDredIfAvailable();
        else LOG("DRED not armed (EnableDred=0). Set EnableDred=1 to report GPU breadcrumbs on a device removal.");

        InitializeCriticalSection(&g_queueLock);
        g_queueLockReady = true;

        const bool native = InstallFactoryImports();
        g_overlayModule = self;
        g_reShadeRequested = useReShadeEvents;
        const bool optional = useReShadeEvents && RegisterReShadeEvents(self);
        g_reShadeRegistered = optional;
        const auto watch = CreateThread(nullptr, 0, WatchPresentation, nullptr, 0, nullptr);
        if (watch) CloseHandle(watch);
        LOG("DX12 route discovery started (game imports=%d, ReShade=%d, retry worker=%d).",
            int(native), int(optional), int(watch != nullptr));
        return native || optional || watch != nullptr;
    }
}
