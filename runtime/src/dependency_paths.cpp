#include "mounted_dependencies.h"
#include "runtime.h"
#include "game_layout.h"
#include "hex_presets.h"
#include "venus_bodies.h"
#include "creator_versions.h"
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
    auto value=input.native();const auto first=value.find_first_not_of(L" \t\r\n"),last=value.find_last_not_of(L" \t\r\n");
    if(first==std::wstring::npos)return {};value=value.substr(first,last-first+1);
    if(value.size()>=2&&((value.front()==L'"'&&value.back()==L'"')||(value.front()==L'\''&&value.back()==L'\'')))value=value.substr(1,value.size()-2);
    if(value.empty())return {};
    wchar_t expanded[32768]{};const auto length=ExpandEnvironmentStringsW(value.c_str(),expanded,32768);if(!length||length>32768)return {};
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
    result.valid=readableDependency(p,"PAR ");result.verified=result.valid&&expected&&sha256(p,true)==expected;result.detail=!result.valid?std::string(label)+" is unreadable or not a PAC file.":result.verified?"Checked external dependency":unverifiedDependency(label);return result;
}
DependencyFile checkedVenus(const fs::path& path){
    auto result=checked(path,nullptr,"VENUS Body");
    if(result.valid){const auto* body=venusBodyVersion(sha256(path,true));result.verified=body!=nullptr;result.detail=body?std::string("Verified VENUS ")+body->version+" / "+body->variant:unverifiedDependency("VENUS Body");}
    return result;
}
DependencyFile checkedHex(const fs::path& path,bool explicitlySelected=false){
    DependencyFile result;result.path=path;
    if(path.empty()||!regular(path)){result.detail="Hex preset not found. Set its path in Settings.";return result;}
    if(isOwnModContent(path)){result.detail="Hex must remain a separate installed dependency.";return result;}
    const auto extension=lower(path.extension().u8string());
    if(extension==".zip"||extension==".7z"||extension==".rar"){result.detail="Extract the preset archive, then select its .ini file or extracted folder.";return result;}
    const auto text=readText(path);
    const bool exported=text.find("InstantSurgeryOutfit=")!=std::string::npos&&text.find("Shape.")!=std::string::npos;
    const bool saved=explicitlySelected&&text.find("[General]")!=std::string::npos&&text.find("[Shape]")!=std::string::npos;
    result.valid=exported||saved;
    const auto* version=result.valid?hexPresetVersion(sha256(path)):nullptr;
    result.verified=version!=nullptr;
    result.detail=!result.valid?"Choose a readable Instant Surgery preset .ini.":version?std::string("Checked external Hex ")+version->version:unverifiedDependency(explicitlySelected?"the selected Instant Surgery preset":"Hex's Hentai Preset");return result;
}
DependencyFile findHexPreset(const std::vector<fs::path>& roots,BodyMode mode){
    DependencyFile best;size_t rank=hexPresetVersions.size()*2+2;
    std::set<std::wstring> visited;
    for(const auto& root:roots){
        std::vector<std::pair<fs::path,int>> directories={{root,0}};size_t cursor=0,files=0;
        while(cursor<directories.size()&&cursor<2048&&files<4096){
            const auto row=directories[cursor++];const auto resolved=absolute(row.first,root);
            if(!directory(resolved)||isOwnModContent(resolved)||!visited.insert(resolved.native()).second)continue;
            for(size_t priority=0;priority<hexPresetVersions.size();++priority){
                const auto candidate=resolved/hexPresetVersions[priority].filename;
                if(!regular(candidate))continue;auto found=checkedHex(candidate);
                const auto score=priority+(mode==BodyMode::Vanilla?hexPresetVersions.size():0);
                if(found.valid&&found.verified&&score<rank){best=std::move(found);rank=score;if(!rank)return best;}
            }
            std::error_code ec;std::vector<fs::path> children,presets;
            for(const auto& entry:fs::directory_iterator(resolved,fs::directory_options::skip_permission_denied,ec)){
                if(entry.is_regular_file(ec)&&lower(entry.path().extension().u8string())==".ini")presets.push_back(entry.path());
                else if(row.second<8&&entry.is_directory(ec)){
                    const auto name=lower(entry.path().filename().u8string());
                    if(name!="character"&&name!="ui"&&name!="texture"&&name!="textures"&&name!="node_modules"&&name!=".git"&&name!="backups")children.push_back(entry.path());
                }
            }
            std::sort(presets.begin(),presets.end());std::sort(children.begin(),children.end());
            for(const auto& candidate:presets){
                if(++files>4096)break;std::error_code sizeError;const auto size=fs::file_size(candidate,sizeError);if(sizeError||size>1024*1024)continue;
                const auto hash=sha256(candidate);size_t priority=hexPresetVersions.size();
                if(const auto* version=hexPresetVersion(hash))priority=size_t(version-hexPresetVersions.data())+(mode==BodyMode::Vanilla?hexPresetVersions.size():0);
                else{
                    const auto filename=lower(candidate.filename().u8string()),text=lower(readText(candidate));
                    const auto marker=text.find("instantsurgeryoutfit=");const auto end=marker==std::string::npos?0:text.find('\n',marker);
                    const auto title=marker==std::string::npos?std::string{}:text.substr(marker,end==std::string::npos?std::string::npos:end-marker);
                    if(filename.find("hentaibody")==std::string::npos&&title.find("hentaibody")==std::string::npos)continue;
                    if(mode==BodyMode::Vanilla)priority=filename.find("venus")==std::string::npos&&title.find("venus")==std::string::npos?0:hexPresetVersions.size()*2;
                }
                if(priority>=rank)continue;auto found=checkedHex(candidate);
                if(found.valid){best=std::move(found);rank=priority;if(!rank)return best;}
            }
            for(const auto& child:children)if(directories.size()<2048)directories.emplace_back(child,row.second+1);
        }
    }
    return best;
}
std::vector<fs::path> configCandidates(const fs::path& game) {
    std::vector<fs::path> result={game/L"nexus_downloads/dmm_update/config.json",game/L"config.json",game/L"bin64/dmm/config.json"};
    const auto snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);if(snapshot==INVALID_HANDLE_VALUE)return result;
    PROCESSENTRY32W entry{};entry.dwSize=sizeof(entry);
    if(Process32FirstW(snapshot,&entry))do{if(_wcsicmp(entry.szExeFile,L"DMM.exe")&&_wcsicmp(entry.szExeFile,L"DefinitiveModManager.exe")&&_wcsicmp(entry.szExeFile,L"Definitive Mod Manager.exe"))continue;
        const auto process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,entry.th32ProcessID);if(!process)continue;
        wchar_t path[32768]{};DWORD size=32768;if(QueryFullProcessImageNameW(process,0,path,&size))result.push_back(fs::path(path).parent_path()/L"config.json");CloseHandle(process);
    }while(Process32NextW(snapshot,&entry));CloseHandle(snapshot);return result;
}
void detectUnderwearSources(const std::map<std::string,fs::path>& routes,DependencyStatus& result){
    struct RemovalMod {const char* folder;const char* label;};
    constexpr RemovalMod mods[]={
        {"remove underwear spouse and npc","Remove Underwear – Spouse and NPC"},
        {"no underwear npcs","No Underwear – NPCs"},
        {"nude character remove character underwear","Nude Character – Remove Character Underwear"},
        {"underwear be gone","Underwear Be Gone"},
        {"hide underwear necklace lantern and lantern rings","Hide Underwear, Necklace, Lantern and Lantern Rings"}
    };
    constexpr const char* appearances[][2]={
        {"cd_nhw_south_20166.app_xml",nullptr},{"cd_nhw_north_20026.app_xml","cd_nhw_south_20167.app_xml"},
        {"cd_nhw_south_20165.app_xml",nullptr},{"cd_nhw_south_20168.app_xml",nullptr},
        {"cd_nhw_south_20169.app_xml",nullptr},{"cd_m0001_00_phw_north_44001.app_xml",nullptr}
    };
    for(const auto& [route,path]:routes){
        const auto slash=route.find_last_of('/');
        const auto file=route.substr(slash==std::string::npos?0:slash+1);
        uint32_t targets=0;
        if(result.bodyMode==BodyMode::Venus&&route.find("/38_underwear/")!=std::string::npos&&
            (file=="phw_00_uw_00_0001.pac"||file=="phw_00_uw_00_0002.pac"||
             file=="cd_phw_00_uw_00_0001.prefab"||file=="cd_phw_00_uw_00_0002.prefab"))targets=63u;
        if(route.rfind("character/appearance/",0)==0){
            for(unsigned i=0;i<result.underwearSources.size();++i)
                for(const auto* appearance:appearances[i])if(appearance&&file==appearance)targets|=1u<<i;
        }
        if(!targets||!regular(path))continue;
        auto folder=path.parent_path();
        for(unsigned depth=0;depth<16&&!folder.empty();++depth){
            auto name=lower(folder.filename().u8string());
            for(auto& c:name)if(!std::isalnum(static_cast<unsigned char>(c)))c=' ';
            std::istringstream words(name);std::string clean,word;
            while(words>>word){if(!clean.empty())clean+=' ';clean+=word;}
            const auto mod=std::find_if(std::begin(mods),std::end(mods),[&](const auto& entry){return clean.find(entry.folder)!=std::string::npos;});
            if(mod!=std::end(mods)){
                for(unsigned i=0;i<result.underwearSources.size();++i)if(targets&(1u<<i)){
                    auto& source=result.underwearSources[i];
                    if(source.empty())source=mod->label;
                    else if(source.find(mod->label)==std::string::npos)source+=", "+std::string(mod->label);
                }
                break;
            }
            const auto parent=folder.parent_path();if(parent==folder)break;folder=parent;
        }
    }
}
std::vector<fs::path> servedModRoots(const std::map<std::string,fs::path>& routes){
    std::vector<fs::path> roots;std::set<std::string> seen,parents;
    for(const auto& row:routes){
        auto parent=row.second.parent_path();
        for(int depth=0;depth<16&&!parent.empty();++depth){
            if(!parents.insert(lower(parent.u8string())).second)break;
            if(regular(parent/L"mod.json")){
                const auto root=parent.parent_path();
                if(!root.empty()&&!isOwnModContent(root)&&seen.insert(lower(root.u8string())).second)roots.push_back(root);
            }
            const auto next=parent.parent_path();if(next==parent)break;parent=next;
        }
        if(roots.size()>=64)break;
    }
    return roots;
}
bool compatibilityRoute(const std::string& route){
    static constexpr const char* patterns[]={
        "cd_m0001_00_phw_north_44001","cd_nhw_north_20026","cd_nhw_south_20092","cd_nhw_south_20165",
        "cd_nhw_south_20166","cd_nhw_south_20167","cd_nhw_south_20168","cd_nhw_south_20169",
        "/38_underwear/","partprefabtable.pappt","cd_phw_damian_customization","nude_00_0001_damian",
        "hva_00_nude_00_0001","phw_00_nude_00_0001.dds","phw_00_nude_00_0001_hand.dds","phw_00_head_00_0001_01.dds",
        "ha_phw_00_foot_","meshparam_spouse_phw","partinoutsocket"};
    return std::any_of(std::begin(patterns),std::end(patterns),[&](const auto* pattern){return route.find(pattern)!=std::string::npos;});
}

void captureMountedSources(const std::map<std::string,fs::path>& routes,const std::vector<fs::path>& storage,DependencyStatus& status){
    std::vector<std::string> prefixes;
    for(const auto& root:storage)if(!root.empty())prefixes.push_back(lower(root.generic_u8string())+"/");
    std::map<std::string,std::string> titles;
    for(const auto& [resource,path]:routes){
        const auto file=path.generic_u8string(),folded=lower(file);
        std::string owner;
        for(const auto& prefix:prefixes){
            if(folded.rfind(prefix,0)!=0)continue;
            const auto end=file.find('/',prefix.size());
            if(end==std::string::npos||end==prefix.size())continue;
            const auto root=file.substr(0,end);
            const auto [entry,inserted]=titles.emplace(lower(root),file.substr(prefix.size(),end-prefix.size()));
            if(inserted){
                const auto folder=fs::u8path(root);
                auto title=property(readText(folder/L"modinfo.json",65536),"title");
                if(title.empty()){
                    const auto metadata=readText(folder/L"mod.json",65536);
                    title=property(metadata,"title");
                    if(title.empty())title=property(metadata,"name");
                }
                if(!title.empty()&&title.size()<=200&&std::none_of(title.begin(),title.end(),[](unsigned char c){return c<32;}))entry->second=std::move(title);
            }
            owner=entry->second;
            ++status.mountedMods[owner];
            break;
        }
        if(compatibilityRoute(resource)||resource==status.skinTone.route||
           (resource==damianeCustomizationRoute&&(status.skinTone.brightness||status.skinTone.colour))){
            if(owner.empty()&&folded.find("/dmm/files/")!=std::string::npos)owner="DMM merged output";
            status.relevantRoutes.push_back({resource,owner,path});
        }
    }
}

void captureMountLog(const fs::path& game,DependencyStatus& status){
    const fs::path candidates[]={status.manager.path.parent_path()/L"mount_log.txt",game/L"nexus_downloads/dmm_update/mount_log.txt"};
    for(const auto& path:candidates){
        if(path.is_relative()||!regular(path))continue;
        const auto contents=readText(path,8*1024*1024);
        if(contents.empty())continue;
        status.managerMountLog=path;
        std::istringstream lines(contents);std::string line;
        while(std::getline(lines,line)&&status.managerFindings.size()<150){
            if(!line.empty()&&line.back()=='\r')line.pop_back();
            const bool summary=line.rfind("[ERROR]",0)==0||line.rfind("[RUNTIME] done",0)==0;
            const bool route=line.rfind("[XML PATCH]",0)==0||line.rfind("[RUNTIME_CONFLICT]",0)==0||
                line.rfind("[RUNTIME_MERGE]",0)==0||line.rfind("[RUNTIME_LOOK_CLASH]",0)==0;
            if((summary||(route&&compatibilityRoute(lower(line))))&&line.size()<=2048)status.managerFindings.push_back(std::move(line));
        }
        return;
    }
}

}
bool isOwnModContent(const std::filesystem::path& path) {
    std::error_code ec;const auto canonical=fs::weakly_canonical(path,ec);const auto& candidate=ec?path:canonical;
    for(const auto& part:candidate){const auto name=lower(part.u8string());if(name=="hex appeal"||name=="hexappeal"||name.rfind("hex appeal -",0)==0)return true;}return false;
}
DependencyStatus inspectDependencies(const fs::path& game,const DependencyPaths& paths) {
    DependencyStatus result;result.scanned=true;const auto base=absolute(game,game);
    const auto nativeFile=[&](const fs::path& path,const char* hash,const char* label){DependencyFile file;file.path=path;file.valid=readableDependency(path);const auto digest=file.valid?sha256(path,true):std::string{};const std::string_view name=label;file.verified=file.valid&&((hash&&digest==hash)||(name=="Crimson Desert"&&checkedGameDigest(digest))||(name=="Character Creator"&&verifiedCreatorVersion(digest)));file.detail=!file.valid?std::string(label)+" is not installed or its file is unreadable.":file.verified?"Verified installed version":unverifiedDependency(label);return file;};
    result.game=nativeFile(base/L"bin64/CrimsonDesert.exe",nullptr,"Crimson Desert");
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
    auto storage=result.searchRoots;
    result.searchRoots.push_back(base/L"bin64/InstantSurgery/Outfits");result.searchRoots.push_back(base/L"bin64/InstantSurgery");
    result.searchRoots.push_back(base/L"InstantSurgery/Outfits");result.searchRoots.push_back(base/L"InstantSurgery");result.searchRoots.push_back(base/L"bin64");
    wchar_t local[32768]{};const auto localLength=GetEnvironmentVariableW(L"LOCALAPPDATA",local,32768);
    if(localLength&&localLength<32768){result.searchRoots.push_back(fs::path(local)/L"InstantSurgery/Outfits");result.searchRoots.push_back(fs::path(local)/L"InstantSurgery");}
    result.manifest.path=paths.manifest.empty()?base/L"bin64/dmm/manifest.tsv":absolute(paths.manifest,base);
    if(paths.manifest.empty()){
        wchar_t runtimeFile[32768]{};const auto module=GetModuleHandleW(L"DmmRuntime.asi");
        if(module&&GetModuleFileNameW(module,runtimeFile,32768)){
            const auto candidate=fs::path(runtimeFile).parent_path()/L"dmm/manifest.tsv";
            if(regular(candidate))result.manifest.path=candidate;
        }
    }
    result.manifest.valid=regular(result.manifest.path);result.manifest.detail=result.manifest.valid?"Active file list found":"Active file list not found. Mount through DMM or set its location.";
    const auto routes=readMountedRoutes(result.manifest.path);
    result.skinTone=result.manifest.valid?activeVenusDamianeSkin(routes):
        SkinToneSource{{},{},"DMM’s active file list is unavailable."};
    std::vector<MountedBodyIssue> bodyIssues;
    result.bodyMode=mountedBodyMode(routes,nullptr,&result.unavailableBodyWitches,&bodyIssues);
    if(result.bodyMode==BodyMode::Invalid){
        const bool noMountedBody=std::none_of(bodyIssues.begin(),bodyIssues.end(),[&](const auto& issue){return routes.count(issue.resource)!=0;});
        const bool hasKnownBodies=std::any_of(bodyIssues.begin(),bodyIssues.end(),[](const auto& issue){
            return issue.knownBody;
        });
        if(noMountedBody||hasKnownBodies){
            result.bodyMode=BodyMode::Live;
            if(noMountedBody){
                result.unavailableBodyWitches=0;
                bodyIssues.clear();
            }
        }
    }
    detectUnderwearSources(routes,result);
    bool underwearMeshRemoved=false;
    const auto npcUnderwear=mountedNpcUnderwearWitches(routes,underwearMeshRemoved);
    result.confirmedUnderwearRules=npcUnderwear;
    for(unsigned i=0;i<result.underwearSources.size();++i)if(npcUnderwear&(1u<<i))
        result.underwearSources[i]="No Underwear – NPCs";
    if(underwearMeshRemoved)result.missingUnderwearMeshMask=npcUnderwear;
    if(const auto barber=routes.find("character/descriptors/customizationmeta/meshparam_spouse_phw.xml");barber!=routes.end())result.barberMeshDefinition=barber->second;
    {
        const auto removers=mountedBodyUnderwearRemovers(routes);
        for(size_t i=0;i<removers.size();++i)if(!removers[i].empty()){
            result.missingUnderwearMeshMask|=1u<<i;
            auto& owner=result.underwearSources[i];
            owner="mounted body replacement";
            auto folder=removers[i].parent_path();
            for(unsigned depth=0;depth<12&&!folder.empty();++depth){
                const auto info=readText(folder/L"modinfo.json",65536);
                if(!info.empty()){
                    const bool shared=lower(removers[i].stem().u8string())=="phw_00_nude_00_0001_damian";
                    auto name=property(info,shared?"title":"generator");
                    if(name.empty())name=property(info,shared?"generator":"title");
                    if(!name.empty()&&name.size()<=120&&std::none_of(name.begin(),name.end(),[](unsigned char c){return c<32;}))
                        owner=std::move(name);
                    break;
                }
                const auto parent=folder.parent_path();if(parent==folder)break;folder=parent;
            }
        }
    }
    const auto servedRoots=servedModRoots(routes);
    result.searchRoots.insert(result.searchRoots.end(),servedRoots.begin(),servedRoots.end());
    if(!result.manager.valid&&paths.managerConfig.empty())for(const auto& root:servedRoots){
        const auto candidate=root.parent_path()/L"config.json";const auto text=readText(candidate);const auto mods=property(text,"modsPath"),configuredGame=property(text,"gamePath");
        if(mods.empty()||(!configuredGame.empty()&&lower(absolute(fs::u8path(configuredGame),candidate.parent_path()).u8string())!=lower(base.u8string())))continue;
        const auto located=absolute(fs::u8path(mods),candidate.parent_path());
        result.manager={true,candidate,"Manager storage detected from active files"};result.searchRoots.insert(result.searchRoots.begin(),located);storage.insert(storage.begin(),located);break;
    }
    captureMountedSources(routes,storage,result);
    captureMountLog(base,result);
    for(const auto& issue:bodyIssues){
        auto& detail=result.unavailableBodyDetails[issue.witch];
        if(!detail.empty())detail+="\n\n";
        detail+=issue.reason+"\nGame file: "+issue.resource;
        const auto route=std::find_if(result.relevantRoutes.begin(),result.relevantRoutes.end(),[&](const auto& row){
            return row.resource==issue.resource;
        });
        if(route!=result.relevantRoutes.end()){
            detail+="\nAppearance supplied by "+(route->sourceName.empty()?route->sourceFile.u8string():route->sourceName);
            for(const auto& finding:result.managerFindings){
                if(finding.rfind("[RUNTIME_LOOK_CLASH]",0)!=0||finding.find(issue.resource)==std::string::npos)continue;
                detail+="\nLast DMM mount log: "+finding;
                break;
            }
        }
    }
    if(!result.manager.valid&&!servedRoots.empty())result.manager={true,result.manifest.path,"Mod storage located from DMM's active file list"};
    const auto body=routes.find("character/model/1_pc/2_phw/nude/phw_00_nude_00_0001_damian.pac");
    if(body!=routes.end()&&result.bodyMode!=BodyMode::Vanilla){
        if(paths.venusBody.empty())result.venus=checkedVenus(body->second);
        auto parent=body->second.parent_path();for(int i=0;i<16&&!parent.empty();++i){if(lower(parent.filename().u8string())=="mods"){result.searchRoots.push_back(parent);break;}const auto next=parent.parent_path();if(next==parent)break;parent=next;}
    }
    if(!paths.venusBody.empty()&&result.bodyMode!=BodyMode::Vanilla)result.venus=checkedVenus(absolute(paths.venusBody,base));
    std::vector<fs::path> roots;std::set<std::string> seen;
    for(const auto& root:result.searchRoots)if(directory(root)&&!isOwnModContent(root)&&seen.insert(lower(root.u8string())).second)roots.push_back(root);result.searchRoots=roots;
    if(paths.hexPreset.empty())result.hex=findHexPreset(roots,result.bodyMode);
    else{
        const auto selected=absolute(paths.hexPreset,base);
        if(directory(selected)){
            result.hex=findHexPreset({selected},result.bodyMode);
            if(!result.hex.valid){result.hex.path=selected;result.hex.detail=isOwnModContent(selected)?"Hex must remain a separate installed dependency.":"No supported Hex preset found in this folder. Select the extracted preset .ini file.";}
        }else{
            result.hex=checkedHex(selected,true);
            result.customPreset=result.hex.valid&&!result.hex.verified;
        }
    }
    if(localLength&&localLength<32768){
        const auto library=fs::path(local)/L"InstantSurgery";
        const auto saved=library/L"Preset.ini";
        if(checkedHex(saved,true).valid)result.surgeryPresets.push_back(saved);
        std::error_code ec;
        for(const auto& entry:fs::directory_iterator(library/L"Outfits",fs::directory_options::skip_permission_denied,ec)){
            if(result.surgeryPresets.size()>=64)break;
            if(lower(entry.path().extension().u8string())==".ini"&&checkedHex(entry.path(),true).valid)result.surgeryPresets.push_back(entry.path());
        }
        std::sort(result.surgeryPresets.begin(),result.surgeryPresets.end());
    }
    if(result.venus.path.empty())result.venus.detail=result.bodyMode==BodyMode::Vanilla?"VENUS is not required by the selected Vanilla body option.":"VENUS active route not found. Mount one Human Body or Elf Body variant through DMM.";
    if(result.hex.path.empty())result.hex.detail="Hex preset not found. Set its path to enable HEX.";
    else if(result.hex.valid&&result.bodyMode==BodyMode::Vanilla)result.hex.detail+=" The preset values are applied relative to each witch's original body.";
    result.bodyVerified=result.bodySourceReady()&&verifyMountedDependencies(base,result.manifest.path,result.bodyDetail,result.bodyWarning,&result.unavailableBodyWitches,&result.bodyNote);
    if(result.bodyMode==BodyMode::Invalid)result.bodyDetail="The mounted witch bodies are missing or mixed. Enable one Hex Appeal body option, remove any stale older entry and remount.";
    if(result.bodyVerified)result.bodyDetail=result.bodyWarning.empty()?(result.bodyMode==BodyMode::Vanilla?"Vanilla body selected explicitly; all seven witch body routes checked. VENUS is not required.":"VENUS and witch body routes verified in the active file list."):result.bodyWarning;
    else if(result.bodyMode==BodyMode::Live)result.bodyDetail="Shape checks each character's live body and outfit skeletons before applying.";
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
    if(!shaping)return "Shaping is unavailable. Check the selected body, Instant Surgery and active body files in Settings.";
    if(((profile.top==Preset::Hex&&profile.topBlend>0)||(profile.bottom==Preset::Hex&&profile.bottomBlend>0))&&!hasHex)return "The preset is unavailable. Select an Instant Surgery preset or use automatic Hex detection in Settings.";
    if(profile.weight!=0&&!weightReady)return "Native Weight controls are unavailable. Check shaping status in Settings; no outfit dependency is required.";
    return {};
}
}
