#pragma once
#include <array>
#include <cstdint>

namespace hex {
enum class WardrobePartKind { Other, DefaultGarment, Underwear };
struct WardrobeClaim {
    uint32_t key=0;
    uintptr_t record=0,wrapper=0,scene=0;
    char prefab[192]{};
    WardrobePartKind kind=WardrobePartKind::Other;
};
struct WardrobeSnapshot {
    int witch=-1;
    uintptr_t actor=0,control=0,renderer=0,skinned=0,node=0,pathWrapper=0,claimData=0;
    uint32_t claimCount=0,readyCount=0;
    char appearance[256]{};
    std::array<WardrobeClaim,64> claims{};
    bool defaultGarment=false,customGarment=false;
};
int wardrobeWitchForAppearance(const char* path);
WardrobePartKind wardrobePartKind(int witch,const char* prefab);
bool captureWardrobeSnapshot(uintptr_t actor,int witch,uintptr_t gameBase,WardrobeSnapshot& output);
}
