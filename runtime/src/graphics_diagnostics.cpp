#include "graphics_diagnostics.h"
#include <Shlwapi.h>
#include <XmlLite.h>
#include <dxcore.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string_view>
#include <vector>

namespace hex {
namespace {
using Microsoft::WRL::ComPtr;
namespace fs = std::filesystem;

constexpr std::array videoOptions{
    std::pair{L"_frameGenerationModeSelect", "Frame Generation"},
    std::pair{L"_numFramesToGenerate", "Frames to generate"},
    std::pair{L"_enableHDR", "HDR"},
    std::pair{L"_enableVsync", "VSync"},
};

constexpr std::array graphicsLibraries{
    L"sl.interposer.dll",
    L"sl.dlss_g.dll",
    L"nvngx_dlssg.dll",
    L"sl.dlss.dll",
    L"nvngx_dlss.dll",
    L"amd_fidelityfx_loader_dx12.dll",
    L"amd_fidelityfx_framegeneration_dx12.dll",
    L"amd_fidelityfx_upscaler_dx12.dll",
    L"libxess_fg.dll",
    L"libxess.dll",
    L"GameOverlayRenderer64.dll",
    L"dxgi.dll",
    L"d3d12.dll",
    L"D3D12Core.dll",
};

struct GameVideo {
    std::array<std::string, videoOptions.size()> values;
    bool read = false;
};

struct Swapchain {
    unsigned width = 0;
    unsigned height = 0;
    LUID adapter{};
    bool hasAdapter = false;
};

struct LoadedModule {
    HMODULE handle = nullptr;
    std::string version;
};

struct GraphicsDiagnostics {
    std::mutex swapchainMutex;
    Swapchain swapchain;
    std::mutex reportMutex;
    fs::path videoSettings;
    fs::file_time_type videoModified{};
    uintmax_t videoSize = 0;
    GameVideo video;
    Swapchain adapterObservation;
    std::string renderingAdapter;
    std::array<LoadedModule, graphicsLibraries.size()> modules;
};

GraphicsDiagnostics& diagnostics()
{
    static GraphicsDiagnostics state;
    return state;
}

std::string utf8(std::wstring_view value)
{
    if (value.empty()) return {};
    const auto length = static_cast<int>(value.size());
    const int bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), length,
        nullptr, 0, nullptr, nullptr);
    if (!bytes) return {};
    std::string result(bytes, '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), length, result.data(), bytes, nullptr, nullptr);
    return result;
}

std::string xmlAttribute(IXmlReader& reader, const wchar_t* name)
{
    if (reader.MoveToAttributeByName(name, nullptr) != S_OK) return {};
    const wchar_t* value = nullptr;
    UINT length = 0;
    std::string result;
    if (reader.GetValue(&value, &length) == S_OK && length && length < 160)
        result = utf8({value, length});
    reader.MoveToElement();
    for (auto& ch : result)
        if (static_cast<unsigned char>(ch) < 32) ch = ' ';
    return result;
}

GameVideo parseGameVideo(const fs::path& path, uintmax_t size)
{
    if (!size || size > 256 * 1024) return {};
    std::ifstream file(path, std::ios::binary);
    std::vector<BYTE> content(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(content.data()), content.size()) ||
        file.peek() != std::char_traits<char>::eof())
        return {};

    ComPtr<IStream> stream;
    stream.Attach(SHCreateMemStream(content.data(), static_cast<UINT>(content.size())));
    ComPtr<IXmlReader> reader;
    if (!stream || FAILED(CreateXmlReader(IID_PPV_ARGS(reader.GetAddressOf()), nullptr)) ||
        FAILED(reader->SetProperty(XmlReaderProperty_DtdProcessing, DtdProcessing_Prohibit)) ||
        FAILED(reader->SetInput(stream.Get())))
        return {};

    GameVideo options;
    bool root = false;
    bool video = false;
    XmlNodeType node{};
    HRESULT step;
    while ((step = reader->Read(&node)) == S_OK) {
        if (node != XmlNodeType_Element && node != XmlNodeType_EndElement) continue;
        UINT depth = 0;
        const wchar_t* name = nullptr;
        UINT length = 0;
        if (FAILED(reader->GetDepth(&depth)) || FAILED(reader->GetLocalName(&name, &length))) return {};
        const std::wstring_view element(name, length);
        if (node == XmlNodeType_EndElement) {
            if (depth == 2) video = false;
            continue;
        }
        if (depth == 0) root = element == L"EngineOptionSave";
        if (depth == 2) video = element == L"EngineOptionVideo";
        if (!root || !video || depth != 3) continue;

        const auto key = xmlAttribute(*reader.Get(), L"Name");
        for (size_t i = 0; i < videoOptions.size(); ++i) {
            if (key != utf8(videoOptions[i].first)) continue;
            options.values[i] = xmlAttribute(*reader.Get(), L"_value");
            if (options.values[i].empty()) options.values[i] = xmlAttribute(*reader.Get(), L"_select");
            break;
        }
    }
    if (step != S_FALSE || !root) return {};
    options.read = true;
    return options;
}

void refreshGameVideo(GraphicsDiagnostics& state)
{
    if (state.videoSettings.empty()) return;
    std::error_code error;
    const auto modified = fs::last_write_time(state.videoSettings, error);
    const auto size = error ? 0 : fs::file_size(state.videoSettings, error);
    if (error) {
        state.video = {};
        return;
    }
    if (state.video.read && modified == state.videoModified && size == state.videoSize) return;
    state.video = parseGameVideo(state.videoSettings, size);
    state.videoModified = modified;
    state.videoSize = size;
}

std::string versionWords(uint64_t version)
{
    std::ostringstream result;
    result << (version >> 48) << '.' << ((version >> 32) & 0xffff)
           << '.' << ((version >> 16) & 0xffff) << '.' << (version & 0xffff);
    return result.str();
}

std::string fileVersion(HMODULE module)
{
    std::wstring path(32768, L'\0');
    const DWORD length = GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
    if (!length || length == path.size()) return {};
    path.resize(length);
    DWORD unused = 0;
    const DWORD size = GetFileVersionInfoSizeW(path.c_str(), &unused);
    if (!size || size > 1024 * 1024) return {};
    std::vector<BYTE> buffer(size);
    if (!GetFileVersionInfoW(path.c_str(), 0, size, buffer.data())) return {};

    void* value = nullptr;
    UINT valueSize = 0;
    if (!VerQueryValueW(buffer.data(), L"\\", &value, &valueSize) || valueSize < sizeof(VS_FIXEDFILEINFO))
        return {};
    const auto& info = *static_cast<const VS_FIXEDFILEINFO*>(value);
    if (info.dwSignature != VS_FFI_SIGNATURE) return {};
    const uint64_t version = (uint64_t(info.dwFileVersionMS) << 32) | info.dwFileVersionLS;
    return version ? versionWords(version) : std::string{};
}

std::string renderingAdapter(LUID luid)
{
    // Resolve the observed adapter on the diagnostics thread. No DXGI factory or
    // rendering device is created by diagnostics.
    const auto library = LoadLibraryExW(L"dxcore.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!library) return {};
    using CreateAdapterFactory = HRESULT (WINAPI*)(REFIID, void**);
    const auto create = reinterpret_cast<CreateAdapterFactory>(
        GetProcAddress(library, "DXCoreCreateAdapterFactory"));
    std::string result;
    if (create) {
        ComPtr<IDXCoreAdapterFactory> factory;
        ComPtr<IDXCoreAdapter> adapter;
        if (SUCCEEDED(create(IID_PPV_ARGS(factory.GetAddressOf()))) && factory &&
            SUCCEEDED(factory->GetAdapterByLuid(luid, IID_PPV_ARGS(adapter.GetAddressOf()))) && adapter) {
            size_t bytes = 0;
            if (SUCCEEDED(adapter->GetPropertySize(DXCoreAdapterProperty::DriverDescription, &bytes)) &&
                bytes && bytes <= 1024) {
                std::string description(bytes, '\0');
                if (SUCCEEDED(adapter->GetProperty(DXCoreAdapterProperty::DriverDescription, bytes, description.data())))
                    result = description.substr(0, description.find('\0'));
            }
            uint64_t driver = 0;
            if (SUCCEEDED(adapter->GetProperty(DXCoreAdapterProperty::DriverVersion, &driver)) && driver)
                result += (result.empty() ? "Windows driver " : " / Windows driver ") + versionWords(driver);
        }
    }
    FreeLibrary(library);
    return result;
}

void appendDisplayInfo(std::ostream& out)
{
    out << "Windows display adapters: ";
    std::vector<std::wstring> names;
    DISPLAY_DEVICEW display{};
    display.cb = sizeof display;
    for (DWORD i = 0; EnumDisplayDevicesW(nullptr, i, &display, 0); ++i) {
        std::wstring name(display.DeviceString);
        if (!name.empty() && std::find(names.begin(), names.end(), name) == names.end())
            names.push_back(std::move(name));
    }
    for (size_t i = 0; i < names.size(); ++i) out << (i ? ", " : "") << utf8(names[i]);
    if (names.empty()) out << "Unavailable";
    out << '\n';

    DEVMODEW mode{};
    mode.dmSize = sizeof mode;
    out << "Primary desktop mode: ";
    if (EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &mode)) {
        out << mode.dmPelsWidth << " x " << mode.dmPelsHeight;
        if (mode.dmDisplayFrequency > 1) out << " @ " << mode.dmDisplayFrequency << " Hz";
    } else {
        out << "Unavailable";
    }
    out << '\n';
}

void appendLoadedModules(std::ostream& out, GraphicsDiagnostics& state)
{
    out << "Loaded graphics libraries (presence alone does not establish enablement):\n";
    bool loaded = false;
    for (size_t i = 0; i < graphicsLibraries.size(); ++i) {
        const auto name = graphicsLibraries[i];
        auto& module = state.modules[i];
        HMODULE current = nullptr;
        if (!GetModuleHandleExW(0, name, &current)) {
            module.handle = nullptr;
            module.version.clear();
            continue;
        }
        if (current != module.handle) {
            module.version = fileVersion(current);
            module.handle = current;
        }
        FreeLibrary(current);
        out << "  " << utf8(name) << " / "
            << (module.version.empty() ? "file version unavailable" : module.version) << '\n';
        loaded = true;
    }
    if (!loaded) out << "  None observed\n";
}
}

void setGraphicsSettingsPath(const std::filesystem::path& path)
{
    auto& state = diagnostics();
    const std::lock_guard<std::mutex> lock(state.reportMutex);
    state.videoSettings = path;
    state.video = {};
}

void observeGraphicsSwapchain(unsigned width, unsigned height, const LUID* adapter)
{
    if (width < 256 || height < 256) return;
    auto& state = diagnostics();
    const std::lock_guard<std::mutex> lock(state.swapchainMutex);
    state.swapchain.width = width;
    state.swapchain.height = height;
    if (adapter) {
        state.swapchain.adapter = *adapter;
        state.swapchain.hasAdapter = true;
    }
}

std::string graphicsSummary(bool includeLibraries)
{
    auto& state = diagnostics();
    const std::lock_guard<std::mutex> lock(state.reportMutex);
    Swapchain observed;
    {
        const std::lock_guard<std::mutex> swapchainLock(state.swapchainMutex);
        observed = state.swapchain;
    }

    auto& last = state.adapterObservation;
    if (observed.hasAdapter && (!last.hasAdapter || observed.adapter.HighPart != last.adapter.HighPart ||
        observed.adapter.LowPart != last.adapter.LowPart)) {
        state.renderingAdapter = renderingAdapter(observed.adapter);
        last = observed;
    }

    std::ostringstream out;
    out << "Graphics\n";
    appendDisplayInfo(out);
    out << "D3D12 rendering adapter: ";
    if (!observed.hasAdapter) out << "Not observed yet";
    else if (state.renderingAdapter.empty()) out << "Game adapter observed, details unavailable";
    else out << state.renderingAdapter;
    out << "\nGame swapchain size: ";
    if (observed.width && observed.height) out << observed.width << " x " << observed.height;
    else out << "Not observed yet";
    out << '\n';

    refreshGameVideo(state);
    for (size_t i = 0; i < videoOptions.size(); ++i) {
        const auto& value = state.video.values[i];
        out << videoOptions[i].second << " (saved game setting): ";
        if (!value.empty()) out << value;
        else out << (state.video.read ? "Not recorded (game default may apply)" : "Settings unavailable");
        out << '\n';
    }
    if (includeLibraries) appendLoadedModules(out, state);
    return out.str();
}
}
