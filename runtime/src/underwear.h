#pragma once
#include <Windows.h>
#include <string>
#include <filesystem>
#include "pose_access.h"
#include "outfit_visibility.h"
namespace hex {
struct UnderwearStatus {bool ready=false,hidden=false,mixed=false,pending=true;std::string detail;};
struct UnderwearPreference {uint32_t hiddenMask=0;uint64_t revision=0;};
struct UnderwearOwnership {bool delegated=false,fullyExternal=false,meshMissing=false;std::string source,meshSource;};
UnderwearPreference baseUnderwearPreference();
UnderwearOwnership underwearOwnership(int target);
bool setExternalUnderwear(bool external,std::string& error);
UnderwearPreference outfitHiddenPreference();
UnderwearStatus outfitHiddenStatus(int target);
bool setOutfitHidden(int target,bool hidden,std::string& error);
void setOutfitHiddenActor(uintptr_t actor,uintptr_t renderer,int witch,bool hidden,bool confirmed,bool basePresent=false,bool emptyProfile=false);
using WardrobeActorObserved=void(*)(void*);
bool initializeUnderwear(WardrobeActorObserved observe,std::string& error);
UnderwearStatus underwearStatus(int target);
bool setUnderwearHidden(int target,bool hidden,std::string& error);
void updateUnderwear(void* actor,const ActorIdentity& identity,uintptr_t checkedRenderer=0);
bool restoreOutfitVisibility(void* actor);
bool readOutfitPartVisibility(uintptr_t renderer,uint32_t key,bool& visible);
bool writeOutfitPartVisibility(uintptr_t renderer,uint32_t key,bool visible);
#ifdef HEX_UNDERWEAR_TESTS
bool runUnderwearPolicyTests(const std::filesystem::path& profile,std::string& error);
#endif
}
