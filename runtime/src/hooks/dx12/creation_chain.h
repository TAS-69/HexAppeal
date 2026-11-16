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

inline bool Read(const void* source, void* destination, size_t size) {
    __try { std::memcpy(destination, source, size); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}


inline EntryObservation Observe(void* entry, void* ours) {
    void* visited[16]{};
    void* at = entry;
    for (unsigned hop = 0; hop < 16; ++hop) {
        if (at == ours && at) return {EntryState::Ours, at};
        for (unsigned i = 0; i < hop; ++i)
            if (visited[i] == at) return {EntryState::Cycle, at};
        visited[hop] = at;
        uint8_t code[16]{};
        if (!at || !Read(at, code, sizeof code)) return {EntryState::Unreadable, at};
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
}
