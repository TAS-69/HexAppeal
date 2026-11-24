#pragma once
#include <Windows.h>
#include <atomic>
#include <cstdint>
#include <string_view>

namespace hex {
struct GameLayout {
    uint32_t timeStamp,imageSize,checkSum;
    const char* digest;
    uintptr_t frameDispatch,visibilityObserver,visibilitySet,visibilityGet,internString;
    uintptr_t rebuildAppearance,mergeAppearance,copyPart,releaseWrapper,destroyRecord,copyRecord,applyAppearance;
    uintptr_t actor,control,skinned,skinGlobal,node,controller,loaderGlobal,appearance,emptyWrapper,emptyRecord;
    uintptr_t actorManagerGlobal=0,actorManager=0,actorContainer=0,playerActor=0;
    uintptr_t frameCalls[2]{};
    uintptr_t hudCall=0,hudSubmit=0;
    uintptr_t characterInfo=0;
};

inline constexpr GameLayout layouts[]={
    {0x6af5d198,0x144e9000,0x13ff2bd0,"8be67178d9b9a149c361a731051665e0ba4d441e250ae1dfe645bd6e524345c8",
     0x414c490,0xb9bf80,0x3043cf0,0x3043da0,0x14de770,
     0x82d7f0,0x2f94ce0,0x5477a0,0x3e6f50,0x536660,0x549710,0x2f8c200,
     0x5991108,0x59c1660,0x5f850f0,0x7246a40,0x5f7be58,0x59a9548,0x7160288,0x5dff260,0x6f29f48,0x6f14108,
     0,0,0,0,{0xb4a0c9,0xb4b747},0x390610e,0x42811b0},
    {0x6afdc0cf,0x13c7e000,0x13787b92,"dfcf5c507d09aa0e09cf9766db866fd1e7cf513b9dd7fdc72a8f121ba2d9a9d5",
     0x4150980,0xb9c9d0,0x3048240,0x30482f0,0x14e02f0,
     0x82df10,0x2f99230,0x547bb0,0x3e7360,0x536a70,0x549b20,0x2f90750,
     0x5995cb8,0x59c6748,0x5f8a010,0x724cbe0,0x5f80df0,0x59ae640,0x7166348,0x5e047f8,0x6f2ff48,0x6f1a108,
     0x724be30,0x59c7a08,0x598cb20,0x59961d0,{0xb4aa09,0xb4c087},0x390a65e,0x4285750,0x724c4b0}
};

constexpr const GameLayout* findGameLayout(uint32_t stamp,uint32_t size,uint32_t checksum){
    for(const auto& layout:layouts)if(layout.timeStamp==stamp&&layout.imageSize==size&&layout.checkSum==checksum)return &layout;
    return nullptr;
}

// Offline tests run outside the game and select a checked layout here.
// The game never sets it.
inline const GameLayout*& testGameLayout(){static const GameLayout* layout=nullptr;return layout;}

// A build missing from the table can still be served by code-signature
// lookup (game_signatures.cpp). Its layout is published here once found.
inline std::atomic<const GameLayout*>& signatureGameLayout(){static std::atomic<const GameLayout*> layout{nullptr};return layout;}
// False while signature lookup runs; background workers wait for it.
inline std::atomic<bool>& gameLayoutSettled(){static std::atomic<bool> settled{true};return settled;}
// [Development] ForceSignatureLayout=1: ignore the table to test the lookup on a known build.
inline std::atomic<bool>& ignoreGameLayoutTable(){static std::atomic<bool> ignore{false};return ignore;}
inline void waitForGameLayout(){while(!gameLayoutSettled().load(std::memory_order_acquire))Sleep(50);}

inline const GameLayout* tableGameLayout(){
    static const auto* layout=[]() -> const GameLayout* {
        const auto* game=reinterpret_cast<const unsigned char*>(GetModuleHandleW(nullptr));
        if(!game)return nullptr;
        const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(game);
        if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<=0||dos->e_lfanew>0xf00)return nullptr;
        const auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(game+dos->e_lfanew);
        if(nt->Signature!=IMAGE_NT_SIGNATURE||nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64)return nullptr;
        return findGameLayout(nt->FileHeader.TimeDateStamp,nt->OptionalHeader.SizeOfImage,nt->OptionalHeader.CheckSum);
    }();
    return layout;
}

inline const GameLayout* loadedGameLayout(){
    if(const auto* selected=testGameLayout())return selected;
    if(!ignoreGameLayoutTable().load(std::memory_order_acquire))
        if(const auto* known=tableGameLayout())return known;
    return signatureGameLayout().load(std::memory_order_acquire);
}

// An unrecognised build gets an empty layout, never another build's
// addresses: every address is 0, so identity checks fail and callers that
// would call game code check loadedGameLayout() first.
inline constexpr GameLayout noGameLayout{};
inline const GameLayout& gameLayout(){
    const auto* layout=loadedGameLayout();
    return layout?*layout:noGameLayout;
}

inline bool checkedGameDigest(std::string_view digest){
    for(const auto& layout:layouts)if(digest==layout.digest)return true;
    return false;
}
}
