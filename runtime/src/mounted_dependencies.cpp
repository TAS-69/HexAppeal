#include "mounted_dependencies.h"
#include "runtime.h"
#include "venus_bodies.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <cstring>
#include <regex>

namespace hex {
namespace {
struct BodyAppearance {const char* path;const char* vanilla;};
constexpr BodyAppearance bodyAppearances[]={
    {"character/appearance/2_mon/cd_m0001_00_twofeet/cd_m0001_00_phw_north/cd_m0001_00_phw_north_44001.app_xml","hvw_00_nude_00_0001"},
    {"character/appearance/3_npc/cd_npc_north/cd_nhw_north/cd_nhw_north_20026.app_xml","hvw_00_nude_00_0301"},
    {"character/appearance/3_npc/cd_npc_south/cd_nhw_south/cd_nhw_south_20165.app_xml","hvw_00_nude_00_0001"},
    {"character/appearance/3_npc/cd_npc_south/cd_nhw_south/cd_nhw_south_20166.app_xml","hvw_00_nude_00_0001"},
    {"character/appearance/3_npc/cd_npc_south/cd_nhw_south/cd_nhw_south_20167.app_xml","hvw_00_nude_00_0001"},
    {"character/appearance/3_npc/cd_npc_south/cd_nhw_south/cd_nhw_south_20168.app_xml","hvw_00_nude_00_0001"},
    {"character/appearance/3_npc/cd_npc_south/cd_nhw_south/cd_nhw_south_20169.app_xml","hvw_00_nude_00_0001_damian"}};
bool appearanceUsesBody(const std::filesystem::path& path,const char* expected){
    std::error_code ec;const auto bytes=std::filesystem::file_size(path,ec);if(ec||bytes>1024*1024)return false;
    std::ifstream input(path,std::ios::binary);const std::string text((std::istreambuf_iterator<char>(input)),{});
    static const std::regex nude(R"(<Nude\s*>\s*(<Prefab\s+[^<>]*/>)\s*</Nude\s*>)");
    static const std::regex name(R"(\bName\s*=\s*(["'])([^"']+)\1)");
    std::smatch match;if(!std::regex_search(text,match,nude))return false;
    const auto part=match[1].str();std::smatch named;return std::regex_search(part,named,name)&&named[2].str()==expected&&!std::regex_search(match.suffix().str(),nude);
}
}
BodyMode mountedBodyMode(const std::filesystem::path& manifest){
    std::ifstream input(manifest);std::string line;std::map<std::string,std::filesystem::path> routes;
    while(std::getline(input,line)){
        if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.rfind("file\t",0))continue;
        const auto tab=line.find('\t',5);if(tab==std::string::npos)continue;auto key=line.substr(5,tab-5);
        std::transform(key.begin(),key.end(),key.begin(),[](unsigned char c){return c=='\\'?'/':char(std::tolower(c));});
        if(key.rfind("character/appearance/",0))continue;auto path=std::filesystem::u8path(line.substr(tab+1));if(path.is_relative())path=manifest.parent_path()/path;
        if(routes.count(key)&&routes[key]!=path)return BodyMode::Invalid;routes[key]=path;
    }
    unsigned venus=0,vanilla=0;
    for(const auto& row:bodyAppearances){
        const auto found=routes.find(row.path);if(found==routes.end())return BodyMode::Invalid;
        if(appearanceUsesBody(found->second,row.vanilla))++vanilla;
        else if(appearanceUsesBody(found->second,"phw_00_nude_00_0001_damian"))++venus;
        else return BodyMode::Invalid;
    }
    return vanilla==7?BodyMode::Vanilla:venus==7?BodyMode::Venus:BodyMode::Invalid;
}
std::string unverifiedDependency(const char* name){return std::string("This version of ")+name+" has not been verified. Functionality is not guaranteed. You can still use these controls.";}
bool readableDependency(const std::filesystem::path& path,const char* signature){if(path.empty())return false;auto name=std::filesystem::absolute(path).lexically_normal().wstring();if(name.rfind(L"\\\\?\\",0)!=0)name=name.rfind(L"\\\\",0)==0?L"\\\\?\\UNC\\"+name.substr(2):L"\\\\?\\"+name;std::ifstream in(std::filesystem::path(name),std::ios::binary);if(!in)return false;char header[4]{};in.read(header,sizeof header);return in.gcount()>0&&(!signature||(in.gcount()==4&&!std::memcmp(header,signature,4)));}
bool verifyMountedDependencies(const std::filesystem::path& game,std::string& error) {
    return verifyMountedDependencies(game,game/L"bin64/dmm/manifest.tsv",error);
}
bool verifyMountedDependencies(const std::filesystem::path& game,const std::filesystem::path& manifest,std::string& error) {
    std::string warning;return verifyMountedDependencies(game,manifest,error,warning);
}
bool verifyMountedDependencies(const std::filesystem::path& game,const std::filesystem::path& manifest,std::string& error,std::string& warning) {
    error.clear();warning.clear();
    std::ifstream in(manifest);
    if(!in){error="Mount one Hex Appeal body option through Desert Mod Manager first.";return false;}
    std::map<std::string,std::filesystem::path> routes;std::string line;bool version=false;
    while(std::getline(in,line)) {
        if(!line.empty()&&line.back()=='\r')line.pop_back();
        if(line.rfind("dmm-runtime\t",0)==0){version=true;if(line!="dmm-runtime\t1")warning=unverifiedDependency("DMM's active file-list format");}
        if(line.rfind("file\t",0)!=0)continue;
        const auto tab=line.find('\t',5);if(tab==std::string::npos)continue;
        auto key=line.substr(5,tab-5);
        std::transform(key.begin(),key.end(),key.begin(),[](unsigned char c){return c=='\\'?'/':char(std::tolower(c));});
        auto path=std::filesystem::u8path(line.substr(tab+1));if(path.is_relative())path=manifest.parent_path()/path;
        if(routes.count(key)&&routes[key]!=path){error="The active file list contains conflicting routes.";return false;}
        routes[key]=path;
    }
    if(!version){error="The selected active file list is not a supported DMM runtime manifest.";return false;}
    const auto mode=mountedBodyMode(manifest);
    if(mode==BodyMode::Invalid){error="The mounted witch bodies are missing or mixed. Enable one Hex Appeal body option, remove any stale older entry and remount.";return false;}
    const char* bodyPaths[]={
        "character/model/1_pc/2_phw/nude/phw_00_nude_00_0001_damian.pac",
        "character/model/1_pc/2_phw/nude/cd_phw_00_nude_00_0001_damian.pac"
    };
    if(mode==BodyMode::Venus)for(const auto path:bodyPaths) {
        const auto found=routes.find(path);
        if(found==routes.end()&&path==bodyPaths[1])continue;
        if(found==routes.end()||isOwnModContent(found->second)||!readableDependency(found->second,"PAR ")) {
            error="The mounted VENUS body path is missing, unreadable or not a PAC file. Check body priority and remount.";return false;
        }
        if(!venusBodyVersion(sha256(found->second)))warning=unverifiedDependency("VENUS Body");
    }
    if(mode==BodyMode::Vanilla){
        const std::pair<const char*,const char*> bodies[]={
            {"hvw_00_nude_00_0001","d51fb429448e147d0eabd512f092a8e7b99524ba24ad183ed231fee1e14da7bb"},
            {"hvw_00_nude_00_0001_damian","bad6e5d6b9f5b35efb35ad70e647a738fc4fde80f5993fe2802282a9ca909f75"},
            {"hvw_00_nude_00_0002","58c491a59da39ece03a2a96f021757bdbe5508a730edaa4267b7c80f20d3f909"}};
        for(const auto& row:bodies){
            const auto path="character/model/ha_p/2_phw/nude/"+std::string(row.first)+".pac";const auto body=routes.find(path);
            if(body==routes.end()||!readableDependency(body->second,"PAR ")){error="A mounted original-witch body is missing or unreadable. Reinstall the Vanilla option and remount.";return false;}
            if(sha256(body->second)!=row.second)warning=unverifiedDependency("the original-witch body resource");
            for(const auto& resource:{"character/modelproperty/ha_p/2_phw/nude/"+std::string(row.first)+".pac_xml","character/bin__/meshphysics/ha_p/2_phw/nude/"+std::string(row.first)+".hkx"}){
                const auto found=routes.find(resource);if(found==routes.end()||!readableDependency(found->second)){error="Original-witch body metadata or physics is missing. Reinstall the Vanilla option and remount.";return false;}
            }
        }
    }
    struct Appearance {const char* path;const char* hash;const char* isolatedHash=nullptr;const char* vanillaName="hvw_00_nude_00_0001";};
    const Appearance appearances[]={
        {"character/appearance/2_mon/cd_m0001_00_twofeet/cd_m0001_00_phw_north/cd_m0001_00_phw_north_44001.app_xml","5c0541a8ef1c686af58e7f6b7370bdeb59293bc71ef5a9bf1c8c4ab2f70d44c0"},
        {"character/appearance/3_npc/cd_npc_north/cd_nhw_north/cd_nhw_north_20026.app_xml","051af75fb41cf669ef2f31713df82a93be1eae9a1ff8c8eaba5f9db517f09143",nullptr,"hvw_00_nude_00_0301"},
        {"character/appearance/3_npc/cd_npc_south/cd_nhw_south/cd_nhw_south_20165.app_xml","94e56c70dc522126106d95765da1befbfb77ef38a889b2c857f2aafb1da18d70","680d69e548f58e18a83e6ec217230e5017738919d7bb85b8cb94b60e3095472e"},
        {"character/appearance/3_npc/cd_npc_south/cd_nhw_south/cd_nhw_south_20166.app_xml","e20de4404c04793b358127f159b84c3bf6b4cfbf55200cec59fe5fc40080f20b"},
        {"character/appearance/3_npc/cd_npc_south/cd_nhw_south/cd_nhw_south_20167.app_xml","da205bbe4a5ce12558de51a5feff531d8d1ca8db56d1b7a48ee3f6fa9037facc","3d0943c11ed5b831b873a785e0ba8da50773c9ce0fdee47f924fe5962cd04b0e"},
        {"character/appearance/3_npc/cd_npc_south/cd_nhw_south/cd_nhw_south_20168.app_xml","099cdc89ffa8e47e8883e7af8766a3e3b86fad73bd9b75663b0e221f44e431de","ff7160fc7a043f8d1ce9070f782b0a767f0802348380ca0e4358021953d1f5b6"},
        {"character/appearance/3_npc/cd_npc_south/cd_nhw_south/cd_nhw_south_20169.app_xml","da7a172a21e53c682fac70504a4edc508b569e5541e1f56e7b85a77f2cdacc9a",nullptr,"hvw_00_nude_00_0001_damian"}
    };
    for(const auto& appearance:appearances) {
        const auto found=routes.find(appearance.path);
        if(found==routes.end()||!readableDependency(found->second)) {
            error="A mounted witch appearance path is missing or unreadable. Remount the body component.";return false;
        }
        if(!appearanceUsesBody(found->second,mode==BodyMode::Vanilla?appearance.vanillaName:"phw_00_nude_00_0001_damian")){error="A witch appearance does not use the selected body. Check the selected body option and remount.";return false;}
        const auto digest=sha256(found->second);
        if(digest!=appearance.hash&&(!appearance.isolatedHash||digest!=appearance.isolatedHash))warning=unverifiedDependency("the mounted witch appearance replacement");
    }
    return true;
}
}
