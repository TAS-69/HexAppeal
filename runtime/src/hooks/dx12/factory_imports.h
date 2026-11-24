#pragma once

#include <Windows.h>
#include <cstddef>
#include <cstring>

namespace hex::creation {
inline void** FindNamedImport(void* image, size_t size, const char* library, const char* function) {
    auto* base = static_cast<unsigned char*>(image);
    const auto contains = [size](size_t offset, size_t bytes) {
        return offset <= size && bytes <= size - offset;
    };
    if (!base || !contains(0, sizeof(IMAGE_DOS_HEADER))) return nullptr;
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 ||
        !contains(dos->e_lfanew, sizeof(IMAGE_NT_HEADERS64))) return nullptr;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
        nt->FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64) ||
        nt->OptionalHeader.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_IMPORT) return nullptr;
    const auto& imports = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!imports.VirtualAddress || !contains(imports.VirtualAddress, imports.Size)) return nullptr;

    const auto librarySize = std::strlen(library) + 1;
    const auto nameSize = std::strlen(function) + 1;
    const auto* descriptors = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(base + imports.VirtualAddress);
    for (size_t index = 0; index < imports.Size / sizeof(*descriptors); ++index) {
        const auto& import = descriptors[index];
        if (!import.Name) break;
        if (!contains(import.Name, librarySize) ||
            _strnicmp(reinterpret_cast<const char*>(base + import.Name), library, librarySize) != 0 ||
            !import.OriginalFirstThunk || !import.FirstThunk) continue;
        for (size_t nameAt = import.OriginalFirstThunk, slotAt = import.FirstThunk;
             contains(nameAt, sizeof(IMAGE_THUNK_DATA64)) && contains(slotAt, sizeof(void*));
             nameAt += sizeof(IMAGE_THUNK_DATA64), slotAt += sizeof(void*)) {
            const auto value = reinterpret_cast<const IMAGE_THUNK_DATA64*>(base + nameAt)->u1.AddressOfData;
            if (!value) break;
            if (IMAGE_SNAP_BY_ORDINAL64(value) || !contains(value, sizeof(WORD) + nameSize)) continue;
            if (std::memcmp(base + value + sizeof(WORD), function, nameSize) == 0)
                return reinterpret_cast<void**>(base + slotAt);
        }
    }
    return nullptr;
}

inline void** FindFactoryImport(void* image, size_t size, const char* function) {
    return FindNamedImport(image, size, "dxgi.dll", function);
}
}
