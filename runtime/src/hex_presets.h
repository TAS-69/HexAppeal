#pragma once
#include <array>
#include <string>
namespace hex {
struct HexPresetVersion {const char* version;const char* filename;const char* marker;const char* sha256;};
inline constexpr std::array<HexPresetVersion,2> hexPresetVersions={{
    {"1.3","preset-f-HentaiBodyV1_3Venus.ini","1:preset-f-HentaiBodyV1_3Venus","6aaada9dc31ebea3203fb3dff6cd57e753c4609a05f08c4b2fa55c7d85c58e34"},
    {"1.2","preset-f-HentaiBodyV1_2Venus.ini","1:preset-f-HentaiBodyV1_2Venus","07e1ed2a6d58ea80adbaf57758d08b2dfd72bf2ddde49ddced3eb7e42bd58d5f"}
}};
inline const HexPresetVersion* hexPresetVersion(const std::string& digest){for(const auto& preset:hexPresetVersions)if(digest==preset.sha256)return &preset;return nullptr;}
}
