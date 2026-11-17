#pragma once
#include "wardrobe_selection.h"
#include "mounted_dependencies.h"
namespace hex {
struct OutfitVisibilityPolicy;
bool ownedOutfitVisibility(uintptr_t actor,int witch,OutfitVisibilityPolicy& policy);
struct WardrobeEditorStatus {
    WardrobeCapabilities available;
    WardrobeChoice selected;
    bool mixedOutfit=false;
    std::string detail,caption="Outfit unavailable";
    bool warning=true;
};
bool initializeNativeWardrobe(std::string& error);
void reportWardrobeUnavailable(const std::string& error);
void tickNativeWardrobe(uintptr_t actor,int witch);
WardrobeEditorStatus wardrobeEditorStatus(int target);
bool changeWardrobeChoice(int target,WardrobeField field,int value,std::string& error);
bool requestSelectedWardrobeRefresh(int witch,const std::string& outfit,std::string& error);
}
