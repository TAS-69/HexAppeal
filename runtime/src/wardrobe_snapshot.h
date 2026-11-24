#pragma once
#include <array>
#include <cstdint>
#include <string>

namespace hex {
inline bool wardrobeActorRecentlySeen(uint64_t now,uint64_t observed){
    return observed&&now>=observed&&now-observed<2000;
}
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
struct WardrobeInstance {
    uintptr_t actor=0,control=0,renderer=0,node=0;
    int witch=-1;
};
struct WardrobeVisibilityClaim {
    WardrobeInstance instance;
    uint32_t key=0;
    uintptr_t record=0,wrapper=0,scene=0;
    std::string prefab;
};
bool operator==(const WardrobeVisibilityClaim& first,const WardrobeVisibilityClaim& second);
WardrobeVisibilityClaim wardrobeVisibilityClaim(const WardrobeSnapshot& snapshot,const WardrobeClaim& claim);
bool hasWardrobeVisibilityClaim(const WardrobeSnapshot& snapshot,const WardrobeVisibilityClaim& mask);
bool rebindAddonVisibilityClaim(const WardrobeSnapshot& snapshot,WardrobeVisibilityClaim& mask);
int wardrobeWitchForAppearance(const char* path);
uintptr_t readWardrobeActorAppearance(uintptr_t actor,uintptr_t gameBase,char (&path)[256]);
int wardrobeWitchForActorAppearance(uintptr_t actor,uintptr_t gameBase);
bool readWardrobeCharacterRecord(uintptr_t actor,uintptr_t gameBase,uintptr_t characterInfo,char (&name)[96]);
WardrobePartKind wardrobePartKind(int witch,const char* prefab);
bool defaultOutfitPart(int witch,const char* prefab);
uintptr_t checkedWardrobeRenderer(uintptr_t actor,int witch,uintptr_t gameBase,const char* addonAppearance=nullptr);
bool matchesWardrobeAppearance(uintptr_t actor,uintptr_t gameBase,const char* appearance);
bool captureWardrobeSnapshot(uintptr_t actor,int witch,uintptr_t gameBase,WardrobeSnapshot& output,
    const char* addonAppearance=nullptr,const char** issue=nullptr,bool verifiedRecord=false);
bool canDiscardWardrobeInstance(const WardrobeInstance& instance,uintptr_t gameBase,uint64_t inactiveFor,bool hasOutfitState);
bool captureBarberMeshChoices(const WardrobeSnapshot& snapshot,uintptr_t gameBase,std::array<uint8_t,16>& choices);
}
