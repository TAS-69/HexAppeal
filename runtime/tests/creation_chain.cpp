#include "hooks/dx12/creation_chain.h"
#include "hooks/dx12/factory_imports.h"
#include <array>
#include <cstdio>
#include <cstdlib>

static void Check(bool condition, const char* name) {
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", name); std::exit(1); }
}
static void Absolute(unsigned char* from, const void* to) {
    std::memset(from, 0x90, 16);
    from[0] = 0xFF; from[1] = 0x25;
    std::memset(from + 2, 0, 4);
    std::memcpy(from + 6, &to, sizeof to);
}
static void CheckFactoryNesting(unsigned level) {
    const hex::creation::FactoryCall call(reinterpret_cast<HWND>(uintptr_t{level}));
    Check(bool(call) == (level <= 8), "factory call nesting remains bounded across different windows");
    if (call) CheckFactoryNesting(level + 1);
}
static void CheckFactoryImports() {
    alignas(void*) std::array<unsigned char, 1024> image{};
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(image.data());
    dos->e_magic = IMAGE_DOS_SIGNATURE;
    dos->e_lfanew = sizeof(*dos);
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(image.data() + dos->e_lfanew);
    nt->Signature = IMAGE_NT_SIGNATURE;
    nt->FileHeader.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER64);
    nt->OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
    nt->OptionalHeader.NumberOfRvaAndSizes = IMAGE_NUMBEROF_DIRECTORY_ENTRIES;
    nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT] = {400, 2 * sizeof(IMAGE_IMPORT_DESCRIPTOR)};
    auto* descriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(image.data() + 400);
    descriptor->Name = 480;
    descriptor->OriginalFirstThunk = 512;
    descriptor->FirstThunk = 600;
    std::memcpy(image.data() + descriptor->Name, "DXGI.DLL", sizeof("DXGI.DLL"));
    auto* names = reinterpret_cast<IMAGE_THUNK_DATA64*>(image.data() + descriptor->OriginalFirstThunk);
    names[0].u1.Ordinal = IMAGE_ORDINAL_FLAG64 | 1;
    names[1].u1.AddressOfData = 720;
    std::memcpy(image.data() + 720 + sizeof(WORD), "CreateDXGIFactory2", sizeof("CreateDXGIFactory2"));
    auto** slots = reinterpret_cast<void**>(image.data() + descriptor->FirstThunk);
    slots[1] = reinterpret_cast<void*>(uintptr_t{1234});
    const auto before = image;
    using hex::creation::FindFactoryImport;
    Check(FindFactoryImport(image.data(), image.size(), "CreateDXGIFactory2") == &slots[1], "factory entry is resolved in the caller's import table");
    Check(!FindFactoryImport(image.data(), image.size(), "CreateDXGIFactory"), "similar type names do not match");
    Check(!FindFactoryImport(image.data(), 726, "CreateDXGIFactory2"), "an incomplete import name is rejected");
    Check(image == before, "factory import discovery is read-only");
    std::memcpy(image.data() + descriptor->Name, "sl.interposer.dll", sizeof("sl.interposer.dll"));
    Check(hex::creation::FindNamedImport(image.data(), image.size(), "sl.interposer.dll", "CreateDXGIFactory2") == &slots[1],
        "a Streamline factory export is resolved from its own import table");
    Check(!FindFactoryImport(image.data(), image.size(), "CreateDXGIFactory2"),
        "a Streamline import cannot alias the native DXGI continuation");
    std::memcpy(image.data() + descriptor->Name, "libxess_fg.dll", sizeof("libxess_fg.dll"));
    std::memcpy(image.data() + 720 + sizeof(WORD), "xefgSwapChainD3D12GetSwapChainPtr", sizeof("xefgSwapChainD3D12GetSwapChainPtr"));
    Check(hex::creation::FindNamedImport(image.data(), image.size(), "libxess_fg.dll", "xefgSwapChainD3D12GetSwapChainPtr") == &slots[1],
        "the game can receive a swapchain through its XeSS-FG import");
    std::memcpy(image.data() + descriptor->Name, "KERNEL32.dll", sizeof("KERNEL32.dll"));
    std::memcpy(image.data() + 720 + sizeof(WORD), "GetProcAddress", sizeof("GetProcAddress"));
    Check(hex::creation::FindNamedImport(image.data(), image.size(), "kernel32.dll", "GetProcAddress") == &slots[1],
        "dynamic factory lookup only uses the game's own named import");
    Check(!hex::creation::FindNamedImport(image.data(), image.size(), "kernelbase.dll", "GetProcAddress"),
        "similarly named loader imports from a different library are ignored");
    descriptor->OriginalFirstThunk = 0;
    Check(!hex::creation::FindNamedImport(image.data(), image.size(), "kernel32.dll", "GetProcAddress"),
        "an import without its name table remains untouched");
}
int main() {
    using namespace hex::creation;
    CheckFactoryImports();
    const auto gameWindow = reinterpret_cast<HWND>(uintptr_t{1});
    const auto probeWindow = reinterpret_cast<HWND>(uintptr_t{2});
    Check(!FactoryCall::Active(), "a provider can attach when no factory wrapper is active");
    {
        const FactoryCall creation(gameWindow);
        Check(creation && !creation.nested(), "first factory call enters");
        Check(FactoryCall::Active(), "a provider knows to defer attachment to an enclosing factory wrapper");
        const FactoryCall delegation(probeWindow);
        Check(delegation && delegation.nested(), "another window can create a nested swapchain");
        const FactoryCall repeated(gameWindow);
        Check(!repeated, "factory dispatch cannot re-enter for the original window");
    }
    Check(!FactoryCall::Active(), "factory dispatch releases attachment ownership after return");
    CheckFactoryNesting(1);
    const FactoryCall nextCreation(gameWindow);
    Check(nextCreation && !nextCreation.nested(), "later swapchain creation on the same window can enter");

    auto* page = static_cast<unsigned char*>(VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Check(page != nullptr, "allocate fixture");
    std::memset(page, 0x90, 4096);
    void* ours = page + 512;
    Absolute(page, page + 64);
    Absolute(page + 64, ours);
    Check(Observe(page, ours).state == EntryState::Ours, "ours through another relay");
    DWORD savedProtect{};
    Check(!ExecutableAddress(page)&&!ExecutableAddress(nullptr),"unexecutable proxy entries are refused");
    Check(VirtualProtect(page,4096,PAGE_EXECUTE_READ,&savedProtect)!=FALSE,"enable checked proxy fixture");
    Check(ExecutableAddress(page)&&ExecutableAddress(ours),"executable proxy entry and destination are accepted");
    Check(VirtualProtect(page,4096,savedProtect,&savedProtect)!=FALSE,"restore checked proxy fixture");



    Absolute(page + 64, page + 128);
    std::array<unsigned char, 4096> before{};
    std::memcpy(before.data(), page, before.size());
    DWORD old{};
    Check(VirtualProtect(page, 4096, PAGE_READONLY, &old) != FALSE, "make foreign relays read-only");
    for (int i = 0; i < 10000; ++i) {
        const auto observation = Observe(page, ours);
        Check(observation.state == EntryState::Other && observation.destination == page + 128, "foreign detour remains first");
    }
    Check(!std::memcmp(before.data(), page, before.size()), "no relay or entry rewrite");
    Check(VirtualProtect(page, 4096, PAGE_READWRITE, &old) != FALSE, "restore relay fixture");

    Absolute(page + 128, page + 64);
    Check(Observe(page, ours).state == EntryState::Cycle, "foreign two-node cycle is bounded");
    Absolute(page + 64, page + 64);
    Check(Observe(page, ours).state == EntryState::Cycle, "self cycle is bounded");
    for (int i = 0; i < 18; ++i) Absolute(page + i * 32, page + (i + 1) * 32);
    Check(Observe(page, page + 2048).state == EntryState::HopLimit, "long chain is bounded");

    std::memset(page, 0x90, 4096);
    page[0] = 0xE9; int32_t relative = 64 - 5; std::memcpy(page + 1, &relative, 4);
    page[64] = 0xEB; page[65] = 30;
    page[96] = 0x48; page[97] = 0xB8; std::memcpy(page + 98, &ours, sizeof ours);
    page[106] = 0xFF; page[107] = 0xE0;
    Check(Observe(page, ours).state == EntryState::Ours, "relative short and absolute register jumps");
    Check(Observe(nullptr, ours).state == EntryState::Unreadable, "null entry fails closed");
    Check(Observe(reinterpret_cast<void*>(1), ours).state == EntryState::Unreadable, "invalid entry is contained");
    Check(VirtualProtect(page, 4096, PAGE_NOACCESS, &old) != FALSE, "protect fixture");
    Check(Observe(page, ours).state == EntryState::Unreadable, "freed or inaccessible relay is contained");
    Check(VirtualProtect(page, 4096, PAGE_READWRITE, &old) != FALSE, "restore fixture");
    Absolute(page, reinterpret_cast<void*>(1));
    Check(Observe(page, ours).state == EntryState::Unreadable, "unreadable indirect destination is contained");

    relative = 4096 - 6; std::memcpy(page + 2, &relative, 4);
    auto* guard = static_cast<unsigned char*>(VirtualAlloc(nullptr, 8192, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Check(guard != nullptr, "allocate guarded slot");
    Absolute(guard, ours);
    std::memcpy(guard + 2, &relative, 4);
    Check(VirtualProtect(guard + 4096, 4096, PAGE_NOACCESS, &old) != FALSE, "guard indirect slot");
    Check(Observe(guard, ours).state == EntryState::Unreadable, "unreadable indirect slot is contained");
    VirtualFree(guard, 0, MEM_RELEASE);
    VirtualFree(page, 0, MEM_RELEASE);
    // Startup probe swapchains race other mods' creation calls through the
    // Steam overlay's factory hook; the probe must stay clear of startup.
    const unsigned long long start = 1000000;
    const auto first = start + firstProbeDelayMs;
    Check(firstProbeDelayMs >= 30000, "the first presentation probe waits at least 30 seconds after load");
    Check(!PresentationProbeDue(start + 8100, first, false, false), "no probe swapchain at the old 8-second mark");
    Check(!PresentationProbeDue(first, first, false, true), "no probe swapchain once the game's swapchain was seen");
    Check(PresentationProbeDue(first, first, false, false), "a probe remains available when creation was never seen");
    Check(!PresentationProbeDue(first + probeRetryMs, first, true, false), "a successful probe is not repeated");
    std::puts("PASS: caller-local DXGI import discovery, bounded factory re-entry, and read-only inspection of invalid or cyclic relays.");
    std::puts("PASS: the presentation probe stays out of startup and is skipped once the game's swapchain was seen.");
}
