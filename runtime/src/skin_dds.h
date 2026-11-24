#pragma once
#include <array>
#include <optional>
#include <string>
#include <string_view>

namespace hex {
using SkinGain=std::array<float,3>;
std::optional<SkinGain> compareSkinColour(std::string_view original,std::string_view installed);
std::string adjustSkinColour(std::string texture,const SkinGain& gain);
}
