#pragma once
#include "wardrobe_slots.h"
#include <vector>
#include <cstdint>

struct IWICBitmapDecoder;
struct IWICImagingFactory;

namespace hex {
struct WardrobeItem {
    uint32_t id=0;
    std::string name,slot,tags,source;
    OutfitParts parts;
    std::vector<uint32_t> icon;
    bool fitUnverified=false;
};

class WardrobeItemCatalog {
    std::vector<WardrobeItem> items;
    std::map<std::string,size_t,std::less<>> byPart;
    void indexParts();
public:
    bool load(const std::filesystem::path& root);
    bool loadStock(const std::filesystem::path& game,const std::filesystem::path& manifest={});
    bool empty() const { return items.empty(); }
    bool hasMissingIcons() const;
    void fillMissingIcons(const WardrobeItemCatalog& stock);
    void supplement(const WardrobeItemCatalog& local);
    void retainMounted(const WardrobeItemCatalog& previous);
    // Body labels are advisory. The game registry validates equipment before preview.
    size_t addMounted(const std::filesystem::path& bin, std::string_view body,
                      const std::filesystem::path& manifest={},bool* scanComplete=nullptr);
    const WardrobeItem* forPart(std::string_view part) const;
    std::vector<const WardrobeItem*> find(std::string_view query,std::string_view slot,size_t limit=48,bool requireIcon=false) const;
};
inline constexpr int wardrobeIconSize=32;
std::vector<uint32_t> wardrobeIconPixels(IWICBitmapDecoder& decoder, IWICImagingFactory& factory);
}
