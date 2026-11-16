#pragma once
#include <array>
#include <string>
#include <filesystem>
#include <cstdint>
#include "HexAppealBackendV1.h"

namespace hex {
enum class Preset : uint32_t { Venus=0, Hex=1, Flat=2 };
inline const char* presetNames[] = {"VENUS", "Hex's Hentai Preset", "Flat"};
struct Witch {
    const char* name;
    const wchar_t* section;
    HexAppealTargetV1 target;
};
inline const std::array<Witch,6> witches = {{
    {"Lyselia",L"Lyselia",{sizeof(HexAppealTargetV1),1,{1002941,0,0}}},
    {"Bari",L"Bari",{sizeof(HexAppealTargetV1),3,{1002940,1001374,1001091}}},
    {"Areciel",L"Areciel",{sizeof(HexAppealTargetV1),1,{1002938,0,0}}},
    {"Elowen",L"Elowen",{sizeof(HexAppealTargetV1),1,{1002939,0,0}}},
    {"Sylvia",L"Sylvia",{sizeof(HexAppealTargetV1),1,{1001369,0,0}}},
    {"White Crow",L"WhiteCrow",{sizeof(HexAppealTargetV1),1,{6622,0,0}}}
}};
struct Profile {
    Preset top=Preset::Venus;
    Preset bottom=Preset::Venus;
    float topBlend=1.f;
    float bottomBlend=1.f;
    bool autoFit=true;
    float height=0.f,weight=0.f;
    bool valid() const;
    HexAppealProfileV1 wire() const;
    bool operator==(const Profile& b) const;
};
struct Model {
    std::array<Profile,6> saved{};
    Profile draft{};
    int target=-1;
    std::string message="Choose a combination. Saving does not change the game.";
    bool applied=false;
    uint64_t pendingTransaction=0;
    Profile pendingProfile{};
    int pendingTarget=-1;
    std::array<Profile,7> drafts{};
    bool draftsReady=false;
    std::array<Profile,6> lastApplied{};
    std::array<bool,6> hasApplied{};
    std::array<Profile,6> pendingProfiles{};
    uint32_t pendingMask=0;
    bool lastApplyFailed=false;
    void selectTarget(int value);
    bool selectCombination(int top,int bottom);
    void saveDraft();
    bool save(const std::filesystem::path& path,std::string& error) const;
    bool load(const std::filesystem::path& path,std::string& error);
    bool apply(const HexAppealBackendV1* backend,std::string& error);
    bool applySaved(const HexAppealBackendV1* backend,uint32_t mask,std::string& error);
    void poll(const HexAppealBackendV1* backend);
    bool draftIsSaved() const;
    bool draftIsApplied() const;
    void restoreSaved();
};
uint64_t requiredCapabilities(const Profile& profile);
bool validBackend(const HexAppealBackendV1* backend);
float regionCoordinate(Preset preset,float blend);
void setRegionCoordinate(float coordinate,Preset& preset,float& blend);
void moveShapePad(Profile& profile,float bottom,float top);
float linkedStature(const Profile& profile);
void snapShapePad(float& x,float& y,float side,int& detent);
}
