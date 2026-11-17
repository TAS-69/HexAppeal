#pragma once
#include "wardrobe_selection.h"
#include "mounted_dependencies.h"
namespace hex {
struct OutfitVisibilityPolicy;
bool ownedOutfitVisibility(uintptr_t actor,int witch,OutfitVisibilityPolicy& policy);
struct WardrobeEditorStatus {
    WardrobeCapabilities available;
    WardrobeChoice selected;
    bool mixedOutfit=false,mixedUnderwear=false,mixedDancer=false,resourceFilesReady=false;
    std::string detail,caption="Outfit unavailable",dependencyDetail,versionWarning;
    bool warning=true;
};
bool initializeNativeWardrobe(std::string& error);
void reportWardrobeUnavailable(const std::string& error);
void refreshWardrobeResources(const DependencyStatus& dependencies);
void tickNativeWardrobe(uintptr_t actor,int witch);
WardrobeEditorStatus wardrobeEditorStatus(int target);
bool changeWardrobeChoice(int target,WardrobeField field,int value,std::string& error);
struct GlobalDancerColourStatus {bool ready=false;int selected=-1,startup=-1;std::string detail,versionWarning;};
GlobalDancerColourStatus globalDancerColourStatus();
bool changeGlobalDancerColour(int colour,std::string& error);
bool requestSelectedWardrobeRefresh(int witch,const std::string& outfit,std::string& error);
}
