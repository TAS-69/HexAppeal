#pragma once

#include <Windows.h>
#include <cstdint>
#include <cstring>

namespace hex::creation {
enum class EntryState { Ours, Other, Unreadable, Cycle, HopLimit };
struct EntryObservation {
    EntryState state;
    void* destination;
};

class FactoryCall {
    inline static thread_local const FactoryCall* active = nullptr;
    const FactoryCall* previous = active;
    HWND window;
    bool entered = false;
public:
    explicit FactoryCall(HWND hwnd) : window(hwnd) {
        unsigned depth = 0;
        for (auto call = previous; call; call = call->previous) {
            if (call->window == window || ++depth == 8) return;
        }
        active = this;
        entered = true;
    }
    FactoryCall(const FactoryCall&) = delete;
    FactoryCall& operator=(const FactoryCall&) = delete;
    ~FactoryCall() { if (entered) active = previous; }
    static bool Active() { return active != nullptr; }
    explicit operator bool() const { return entered; }
    bool nested() const { return previous != nullptr; }
};

inline bool Read(const void* source, void* destination, size_t size) {
    __try { std::memcpy(destination, source, size); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}


inline bool ExecutableAddress(const void* address){
    MEMORY_BASIC_INFORMATION region{};
    if(!address||!VirtualQuery(address,&region,sizeof region)||region.State!=MEM_COMMIT||
       (region.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
    const auto protection=region.Protect&0xff;
    return protection==PAGE_EXECUTE||protection==PAGE_EXECUTE_READ||
           protection==PAGE_EXECUTE_READWRITE||protection==PAGE_EXECUTE_WRITECOPY;
}

inline EntryObservation Observe(void* entry, void* ours, bool executableOnly=false) {
    void* visited[16]{};
    void* at = entry;
    for (unsigned hop = 0; hop < 16; ++hop) {
        if (at == ours && at) return {EntryState::Ours, at};
        for (unsigned i = 0; i < hop; ++i)
            if (visited[i] == at) return {EntryState::Cycle, at};
        visited[hop] = at;
        uint8_t code[16]{};
        if ((executableOnly&&!ExecutableAddress(at))||!at||!Read(at, code, sizeof code)) return {EntryState::Unreadable, at};
        const uintptr_t address = reinterpret_cast<uintptr_t>(at);
        if (code[0] == 0xE9) {
            int32_t relative{}; std::memcpy(&relative, code + 1, sizeof relative);
            at = reinterpret_cast<void*>(address + 5 + static_cast<intptr_t>(relative));
        } else if (code[0] == 0xEB) {
            at = reinterpret_cast<void*>(address + 2 + static_cast<intptr_t>(static_cast<int8_t>(code[1])));
        } else if (code[0] == 0xFF && code[1] == 0x25) {
            int32_t relative{}; std::memcpy(&relative, code + 2, sizeof relative);
            if (!Read(reinterpret_cast<void*>(address + 6 + static_cast<intptr_t>(relative)), &at, sizeof at))
                return {EntryState::Unreadable, at};
        } else if (code[0] == 0x48 && code[1] == 0xB8 && code[10] == 0xFF && code[11] == 0xE0) {
            std::memcpy(&at, code + 2, sizeof at);
        } else {
            return {EntryState::Other, at};
        }
    }
    return at == ours && at ? EntryObservation{EntryState::Ours, at}
                           : EntryObservation{EntryState::HopLimit, at};
}

// A probe swapchain is a last resort for finding the game's presentation
// when its swapchain creation was never seen. Creating one while the game or
// other mods create theirs can make the Steam overlay's factory hook recurse
// until the game crashes during startup, so the first probe waits until
// startup is long over, and none is made once the game's swapchain was seen.
inline constexpr unsigned long long firstProbeDelayMs = 30000;
inline constexpr unsigned long long probeRetryMs = 15000;
inline bool PresentationProbeDue(unsigned long long now, unsigned long long nextProbe,
                                 bool probed, bool gameSwapChainSeen) {
    return !probed && !gameSwapChainSeen && now >= nextProbe;
}
}
