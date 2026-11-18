#pragma once
#include "../sdk/HexAppealWardrobe.h"
#include <filesystem>
#include <array>
#include <string>
#include <vector>

namespace hex {
struct WardrobeOption {std::string id,label,prefab;};
struct WardrobeControl {std::string id,label,help;uint32_t type=0,scope=0,flags=0,defaultValue=0;std::vector<WardrobeOption> options;};
struct WardrobePrefabState {std::array<uint32_t,HA_WARDROBE_MAX_CONTROLS> values{};std::string prefab;};
struct WardrobeVisibilityPart {uint32_t control=0;std::string part;bool visible=true;};
struct WardrobeProviderOutfit {uint64_t token=0;std::string id,label,prefab,credit,requirements,reason;bool available=false,removed=false;std::vector<WardrobeControl> controls;uint32_t nativeMask=0,policyFlags=0;std::vector<WardrobePrefabState> prefabStates;std::vector<WardrobeVisibilityPart> visibilityParts;};
bool initializeWardrobeAPI(const std::filesystem::path& settings,std::string& error);
std::vector<WardrobeProviderOutfit> wardrobeProviderOutfits();
bool resolveWardrobeProvider(const std::string& id,int witch,WardrobeProviderOutfit& output,std::string& error);
bool readWardrobeControl(int witch,const std::string& outfit,const std::string& control,uint32_t& value,uint64_t& revision);
bool writeWardrobeControl(int witch,const std::string& outfit,const std::string& control,uint32_t value,std::string& error);
bool wardrobeControlPinned(const std::string& outfit,const std::string& control);
bool pinWardrobeControl(const std::string& outfit,const std::string& control,bool pinned,std::string& error);
bool requestWardrobeOutfit(int witch,const std::string& id,std::string& error);
bool nativeWardrobeSelection(int witch,std::string& id,std::string& prefab,bool& available,uint64_t& revision);
void reapplyWardrobeProviderControls(int witch,const std::string& id);
}
