#include "mounted_dependencies.h"
#include "runtime.h"
#include "hex_presets.h"
#include "venus_bodies.h"
#include <TlHelp32.h>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <set>
#include <sstream>

namespace hex {
namespace {
namespace fs=std::filesystem;
std::string lower(std::string value){std::transform(value.begin(),value.end(),value.begin(),[](unsigned char c){return char(std::tolower(c));});return value;}
fs::path absolute(const fs::path& input,const fs::path& relativeTo) {
    if(input.empty())return {};
    wchar_t expanded[32768]{};const auto length=ExpandEnvironmentStringsW(input.c_str(),expanded,32768);if(!length||length>32768)return {};
    fs::path path(expanded);if(path.is_relative())path=relativeTo/path;
    std::error_code ec;auto resolved=fs::weakly_canonical(path,ec);return ec?path.lexically_normal():resolved;
}
bool regular(const fs::path& p){std::error_code ec;return fs::is_regular_file(p,ec);}
bool directory(const fs::path& p){std::error_code ec;return fs::is_directory(p,ec);}
std::string readText(const fs::path& p,size_t limit=1024*1024){std::error_code ec;const auto bytes=fs::file_size(p,ec);if(ec||bytes>limit)return {};std::ifstream file(p,std::ios::binary);return {std::istreambuf_iterator<char>(file),{}};}
bool jsonString(const std::string& text,size_t& at,std::string& out) {
    if(at>=text.size()||text[at++]!='"')return false;out.clear();
    const auto hexDigit=[](char c)->int{return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;};
    const auto unicode=[&](uint32_t& value){value=0;for(int i=0;i<4;++i){if(at>=text.size())return false;const int digit=hexDigit(text[at++]);if(digit<0)return false;value=(value<<4)|digit;}return true;};
    while(at<text.size()){
        const auto c=static_cast<unsigned char>(text[at++]);if(c=='"')return true;if(c<32)return false;
        if(c!='\\'){out+=char(c);continue;}if(at>=text.size())return false;
        switch(text[at++]){
            case '"':out+='"';break;case '\\':out+='\\';break;case '/':out+='/';break;
            case 'b':out+='\b';break;case 'f':out+='\f';break;case 'n':out+='\n';break;case 'r':out+='\r';break;case 't':out+='\t';break;
            case 'u':{uint32_t cp=0;if(!unicode(cp))return false;if(cp>=0xd800&&cp<=0xdbff){if(at+2>text.size()||text[at++]!='\\'||text[at++]!='u')return false;uint32_t low=0;if(!unicode(low)||low<0xdc00||low>0xdfff)return false;cp=0x10000+((cp-0xd800)<<10)+low-0xdc00;}else if(cp>=0xdc00&&cp<=0xdfff)return false;
                if(cp<0x80)out+=char(cp);else if(cp<0x800){out+=char(0xc0|(cp>>6));out+=char(0x80|(cp&63));}else if(cp<0x10000){out+=char(0xe0|(cp>>12));out+=char(0x80|((cp>>6)&63));out+=char(0x80|(cp&63));}else{out+=char(0xf0|(cp>>18));out+=char(0x80|((cp>>12)&63));out+=char(0x80|((cp>>6)&63));out+=char(0x80|(cp&63));}break;}
            default:return false;
        }
    }return false;
}
std::string property(const std::string& text,const char* key) {
    size_t at=0;int depth=0;std::string found;
    while(at<text.size()){
        const auto c=text[at];if(c=='{'||c=='['){++depth;++at;continue;}if(c=='}'||c==']'){--depth;++at;continue;}
        if(c!='"'){++at;continue;}std::string name;if(!jsonString(text,at,name))return {};
        if(depth!=1||name!=key)continue;
        while(at<text.size()&&isspace(static_cast<unsigned char>(text[at])))++at;
        if(at==text.size()||text[at++]!=':')return {};
        while(at<text.size()&&isspace(static_cast<unsigned char>(text[at])))++at;
        std::string value;if(!jsonString(text,at,value)||(!found.empty()&&found!=value))return {};found=value;
    }return found;
}
DependencyFile checked(const fs::path& p,const char* expected,const char* label) {
    DependencyFile result;result.path=p;
    if(p.empty()||!regular(p)){result.detail=std::string(label)+" not found. Set its path in Settings.";return result;}
    if(isOwnModContent(p)){result.detail=std::string(label)+" must be a separate installed dependency, outside Hex Appeal.";return result;}
    result.valid=readableDependency(p,"PAR ");result.verified=result.valid&&expected&&sha256(p)==expected;result.detail=!result.valid?std::string(label)+" is unreadable or not a PAC file.":result.verified?"Checked external dependency":unverifiedDependency(label);return result;
}
DependencyFile checkedVenus(const fs::path& path){
    auto result=checked(path,nullptr,"VENUS Body");
    if(result.valid){const auto* body=venusBodyVersion(sha256(path));result.verified=body!=nullptr;result.detail=body?std::string("Verified VENUS ")+body->version+" / "+body->variant:unverifiedDependency("VENUS Body");}
    return result;
}
DependencyFile checkedHex(const fs::path& path){
    DependencyFile result;result.path=path;
    if(path.empty()||!regular(path)){result.detail="Hex preset not found. Set its path in Settings.";return result;}
    if(isOwnModContent(path)){result.detail="Hex must remain a separate installed dependency.";return result;}
    const auto* version=hexPresetVersion(sha256(path));const auto text=readText(path);
    result.valid=text.find("InstantSurgeryOutfit")!=std::string::npos&&text.find("Shape.")!=std::string::npos;result.verified=version!=nullptr;
    result.detail=!result.valid?"The Hex preset file is unreadable or not an Instant Surgery preset.":version?std::string("Checked external Hex ")+version->version:unverifiedDependency("Hex's Hentai Preset");return result;
}
std::vector<fs::path> configCandidates(const fs::path& game) {
    std::vector<fs::path> result={game/L"nexus_downloads/dmm_update/config.json",game/L"config.json"};
    const auto snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);if(snapshot==INVALID_HANDLE_VALUE)return result;
    PROCESSENTRY32W entry{};entry.dwSize=sizeof(entry);
    if(Process32FirstW(snapshot,&entry))do{if(_wcsicmp(entry.szExeFile,L"DMM.exe"))continue;
        const auto process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,entry.th32ProcessID);if(!process)continue;
        wchar_t path[32768]{};DWORD size=32768;if(QueryFullProcessImageNameW(process,0,path,&size))result.push_back(fs::path(path).parent_path()/L"config.json");CloseHandle(process);
    }while(Process32NextW(snapshot,&entry));CloseHandle(snapshot);return result;
}
std::map<std::string,fs::path> readRoutes(const fs::path& manifest) {
    std::map<std::string,fs::path> result;std::ifstream file(manifest);std::string line;
    while(std::getline(file,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.rfind("file\t",0))continue;const auto tab=line.find('\t',5);if(tab==std::string::npos)continue;
        auto key=lower(line.substr(5,tab-5));std::replace(key.begin(),key.end(),'\\','/');result[key]=absolute(fs::u8path(line.substr(tab+1)),manifest.parent_path());}
    return result;
}

}
bool isOwnModContent(const std::filesystem::path& path) {
    std::error_code ec;const auto canonical=fs::weakly_canonical(path,ec);const auto& candidate=ec?path:canonical;
    for(const auto& part:candidate){const auto name=lower(part.u8string());if(name=="hex appeal"||name=="hexappeal"||name.rfind("hex appeal -",0)==0)return true;}return false;
}
DependencyStatus inspectDependencies(const fs::path& game,const DependencyPaths& paths) {
    DependencyStatus result;result.scanned=true;const auto base=absolute(game,game);
    const auto nativeFile=[&](const fs::path& path,const char* hash,const char* label){DependencyFile file;file.path=path;file.valid=readableDependency(path);file.verified=file.valid&&sha256(path)==hash;file.detail=!file.valid?std::string(label)+" is not installed or its file is unreadable.":file.verified?"Verified installed version":unverifiedDependency(label);return file;};
    result.game=nativeFile(base/L"bin64/CrimsonDesert.exe","8be67178d9b9a149c361a731051665e0ba4d441e250ae1dfe645bd6e524345c8","Crimson Desert");
    result.instant=nativeFile(base/L"bin64/InstantSurgery.asi","92d7cab4e244004ea433d1079c42288a985627fce9cb585844030282e591f009","Instant Surgery");
    result.physics=nativeFile(base/L"bin64/BodyPhysics.asi","6ed64a5136b3fd8a36fc1a3fcd02e62e102e7d0915f1c184daabd31c588db524","Body Physics");
    result.creator=nativeFile(base/L"bin64/CharacterCreator.asi","de9b8b3acf79dce35d219cc9daf2f1daf7f6e482950cab389fc2dd4b8f236b32","Character Creator");
    auto configs=paths.managerConfig.empty()?configCandidates(base):std::vector<fs::path>{absolute(paths.managerConfig,base)};
    for(const auto& config:configs){const auto text=readText(config);if(text.empty())continue;const auto configuredGame=property(text,"gamePath"),mods=property(text,"modsPath");
        if(!configuredGame.empty()&&lower(absolute(fs::u8path(configuredGame),config.parent_path()).u8string())!=lower(base.u8string()))continue;
        if(mods.empty())continue;result.manager={true,config,"Manager storage detected"};result.searchRoots.push_back(absolute(fs::u8path(mods),config.parent_path()));break;}
    if(!result.manager.valid)result.manager={false,paths.managerConfig,"Manager config not detected. A Mods folder can be set manually."};
    if(!paths.modsDirectory.empty())result.searchRoots.insert(result.searchRoots.begin(),absolute(paths.modsDirectory,base));
    result.searchRoots.push_back(base/L"nexus_downloads/dmm_update/mods");result.searchRoots.push_back(base/L"mods");
    result.manifest.path=paths.manifest.empty()?base/L"bin64/dmm/manifest.tsv":absolute(paths.manifest,base);
    result.manifest.valid=regular(result.manifest.path);result.manifest.detail=result.manifest.valid?"Active file list found":"Active file list not found. Mount through DMM or set its location.";
    const auto routes=readRoutes(result.manifest.path);
    const auto body=routes.find("character/model/1_pc/2_phw/nude/phw_00_nude_00_0001_damian.pac");
    if(body!=routes.end()){
        if(paths.venusBody.empty())result.venus=checkedVenus(body->second);
        auto parent=body->second.parent_path();for(int i=0;i<16&&!parent.empty();++i){if(lower(parent.filename().u8string())=="mods"){result.searchRoots.push_back(parent);break;}const auto next=parent.parent_path();if(next==parent)break;parent=next;}
    }
    if(!paths.venusBody.empty())result.venus=checkedVenus(absolute(paths.venusBody,base));
    std::vector<fs::path> roots;std::set<std::string> seen;
    for(const auto& root:result.searchRoots)if(directory(root)&&!isOwnModContent(root)&&seen.insert(lower(root.u8string())).second)roots.push_back(root);result.searchRoots=roots;
    size_t hexPriority=hexPresetVersions.size();
    if(!paths.hexPreset.empty())result.hex=checkedHex(absolute(paths.hexPreset,base));
    for(const auto& root:roots){
        std::vector<std::pair<fs::path,int>> directories={{root,0}};size_t cursor=0;
        while(cursor<directories.size()&&cursor<1024){const auto row=directories[cursor++];
            if(paths.hexPreset.empty())for(size_t priority=0;priority<hexPriority;++priority){
                const auto candidate=row.first/hexPresetVersions[priority].filename;
                if(regular(candidate)){auto found=checkedHex(candidate);if(found.valid&&sha256(candidate)==hexPresetVersions[priority].sha256){result.hex=found;hexPriority=priority;break;}}
            }
            if(paths.hexPreset.empty()&&!result.hex.valid){std::error_code scanError;for(const auto& entry:fs::directory_iterator(row.first,fs::directory_options::skip_permission_denied,scanError)){
                const auto name=lower(entry.path().filename().u8string());if(!entry.is_regular_file(scanError)||name.rfind("preset-f-hentaibody",0)!=0||entry.path().extension()!=L".ini")continue;auto found=checkedHex(entry.path());if(found.valid){result.hex=found;break;}
            }}
            if(result.hex.valid&&(!paths.hexPreset.empty()||hexPriority==0))break;
            if(row.second>=3)continue;std::error_code ec;
            for(const auto& entry:fs::directory_iterator(row.first,fs::directory_options::skip_permission_denied,ec)){
                if(!entry.is_directory(ec)||entry.is_symlink(ec)||isOwnModContent(entry.path()))continue;
                const auto name=lower(entry.path().filename().u8string());if(name=="character"||name=="ui"||name=="texture"||name=="textures"||name=="node_modules"||name==".git")continue;
                if(directories.size()<1024)directories.emplace_back(entry.path(),row.second+1);
            }
        }
    }
    if(result.venus.path.empty())result.venus.detail="VENUS active route not found. Mount one Human Body or Elf Body variant through DMM.";
    if(result.hex.path.empty())result.hex.detail="Hex preset not found. Set its path to enable HEX.";
    result.bodyVerified=result.venus.valid&&verifyMountedDependencies(base,result.manifest.path,result.bodyDetail,result.bodyWarning);
    if(result.bodyVerified)result.bodyDetail=result.bodyWarning.empty()?"VENUS and witch body routes verified in the active file list.":result.bodyWarning;
    else if(result.bodyDetail.empty())result.bodyDetail=result.venus.detail;
    return result;
}
bool loadDependencyPaths(const fs::path& file,DependencyPaths& paths,std::string& error) {
    if(!regular(file))return true;std::ifstream input(file);if(!input){error="Dependency paths could not be read.";return false;}DependencyPaths next;std::string line;bool hasDancerSource=false;
    try {while(std::getline(input,line)){
        if(line.size()>32768){error="A dependency path is too long.";return false;}if(!line.empty()&&line.back()=='\r')line.pop_back();const auto eq=line.find('=');if(eq==std::string::npos)continue;
        const auto key=line.substr(0,eq),value=line.substr(eq+1);
        if(std::any_of(value.begin(),value.end(),[](unsigned char c){return c<32;})){error="A dependency path contains a control character.";return false;}
        const auto path=fs::u8path(value);
        if(key=="ManagerConfig")next.managerConfig=path;else if(key=="ModsDirectory")next.modsDirectory=path;else if(key=="ActiveManifest")next.manifest=path;else if(key=="HexPreset")next.hexPreset=path;else if(key=="VenusBody")next.venusBody=path;else if(key=="DancerSource"){next.legacyOutfitSource=path;hasDancerSource=true;}else if(key=="WeightReference"&&!hasDancerSource)next.legacyOutfitSource=path;
    }}catch(...){error="A dependency path is not valid UTF-8.";return false;}paths=next;return true;
}
bool saveDependencyPaths(const fs::path& file,const DependencyPaths& paths,std::string& error) {
    for(const auto& path:{paths.managerConfig,paths.modsDirectory,paths.manifest,paths.hexPreset,paths.venusBody,paths.legacyOutfitSource}){
        const auto value=path.native();if(value.size()>32767||std::any_of(value.begin(),value.end(),[](wchar_t c){return c<32;})){error="A dependency path is too long or contains a control character.";return false;}
    }
    const fs::path temporary=file.wstring()+L".tmp";std::error_code ec;fs::create_directories(file.parent_path(),ec);if(ec){error="Dependency settings folder is unavailable.";return false;}
    std::ofstream out(temporary,std::ios::binary|std::ios::trunc);if(!out){error="Dependency paths could not be saved.";return false;}
    out<<"[Dependencies]\nVersion=1\nManagerConfig="<<paths.managerConfig.u8string()<<"\nModsDirectory="<<paths.modsDirectory.u8string()<<"\nActiveManifest="<<paths.manifest.u8string()<<"\nHexPreset="<<paths.hexPreset.u8string()<<"\nVenusBody="<<paths.venusBody.u8string()<<"\nDancerSource="<<paths.legacyOutfitSource.u8string()<<'\n';
    out.flush();const bool written=bool(out);out.close();if(!written||!MoveFileExW(temporary.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){error="Dependency paths could not be saved atomically.";return false;}return true;
}
std::string profileDependencyIssue(const Profile& profile,bool shaping,bool hasHex,bool weightReady) {
    if(!shaping)return "Shaping is unavailable. Check VENUS, Instant Surgery and active body files in Settings.";
    if(((profile.top==Preset::Hex&&profile.topBlend>0)||(profile.bottom==Preset::Hex&&profile.bottomBlend>0))&&!hasHex)return "HEX is unavailable. Install or locate Hex's required preset in Settings.";
    if(profile.weight!=0&&!weightReady)return "Native Weight controls are unavailable. Check shaping status in Settings; no outfit dependency is required.";
    return {};
}
}
