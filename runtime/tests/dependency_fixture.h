#pragma once
#include "mounted_dependencies.h"
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <cstring>
#include <vector>

namespace hex::test {
inline bool witchAppearance(const std::string& path){
    for(const auto* suffix:{"44001.app_xml","20026.app_xml","20165.app_xml","20166.app_xml","20167.app_xml","20168.app_xml","20169.app_xml"})
        if(path.size()>=std::strlen(suffix)&&path.compare(path.size()-std::strlen(suffix),std::strlen(suffix),suffix)==0)return true;
    return false;
}
inline std::string vanillaBodyFor(const std::string& path){return path.find("20026.app_xml")!=std::string::npos?"hvw_00_nude_00_0301":path.find("20169.app_xml")!=std::string::npos?"hvw_00_nude_00_0001_damian":"hvw_00_nude_00_0001";}
inline std::filesystem::path venusManifestFixture(const std::filesystem::path& source,const std::filesystem::path& folder){
    namespace fs=std::filesystem;
    if(mountedBodyMode(source)==BodyMode::Venus)return source;
    fs::create_directories(folder);std::ifstream input(source);std::string line;std::vector<std::string> lines;std::map<std::string,fs::path> routes;
    while(std::getline(input,line)){
        if(!line.empty()&&line.back()=='\r')line.pop_back();lines.push_back(line);const auto tab=line.find('\t',5);
        if(line.rfind("file\t",0)||tab==std::string::npos)continue;auto path=fs::u8path(line.substr(tab+1));if(path.is_relative())path=source.parent_path()/path;routes[line.substr(5,tab-5)]=path;
    }
    auto body=routes.find("character/model/1_pc/2_phw/nude/phw_00_nude_00_0001_damian.pac");
    if(body==routes.end())body=routes.find("character/model/ha_p/2_phw/nude/hvw_00_nude_00_0001.pac");
    if(body==routes.end())throw std::runtime_error("A readable body is required for the isolated dependency fixture");
    const auto external=folder/L"reference-body.pac";fs::copy_file(body->second,external,fs::copy_options::overwrite_existing);
    std::ostringstream output;unsigned count=0;
    const std::regex nude(R"((<Nude\s*>\s*<Prefab\s+[^<>]*\bName\s*=\s*["'])([^"']+)(["']))");
    for(auto row:lines){
        const auto tab=row.find('\t',5);
        if(row.rfind("file\t",0)==0&&tab!=std::string::npos){
            const auto key=row.substr(5,tab-5);
            if(key.find("character/model/1_pc/2_phw/nude/")==0||key.find("character/model/ha_p/2_phw/nude/")==0||key.find("character/modelproperty/ha_p/2_phw/nude/")==0||key.find("character/bin__/meshphysics/ha_p/2_phw/nude/")==0)continue;
            if(key.find("character/appearance/")==0&&witchAppearance(key)){
                std::ifstream app(routes.at(key),std::ios::binary);const std::string before((std::istreambuf_iterator<char>(app)),{});
                if(!std::regex_search(before,nude))throw std::runtime_error("Original appearance fixture has no body slot");
                const auto path=folder/(std::to_wstring(count++)+L".app_xml");std::ofstream file(path,std::ios::binary);file<<std::regex_replace(before,nude,"$1phw_00_nude_00_0001_damian$3",std::regex_constants::format_first_only);
                row.replace(tab+1,std::string::npos,path.u8string());
            }
        }
        output<<row<<'\n';
    }
    if(count!=7)throw std::runtime_error("Seven appearance fixtures are required");
    output<<"file\tcharacter/model/1_pc/2_phw/nude/phw_00_nude_00_0001_damian.pac\t"<<external.u8string()<<'\n';
    const auto manifest=folder/L"manifest.tsv";std::ofstream file(manifest,std::ios::binary);file<<output.str();return manifest;
}
}
