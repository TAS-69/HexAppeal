#pragma once
#include <array>
#include <filesystem>
#include <string>

namespace hex {
struct DancerColourVariant {const char* id;const char* label;const char* folder;const char* sha256;};
inline constexpr std::array<DancerColourVariant,5> dancerColours{{
    {"white","Original White","00 Color - Original White","1547547f672fc339019269fe52714273439f7ae7a63f90a14a2194be83ff1056"},
    {"black","Midnight Black","10 Color - Midnight Black","a6afb66e561a2bb0877cdce7166d4a71dc665e7de284deeea0e6e1f66120a237"},
    {"crimson","Crimson Red","20 Color - Crimson Red","9c5d65142c2cd1d8748cadfd01f42e6fc314a80c18e440495403afcc982dbe2b"},
    {"pink","Arcane Pink","30 Color - Arcane Pink","740de77a37b2f4d28cecb8de53c74a0783f799ed27502fa2fd42fd4585dcfef0"},
    {"emerald","Emerald Green","40 Color - Emerald Green","e6dc9ce64307a60d0ece5ee1d10e33f84d6cc83303a5fe871d19238f737bdfd7"}
}};
inline constexpr const char* dancerTexture="character/texture/cd_phw_00_ub_00_0135_00_02_01_o.dds";
inline constexpr const char* dancerBindingMarker="HexAppeal.ExternalDancerColourBinding/1\n";
int dancerColourForHash(const std::string& digest);
bool managedDancerBinding(const std::filesystem::path& texture,std::filesystem::path& root);
bool verifyDancerPalette(const std::filesystem::path& sourceRoot,std::string& error);
bool changeDancerBinding(const std::filesystem::path& texture,const std::filesystem::path& sourceRoot,int colour,std::string& error);
}
