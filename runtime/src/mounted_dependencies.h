#pragma once
#include <filesystem>
#include <string>
#include <vector>
namespace hex {
struct Profile;
struct DependencyPaths {
    std::filesystem::path managerConfig,modsDirectory,manifest,hexPreset,venusBody,legacyOutfitSource;
};
struct DependencyFile {bool valid=false;std::filesystem::path path;std::string detail;bool verified=false;};
struct DependencyStatus {
    bool scanned=false,bodyVerified=false;
    DependencyFile manager,manifest,venus,hex,game,instant,physics,creator;
    std::vector<std::filesystem::path> searchRoots;
    std::string bodyDetail,bodyWarning;
};
DependencyStatus inspectDependencies(const std::filesystem::path& game,const DependencyPaths& paths);
bool loadDependencyPaths(const std::filesystem::path& file,DependencyPaths& paths,std::string& error);
bool saveDependencyPaths(const std::filesystem::path& file,const DependencyPaths& paths,std::string& error);
bool isOwnModContent(const std::filesystem::path& path);
bool verifyMountedDependencies(const std::filesystem::path& game,std::string& error);
bool verifyMountedDependencies(const std::filesystem::path& game,const std::filesystem::path& manifest,std::string& error);
bool verifyMountedDependencies(const std::filesystem::path& game,const std::filesystem::path& manifest,std::string& error,std::string& warning);
bool readableDependency(const std::filesystem::path& path,const char* signature=nullptr);
std::string unverifiedDependency(const char* name);
std::string profileDependencyIssue(const Profile& profile,bool shaping,bool hex,bool weight);
}
