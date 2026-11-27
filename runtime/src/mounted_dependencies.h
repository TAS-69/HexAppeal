#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>
#include "skin_tone.h"
namespace hex {
struct Profile;
enum class BodyMode {Venus,Vanilla,Invalid,Live};
struct DependencyPaths {
    std::filesystem::path managerConfig,modsDirectory,manifest,hexPreset,venusBody,legacyOutfitSource;
};
struct DependencyFile {bool valid=false;std::filesystem::path path;std::string detail;bool verified=false;};
struct MountedRoute {std::string resource,sourceName;std::filesystem::path sourceFile;};
struct MountedBodyIssue {int witch;std::string resource,reason;bool knownBody=false;};
struct DependencyStatus {
    bool scanned=false,bodyVerified=false,customPreset=false;
    BodyMode bodyMode=BodyMode::Venus;
    DependencyFile manager,manifest,venus,hex,game,instant,physics,creator;
    SkinToneSource skinTone;
    std::filesystem::path barberMeshDefinition;
    std::array<std::string,6> underwearSources;
    std::array<std::string,6> unavailableBodyDetails;
    uint32_t missingUnderwearMeshMask=0;
    uint32_t confirmedUnderwearRules=0;
    uint32_t unavailableBodyWitches=0;
    std::vector<std::filesystem::path> searchRoots;
    std::vector<std::filesystem::path> surgeryPresets;
    // bodyWarning covers every character (an unverified shared body, for example). A witch's own
    // appearance or body replacement is reported only for her; bodySkipped names witches Shape skips.
    std::string bodyDetail,bodyWarning,bodyNote,bodySkipped;
    std::array<std::string,6> witchBodyWarnings;
    std::map<std::string,unsigned> mountedMods;
    std::vector<MountedRoute> relevantRoutes;
    std::filesystem::path managerMountLog;
    std::vector<std::string> managerFindings;
    bool bodySourceReady() const {return bodyMode==BodyMode::Vanilla||(bodyMode==BodyMode::Venus&&venus.valid);}
    bool canShapeBody() const {return bodyVerified||bodyMode==BodyMode::Live;}
};
BodyMode mountedBodyMode(const std::filesystem::path& manifest,uint32_t* cloneWitches=nullptr,uint32_t* unavailableWitches=nullptr);
BodyMode mountedBodyMode(const std::map<std::string,std::filesystem::path>& routes,uint32_t* cloneWitches,uint32_t* unavailableWitches,
                         std::vector<MountedBodyIssue>* issues=nullptr,const std::filesystem::path& game={});
bool mountedDamianeVenus(const std::map<std::string,std::filesystem::path>& routes);
std::map<std::string,std::filesystem::path> readMountedRoutes(const std::filesystem::path& manifest,
    bool* supported=nullptr,std::string* formatWarning=nullptr);
uint32_t mountedNpcUnderwearWitches(const std::map<std::string,std::filesystem::path>& routes,bool& meshRemoved);
std::array<std::filesystem::path,6> mountedBodyUnderwearRemovers(const std::map<std::string,std::filesystem::path>& routes);
DependencyStatus inspectDependencies(const std::filesystem::path& game,const DependencyPaths& paths);
bool loadDependencyPaths(const std::filesystem::path& file,DependencyPaths& paths,std::string& error);
bool saveDependencyPaths(const std::filesystem::path& file,const DependencyPaths& paths,std::string& error);
bool isOwnModContent(const std::filesystem::path& path);
bool verifyMountedDependencies(const std::filesystem::path& game,std::string& error);
bool verifyMountedDependencies(const std::filesystem::path& game,const std::filesystem::path& manifest,std::string& error);
bool verifyMountedDependencies(const std::filesystem::path& game,const std::filesystem::path& manifest,std::string& error,std::string& warning,uint32_t* unavailableWitches=nullptr,std::string* note=nullptr,
                               std::array<std::string,6>* witchWarnings=nullptr,std::string* skipped=nullptr);
bool readableDependency(const std::filesystem::path& path,const char* signature=nullptr);
std::string unverifiedDependency(const char* name);
std::string profileDependencyIssue(const Profile& profile,bool shaping,bool hex,bool weight);
}
