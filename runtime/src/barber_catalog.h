#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include "wardrobe_slots.h"

namespace hex {
OutfitSlots readDefaultWardrobeSlots(const std::filesystem::path& appearance,int witch);
class BarberCatalog {
    struct Slot {
        bool armor=false;
        std::map<uint8_t,std::set<std::string>> options;
    };
    std::array<Slot,16> slots;
    std::set<std::string> prefabs;
public:
    bool load(const std::filesystem::path& path);
    bool has(const std::string& prefab) const;
    bool isArmorSlot(size_t slot) const;
    bool matchesIdentity(const std::array<uint8_t,16>& first,const std::array<uint8_t,16>& second) const;
    std::string slotFor(const std::string& prefab) const;
    bool resolveSlots(const std::array<uint8_t,16>& choices,OutfitSlots& clothing) const;
    OutfitParts combatFallbacks(std::array<uint8_t,16> choices) const;
    bool resolve(const std::array<uint8_t,16>& choices,std::set<std::string>& clothing) const;
};
}
