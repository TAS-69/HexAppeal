#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <string_view>

namespace hex {
using OutfitParts = std::set<std::string>;
using OutfitSlots = std::map<std::string, OutfitParts>;
using WardrobeItemOptions = std::map<std::pair<std::string,std::string>,std::string>;

enum class SlotOutfit { Game, Default, Barber, Saved, Combat };
struct SlotProfile {
    SlotOutfit selected = SlotOutfit::Game;
    OutfitSlots barber, saved, combat;
    std::set<std::string> hidden;
    WardrobeItemOptions itemOptions;
    bool hasSaved=false,hideOutfit=false;
};

bool validOutfitPart(std::string_view name);
bool protectedOutfitPart(std::string_view name);
std::string outfitSlotForPart(std::string_view name);
OutfitParts outfitSlotParts(const OutfitSlots& slots);
bool outfitPartsPresent(const OutfitParts& worn, const OutfitParts& target);
bool canMaskInheritedOutfitParts(const OutfitParts& worn, const OutfitParts& target,
                               const OutfitParts& defaults, const OutfitParts& follow);
bool validOutfitSlot(std::string_view slot);

struct OutfitSlotChanges {
    OutfitParts remove, add;
};
OutfitSlotChanges outfitSlotChanges(const OutfitSlots& current, const OutfitSlots& target, bool includeRetainedParts = false);

struct WardrobeSlotRetry {
    unsigned attempts=0;
    uint64_t matchingSince=0;
    bool tryAgain(uint64_t now,bool matches,bool settled,uint64_t deadline);
};

class WardrobeSlots {
    std::filesystem::path path;
    bool separateOutfitVisibility=false;
    bool recoveryBackupNeeded=false;
    std::array<SlotProfile, 6> profiles;
    bool commit(const std::array<SlotProfile, 6>& updated, std::string& error);
public:
    bool load(const std::filesystem::path& file, std::string& error,bool separateOutfitVisibility=false);
    const SlotProfile& profile(int witch) const { return profiles.at(witch); }
    bool select(int witch, SlotOutfit outfit, std::string& error);
    bool save(int witch, SlotOutfit outfit, const OutfitSlots& slots, std::string& error,bool activate=false,
              const WardrobeItemOptions* itemOptions=nullptr);
    bool observe(int witch, SlotOutfit outfit, OutfitSlots slots);
    bool setHidden(int witch, const std::string& slot, bool hidden, std::string& error);
    bool setOutfitHidden(int witch,bool hidden,std::string& error);
};
}
