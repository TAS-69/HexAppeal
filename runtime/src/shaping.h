#pragma once
#include <Windows.h>
#include <array>
#include <filesystem>
#include <string>
#include <vector>
#include "model.h"

namespace hex {
struct SurgeryProfile {
    std::array<float,64> scale;
    std::array<float,64> left;
    std::array<float,64> right;
    std::array<float,16> macros;
    uint8_t symmetry;
    uint8_t padding[3];
    float teardrop;
    std::array<float,4> contours;
};
static_assert(sizeof(SurgeryProfile)==0x358);
static_assert(offsetof(SurgeryProfile,macros)==0x300);
static_assert(offsetof(SurgeryProfile,symmetry)==0x340);
struct ShapeBone {uint32_t nameHash;int32_t parent;};
using PoseMatrix=std::array<float,16>;
bool applyStature(const Profile& profile,const std::vector<PoseMatrix>& input,std::vector<PoseMatrix>& output,std::string& error);
uint64_t statureStamp(uint64_t nativeStamp,const Profile& profile);
struct ResolvedShape {
    SurgeryProfile native{};
    std::array<float,64> percent{};
};
class NativeShapeEngine {
    unsigned char* base=nullptr;
    std::array<std::string,64> names{};
    std::array<std::string,16> macroNames{};
    size_t macroCount=0;
    SurgeryProfile originalHex{};
    bool hasHex=false;
    bool weightAvailable=false;
    bool initialiseMetadata();
    bool initialiseMetadataGuard();
public:
    bool bind(HMODULE module,std::string& error);
    void setWeightAvailable(bool available){weightAvailable=available;}
    bool readHex(const std::filesystem::path& path,std::string& error);
    void forgetHex(){originalHex={};hasHex=false;}
    bool compose(const Profile& choice,ResolvedShape& result,std::string& error);
    bool deform(const std::vector<ShapeBone>& bones,const std::vector<PoseMatrix>& input,
                const SurgeryProfile& shape,std::vector<PoseMatrix>& output,std::string& error,bool callerOwnsSurgeryLock=false);
    bool resolve(const SurgeryProfile& shape,std::array<float,64>& percent,std::string& error);
    const SurgeryProfile& hexSource() const{return originalHex;}
    const std::array<std::string,64>& parameterNames() const{return names;}
    static bool bottomParameter(size_t index);
    float minimumScale() const;
};
}
