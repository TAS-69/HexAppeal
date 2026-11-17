#include "mounted_dependencies.h"
#include "runtime.h"
#include "venus_bodies.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <cstring>

namespace hex {
std::string unverifiedDependency(const char* name){return std::string("This version of ")+name+" has not been verified. Functionality is not guaranteed. You can still use these controls.";}
bool readableDependency(const std::filesystem::path& path,const char* signature){if(path.empty())return false;auto name=std::filesystem::absolute(path).lexically_normal().wstring();if(name.rfind(L"\\\\?\\",0)!=0)name=name.rfind(L"\\\\",0)==0?L"\\\\?\\UNC\\"+name.substr(2):L"\\\\?\\"+name;std::ifstream in(std::filesystem::path(name),std::ios::binary);if(!in)return false;char header[4]{};in.read(header,sizeof header);return in.gcount()>0&&(!signature||(in.gcount()==4&&!std::memcmp(header,signature,4)));}
bool verifyMountedDependencies(const std::filesystem::path& game,std::string& error) {
    return verifyMountedDependencies(game,game/L"bin64/dmm/manifest.tsv",error);
}
bool verifyMountedDependencies(const std::filesystem::path& game,const std::filesystem::path& manifest,std::string& error) {
    std::string warning;return verifyMountedDependencies(game,manifest,error,warning);
}
bool verifyMountedDependencies(const std::filesystem::path& game,const std::filesystem::path& manifest,std::string& error,std::string& warning) {
    warning.clear();
    std::ifstream in(manifest);
    if(!in){error="Mount the Hex Appeal bodies and one VENUS Human Body or Elf Body through Desert Mod Manager first.";return false;}
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
    const char* bodyPaths[]={
        "character/model/1_pc/2_phw/nude/phw_00_nude_00_0001_damian.pac",
        "character/model/1_pc/2_phw/nude/cd_phw_00_nude_00_0001_damian.pac"
    };
    for(const auto path:bodyPaths) {
        const auto found=routes.find(path);
        if(found==routes.end()&&path==bodyPaths[1])continue;
        if(found==routes.end()||isOwnModContent(found->second)||!readableDependency(found->second,"PAR ")) {
            error="The mounted VENUS body path is missing, unreadable or not a PAC file. Check body priority and remount.";return false;
        }
        if(!venusBodyVersion(sha256(found->second)))warning=unverifiedDependency("VENUS Body");
    }
    struct Appearance {const char* path;const char* hash;};
    const Appearance appearances[]={
        {"character/appearance/2_mon/cd_m0001_00_twofeet/cd_m0001_00_phw_north/cd_m0001_00_phw_north_44001.app_xml","5c0541a8ef1c686af58e7f6b7370bdeb59293bc71ef5a9bf1c8c4ab2f70d44c0"},
        {"character/appearance/3_npc/cd_npc_north/cd_nhw_north/cd_nhw_north_20026.app_xml","051af75fb41cf669ef2f31713df82a93be1eae9a1ff8c8eaba5f9db517f09143"},
        {"character/appearance/3_npc/cd_npc_south/cd_nhw_south/cd_nhw_south_20165.app_xml","94e56c70dc522126106d95765da1befbfb77ef38a889b2c857f2aafb1da18d70"},
        {"character/appearance/3_npc/cd_npc_south/cd_nhw_south/cd_nhw_south_20166.app_xml","e20de4404c04793b358127f159b84c3bf6b4cfbf55200cec59fe5fc40080f20b"},
        {"character/appearance/3_npc/cd_npc_south/cd_nhw_south/cd_nhw_south_20167.app_xml","da205bbe4a5ce12558de51a5feff531d8d1ca8db56d1b7a48ee3f6fa9037facc"},
        {"character/appearance/3_npc/cd_npc_south/cd_nhw_south/cd_nhw_south_20168.app_xml","099cdc89ffa8e47e8883e7af8766a3e3b86fad73bd9b75663b0e221f44e431de"},
        {"character/appearance/3_npc/cd_npc_south/cd_nhw_south/cd_nhw_south_20169.app_xml","da7a172a21e53c682fac70504a4edc508b569e5541e1f56e7b85a77f2cdacc9a"}
    };
    for(const auto& appearance:appearances) {
        const auto found=routes.find(appearance.path);
        if(found==routes.end()||!readableDependency(found->second)) {
            error="A mounted witch appearance path is missing or unreadable. Remount the body component.";return false;
        }
        if(sha256(found->second)!=appearance.hash)warning=unverifiedDependency("the mounted witch appearance replacement");
    }
    return true;
}
}
