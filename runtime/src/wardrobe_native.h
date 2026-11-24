#pragma once
#include "wardrobe_selection.h"
#include "mounted_dependencies.h"
#include "wardrobe_slots.h"
#include "wardrobe_api.h"
#include <vector>
namespace hex {
struct OutfitVisibilityPolicy;
bool readOwnedOutfitVisibility(uintptr_t actor,int witch,OutfitVisibilityPolicy& policy);
struct WardrobeEditorStatus {
    WardrobeCapabilities available;
    WardrobeChoice selected;
    bool mixedOutfit=false;
    std::string detail,caption="Outfit unavailable";
    bool warning=true;
    SlotOutfit slotSelection=SlotOutfit::Game;
    bool barberOutfitAvailable=false,spouseCombatActive=false,savedOutfitAvailable=false,visibleSlots=false;
    OutfitSlots slots,savedSlots,draftSlots;
    bool editingSaved=false;
    std::set<std::string> hiddenSlots;
};
bool initializeNativeWardrobe(std::string& error);
void reportWardrobeUnavailable(const std::string& error);
void tickNativeWardrobePipeline();
void tickNativeWardrobe(uintptr_t actor,int witch,uint64_t observedAt);
void observeNativeWardrobeInteraction(uintptr_t actor,int witch) noexcept;
WardrobeEditorStatus wardrobeEditorStatus(int target);
bool changeWardrobeChoice(int target,WardrobeField field,int value,std::string& error);
bool requestSelectedWardrobeRefresh(int witch,const std::string& outfit,std::string& error);
bool openSavedWardrobe(int witch,std::string& error);
bool applySavedWardrobe(int witch,std::string& error);
bool clearSavedWardrobeDraft(int witch,std::string& error);
void cancelSavedWardrobe(int witch);
bool requestSlotOutfit(int witch,SlotOutfit outfit,std::string& error);
bool equipWardrobeItem(int witch,const std::string& slot,const OutfitParts& parts,std::string& error);
bool setWardrobeSlotHidden(int witch,const std::string& slot,bool hidden,std::string& error);
bool unequipWardrobeSlot(int target,const std::string& slot,std::string& error);
std::vector<WardrobeItemControl> equippedWardrobeItemControls(int witch,bool preview);
bool setEquippedWardrobeItemControl(int witch,bool preview,const std::string& outfit,const std::string& control,uint32_t value,std::string& error);
struct AddonWardrobeStatus {
    std::string detail;
    OutfitSlots slots,savedSlots,draftSlots;
    std::set<std::string> hiddenSlots;
    SlotOutfit selected=SlotOutfit::Game;
    bool canEdit=false,editing=false,hidden=false,hasSaved=false;
    bool canSwitch=false,underwearPresent=false,actorSeen=false,externalUnderwear=false;
};
void tickAddonWardrobe(uintptr_t actor,int target,uint64_t observedAt);
AddonWardrobeStatus addonWardrobeStatus(int target);
bool selectAddonOutfit(int target,SlotOutfit outfit,std::string& error);
#ifdef HEX_MENU_TESTS
bool verifyAddonFollowGame(const std::filesystem::path& folder,std::string& error,bool fullRoster=false);
#endif
bool setAddonOutfitHidden(int target,bool hidden,std::string& error);
bool openAddonWardrobe(int target,std::string& error,bool allowSharedFallback=true);
bool equipAddonItem(int target,const std::string& slot,const OutfitParts& parts,std::string& error);
bool clearAddonSlot(int target,const std::string& slot,std::string& error);
bool clearAddonWardrobe(int target,std::string& error);
bool saveAddonWardrobe(int target,std::string& error);
void cancelAddonWardrobe(int target);
// Shares one character's settings across its game records, or gives each record its own.
// Records without their own saved outfit start from the shared one.
bool setAddonSettingsShared(int owner,bool shared,std::string& error);
}
