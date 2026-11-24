#pragma once
#include "wardrobe_slots.h"
#include "model.h"
#include <vector>

namespace hex {
inline constexpr int firstAddonCharacter=6;
inline constexpr size_t maximumAddonCharacters=24;
struct CharacterAddonItem {
    std::string label,slot,prefab;
};
struct CharacterAddon {
    std::string id,label,record,appearance;
    uint32_t recordKey=0;
    size_t pack=0;
    OutfitSlots defaults;
    std::vector<CharacterAddonItem> items;
};
struct CharacterAddonPack {
    std::string id,label,version,minimumVersion,detail;
    size_t accepted=0,declared=0;
};
struct CharacterAddonScan {
    std::vector<CharacterAddon> characters;
    std::vector<CharacterAddonPack> packs;
    std::vector<std::string> messages;
};
CharacterAddonScan scanCharacterAddons(const std::filesystem::path& folder,const std::filesystem::path& dmmManifest={},
    std::string_view coreVersion={});
void initializeCharacterAddons(const std::filesystem::path& settings,const std::filesystem::path& dmmManifest={});
const std::vector<CharacterAddon>& characterAddons();
const CharacterAddon* characterAddon(int target);
const std::vector<CharacterAddonPack>& characterAddonPacks();
bool characterAddonPackEnabled(size_t pack);
bool setCharacterAddonPackEnabled(size_t pack,bool enabled,std::string& error);
bool characterAddonEnabled(int target);
std::vector<int> enabledAddonTargets();
int enabledAddonTargetForAppearance(const char* appearance);
void recordAddonVenusBody(int target,bool hasBaseUnderwear);
bool addonVenusBodyActive(int target);
bool addonVenusUnderwearPresent(int target);
bool addonUnderwearHidden(int target);
bool saveAddonUnderwearHidden(int target,bool hidden,std::string& error);
struct AddonBodyProfile {
    Profile profile;
    uint64_t revision=0;
};
AddonBodyProfile addonBodyProfile(int target);
bool saveAddonBodyProfile(int target,const Profile& profile,std::string& error);
void reportAddonShape(int target,uint64_t revision,std::string detail);
std::string addonShapeDetail(int target);
int addonTargetForRecord(std::string_view record);
}
