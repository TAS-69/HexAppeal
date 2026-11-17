#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>

namespace hex {
enum class WitchOutfit : uint8_t { Default=0,Legacy=1,Provider=2 };
enum class WitchColour : uint8_t { Original=0,White,Black,Crimson,Pink,Emerald };
enum class WardrobeField { Outfit };
struct WardrobeChoice {
    WitchOutfit outfit=WitchOutfit::Default;
    std::string provider;
    WitchColour underwear=WitchColour::Original,legacyColour=WitchColour::Original;
    bool operator==(const WardrobeChoice& rhs)const{return outfit==rhs.outfit&&provider==rhs.provider&&underwear==rhs.underwear&&legacyColour==rhs.legacyColour;}
};
inline bool validWardrobeId(const std::string& id){if(id.empty()||id.size()>95)return false;for(unsigned char c:id)if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'||c=='/'))return false;return id.find('/')!=std::string::npos;}
inline std::string wardrobeOutfitId(const WardrobeChoice& choice){return choice.outfit==WitchOutfit::Provider?choice.provider:choice.outfit==WitchOutfit::Legacy?"hexappeal/legacy-outfit-1":"hexappeal/default";}
struct WardrobeCapabilities {bool switchReady=false;};
struct WardrobeSelectionSnapshot {std::array<WardrobeChoice,6> choices{};std::array<uint64_t,6> generations{};};
class WardrobeSelection {
    mutable std::mutex mutex;
    WardrobeSelectionSnapshot state;
    std::filesystem::path file;
public:
    bool load(const std::filesystem::path& path,std::string& error);
    WardrobeSelectionSnapshot snapshot()const;
    bool change(int target,WardrobeField field,int value,const WardrobeCapabilities& available,std::string& error);
    bool providerOutfit(int target,const std::string& id,std::string& error);
    void invalidate(int target,const std::string& id);
};
}
