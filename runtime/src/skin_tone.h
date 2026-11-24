#pragma once
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace hex {
struct SkinToneSource {
    std::string route;
    std::filesystem::path texture;
    std::string detail;
    std::optional<unsigned> brightness,colour;
    std::map<std::string,std::string> bodyColours;
    bool customBody=false;
    bool custom() const {return !texture.empty()||customBody||brightness||colour;}
};

SkinToneSource activeDamianeSkin(const std::map<std::string,std::filesystem::path>& routes);
SkinToneSource activeVenusDamianeSkin(const std::map<std::string,std::filesystem::path>& routes);
inline constexpr auto skinRegistryRoute="character/bin__/partprefabtable.pappt";
inline constexpr auto damianeCustomizationRoute="character/descriptors/customization/cd_pc/cd_phw_damian_customization.paccd_xml";
struct SkinToneAssets {
    SkinToneSource source;
    std::map<std::string,std::string> files;
    std::vector<std::string> appearances,tones,sources,skipped;
    size_t privateBodies=0;
};
SkinToneAssets prepareSkinTone(const std::filesystem::path& game,
    const std::map<std::string,std::filesystem::path>& routes,
    const std::map<std::string,std::string>& addonAppearances={});
struct AddonVenusAssets {
    std::map<std::string,std::string> appearances;
    bool hasBaseUnderwear=false;
};
AddonVenusAssets prepareAddonVenus(const std::filesystem::path& game,
    const std::map<std::string,std::filesystem::path>& routes,const std::vector<std::string>& appearances);
inline constexpr auto venusBodyModelRoute="character/model/1_pc/2_phw/nude/phw_00_nude_00_0001_damian.pac";
}
