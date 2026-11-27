#include "mounted_dependencies.h"
#include "game_archive.h"
#include "runtime.h"
#include "venus_bodies.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <map>
#include <cstring>
#include <regex>
#include <set>
#include <Windows.h>

namespace hex {
namespace {
struct BodyAppearance {const char* path;const char* vanilla;int witch;bool stockUsesDamiane=false;};
constexpr BodyAppearance bodyAppearances[]={
    {"character/appearance/2_mon/cd_m0001_00_twofeet/cd_m0001_00_phw_north/cd_m0001_00_phw_north_44001.app_xml","hvw_00_nude_00_0001",5},
    {"character/appearance/3_npc/cd_npc_north/cd_nhw_north/cd_nhw_north_20026.app_xml","hvw_00_nude_00_0301",1},
    {"character/appearance/3_npc/cd_npc_south/cd_nhw_south/cd_nhw_south_20165.app_xml","hvw_00_nude_00_0001",2},
    {"character/appearance/3_npc/cd_npc_south/cd_nhw_south/cd_nhw_south_20166.app_xml","hvw_00_nude_00_0001",0},
    {"character/appearance/3_npc/cd_npc_south/cd_nhw_south/cd_nhw_south_20167.app_xml","hvw_00_nude_00_0001",1},
    {"character/appearance/3_npc/cd_npc_south/cd_nhw_south/cd_nhw_south_20168.app_xml","hvw_00_nude_00_0001",3},
    {"character/appearance/3_npc/cd_npc_south/cd_nhw_south/cd_nhw_south_20169.app_xml","hvw_00_nude_00_0001_damian",4,true}};
constexpr auto npcUnderwearPrefab="phw_00_nude_00_0001_damian_lnw";
constexpr auto npcUnderwearPrefabPath="character/bin__/prefab/1_pc/02_phw/nude/phw_00_nude_00_0001_damian_lnw.prefab";
constexpr auto npcUnderwearMetadataPath="character/prefab/1_pc/02_phw/nude/phw_00_nude_00_0001_damian_lnw.prefabdata_xml";
constexpr auto venusPrefab="phw_00_nude_00_0001_damian";
constexpr auto venusPrefabPath="character/bin__/prefab/1_pc/02_phw/nude/phw_00_nude_00_0001_damian.prefab";
constexpr auto isolatedVenusPrefab="hva_00_nude_00_0001_damian";
constexpr auto isolatedVenusPrefabPath="character/bin__/prefab/ha_p/02_phw/nude/hva_00_nude_00_0001_damian.prefab";
constexpr auto isolatedVenusMetadataPath="character/prefab/ha_p/02_phw/nude/hva_00_nude_00_0001_damian.prefabdata_xml";
constexpr auto venusModelPath="character/model/1_pc/2_phw/nude/phw_00_nude_00_0001_damian.pac";
constexpr auto isolatedVenusModelPath="character/model/ha_p/2_phw/nude/hva_00_nude_00_0001_damian.pac";
constexpr auto isolatedVenusMaterialPath="character/modelproperty/ha_p/2_phw/nude/hva_00_nude_00_0001_damian.pac_xml";
constexpr auto isolatedVenusPhysicsPath="character/bin__/meshphysics/ha_p/2_phw/nude/hva_00_nude_00_0001_damian.hkx";
constexpr auto isolatedBodyColourPath="character/texture/hva_00_nude_00_0001.dds";
constexpr auto isolatedHandColourPath="character/texture/hva_00_nude_00_0001_hand.dds";
constexpr auto isolatedUnderwearModelPath="character/model/ha_p/2_phw/armor/38_underwear/hva_00_uw_00_0001.pac";
constexpr auto isolatedUnderwearMaterialPath="character/modelproperty/ha_p/2_phw/armor/38_underwear/hva_00_uw_00_0001.pac_xml";
constexpr auto isolatedUnderwearPhysicsPath="character/bin__/meshphysics/ha_p/2_phw/armor/38_underwear/hva_00_uw_00_0001.hkx";
bool verifiedNpcUnderwearAssets(const std::map<std::string,std::filesystem::path>& routes){
    const auto prefab=routes.find(npcUnderwearPrefabPath);
    const auto metadata=routes.find(npcUnderwearMetadataPath);
    return prefab!=routes.end()&&metadata!=routes.end()&&
        sha256(prefab->second,true)=="57c226a0e42e43718a7f4221c223186c1511b1da7ecdf0ad0dca37b37a4e0c5e"&&
        sha256(metadata->second,true)=="ead487d55bc0ed194dac4df1294f7bfe14db477e2946ba78eea24de44db95a88";
}
std::string readMountAsset(const std::filesystem::path& path){
    std::error_code ec;const auto bytes=std::filesystem::file_size(path,ec);
    if(ec||!bytes||bytes>1024*1024)return {};
    std::ifstream input(path,std::ios::binary);return {std::istreambuf_iterator<char>(input),{}};
}
bool isolatedVenusReady(const std::map<std::string,std::filesystem::path>& routes){
    const auto prefab=routes.find(isolatedVenusPrefabPath),definition=routes.find(isolatedVenusMetadataPath);
    if(prefab==routes.end()||definition==routes.end())return false;
    const auto data=readMountAsset(prefab->second),metadata=readMountAsset(definition->second);
    if(data.compare(0,6,std::string("\xff\xff\x04\0\0\0",6))!=0||
       metadata.find("<SkeletonName FileName=\"1_pc/2_phw/PHW_01.pab\"")==std::string::npos)return false;
    const bool stockUnderwear=data.find("character/model/1_pc/2_phw/armor/38_underwear/phw_00_uw_00_0001.pac")!=std::string::npos;
    if(data.find(isolatedUnderwearModelPath)!=std::string::npos){
        const auto model=routes.find(isolatedUnderwearModelPath),material=routes.find(isolatedUnderwearMaterialPath),physics=routes.find(isolatedUnderwearPhysicsPath);
        if(stockUnderwear||model==routes.end()||material==routes.end()||physics==routes.end()||
           !readableDependency(model->second,"PAR ")||!readableDependency(material->second)||!readableDependency(physics->second))return false;
    }else if(!stockUnderwear)return false;
    if(data.find(venusModelPath)!=std::string::npos)return true;
    if(data.find(isolatedVenusModelPath)==std::string::npos)return false;
    const auto model=routes.find(isolatedVenusModelPath),material=routes.find(isolatedVenusMaterialPath),physics=routes.find(isolatedVenusPhysicsPath);
    const auto body=routes.find(isolatedBodyColourPath),hands=routes.find(isolatedHandColourPath);
    if(model==routes.end()||material==routes.end()||physics==routes.end()||body==routes.end()||hands==routes.end())return false;
    const auto bindings=readMountAsset(material->second);
    return readableDependency(model->second,"PAR ")&&readableDependency(physics->second)&&
        readableDependency(body->second,"DDS ")&&readableDependency(hands->second,"DDS ")&&
        bindings.find(isolatedBodyColourPath)!=std::string::npos&&bindings.find(isolatedHandColourPath)!=std::string::npos;
}
std::string lowerAscii(std::string text){
    std::transform(text.begin(),text.end(),text.begin(),[](unsigned char c){return char(std::tolower(c));});
    return text;
}
// The game matches prefab names case-insensitively, and DMM's routes are lower case.
// Generated looks such as Legion's Slider ..._IAL bodies keep capitals in the name.
std::string appearanceBodyFromXml(const std::string& text){
    static const std::regex nude(R"(<Nude\s*>\s*(<Prefab\s+[^<>]*/>)\s*</Nude\s*>)");
    static const std::regex name(R"(\bName\s*=\s*(["'])([^"']+)\1)");
    std::smatch match;if(!std::regex_search(text,match,nude)||std::regex_search(match.suffix().str(),nude))return {};
    const auto part=match[1].str();std::smatch named;return std::regex_search(part,named,name)?lowerAscii(named[2].str()):std::string{};
}
std::string appearanceBody(const std::filesystem::path& path){return appearanceBodyFromXml(readMountAsset(path));}
bool checkedStockDamianeAppearance(const BodyAppearance& row,const std::filesystem::path& game){
    if(!row.stockUsesDamiane||game.empty())return false;
    const std::string_view route(row.path);
    try{
        const GameArchive archive(game/L"0009",route.substr(0,route.find_last_of('/')));
        return appearanceBodyFromXml(archive.read(row.path))==venusPrefab;
    }catch(const std::runtime_error&){return false;}
    catch(const std::out_of_range&){return false;}
}
bool cloneUnderwearPathCleared(const std::string& prefab){
    static constexpr char partHeader[]="\x09\0\0\0" "Underwear" "\x01\x01\0\x01\x03\0\0"
        "\xff\xff\xff\xff\xff\xff\xff\xff";
    constexpr size_t headerSize=sizeof(partHeader)-1,resourceIdSize=8;
    const auto tag=prefab.find(partHeader,0,headerSize);
    if(tag==std::string::npos||prefab.find(partHeader,tag+headerSize,headerSize)!=std::string::npos)return false;
    const auto sizeField=tag+headerSize+resourceIdSize;
    uint32_t size=0;
    if(sizeField+sizeof(size)>prefab.size())return false;
    std::memcpy(&size,prefab.data()+sizeField,sizeof(size));
    const auto pathStart=sizeField+sizeof(size);
    return size>0&&size<=512&&size<=prefab.size()-pathStart&&
        prefab.find_first_not_of('\0',pathStart)>=pathStart+size;
}
// sharesVenusModel reports a clone that draws the mounted Venus Body for Damiane model itself,
// such as a copy of the Damiane prefab whose only change is its underwear part.
bool mountedPhwBodyClone(const std::map<std::string,std::filesystem::path>& routes,const std::string& prefabName,std::filesystem::path* remover=nullptr,
                         bool* sharesVenusModel=nullptr){
    constexpr auto root="character/bin__/prefab/";
    if(sharesVenusModel)*sharesVenusModel=false;
    const auto name=lowerAscii(prefabName);
    if(name.empty()||name.size()>96||name==venusPrefab||name==npcUnderwearPrefab||name==isolatedVenusPrefab||
       name.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_")!=std::string::npos)return false;
    const std::string suffix="/"+name+".prefab";
    auto prefab=routes.end();
    for(auto route=routes.lower_bound(root);route!=routes.end()&&route->first.rfind(root,0)==0;++route){
        const auto& key=route->first;
        if(key.size()<suffix.size()||key.compare(key.size()-suffix.size(),suffix.size(),suffix)!=0)continue;
        if(prefab!=routes.end())return false;
        prefab=route;
    }
    if(prefab==routes.end())return false;
    const auto stem=prefab->first.substr(std::strlen(root),prefab->first.size()-std::strlen(root)-std::strlen(".prefab"));
    const auto metadata=routes.find("character/prefab/"+stem+".prefabdata_xml");
    if(metadata==routes.end())return false;
    const auto data=readMountAsset(prefab->second),description=lowerAscii(readMountAsset(metadata->second));
    static const std::regex nudeModel(R"(character/model/[a-zA-Z0-9_/]{1,200}/nude/[a-zA-Z0-9_]{1,96}\.pac)",std::regex::icase);
    std::smatch modelMatch;
    // Resource paths are case-insensitive: the stock rig writes 1_PC/2_PHW/Nude, generated clones 1_pc/2_phw/nude.
    const bool sameRig=data.compare(0,6,std::string("\xff\xff\x04\0\0\0",6))==0&&
        data.find("SkinnedMeshComponent")!=std::string::npos&&
        std::regex_search(data,modelMatch,nudeModel)&&
        description.find("<skeletonname filename=\"1_pc/2_phw/phw_01.pab\"")!=std::string::npos&&
        description.find("<skeletonvariationname filename=\"1_pc/2_phw/nude/phw_00_nude_00_0001.pabc\"")!=std::string::npos;
    if(!sameRig)return false;
    auto modelRoute=modelMatch.str();
    std::transform(modelRoute.begin(),modelRoute.end(),modelRoute.begin(),[](unsigned char c){return char(std::tolower(c));});
    if(const auto model=routes.find(modelRoute);model!=routes.end()){
        if(!readableDependency(model->second,"PAR "))return false;
    }else if(modelRoute!=venusModelPath&&modelRoute!="character/model/1_pc/2_phw/nude/phw_00_nude_00_0001.pac")return false;
    if(remover&&cloneUnderwearPathCleared(data))*remover=prefab->second;
    if(sharesVenusModel)*sharesVenusModel=modelRoute==venusModelPath;
    return true;
}
}
std::array<std::filesystem::path,6> mountedBodyUnderwearRemovers(const std::map<std::string,std::filesystem::path>& routes){
    std::filesystem::path sharedRemover;
    if(const auto shared=routes.find(venusPrefabPath);shared!=routes.end()){
        const auto prefab=readMountAsset(shared->second);
        if(prefab.compare(0,6,std::string("\xff\xff\x04\0\0\0",6))==0&&
           prefab.find(venusModelPath)!=std::string::npos&&cloneUnderwearPathCleared(prefab))sharedRemover=shared->second;
    }
    std::array<std::filesystem::path,6> removers;uint32_t keepsUnderwear=0;
    for(const auto& row:bodyAppearances){
        const auto found=routes.find(row.path);std::filesystem::path remover;
        if(found!=routes.end()){
            const auto body=appearanceBody(found->second);
            if(body==venusPrefab)remover=sharedRemover;
            else mountedPhwBodyClone(routes,body,&remover);
        }
        if(remover.empty())keepsUnderwear|=1u<<row.witch;
        else removers[row.witch]=std::move(remover);
    }
    for(size_t i=0;i<removers.size();++i)if(keepsUnderwear&(1u<<i))removers[i].clear();
    return removers;
}
namespace {
bool npcUnderwearReady(const std::map<std::string,std::filesystem::path>& routes,bool& meshRemoved){
    meshRemoved=false;
    const auto prefab=routes.find(npcUnderwearPrefabPath);
    const auto meta=routes.find(npcUnderwearMetadataPath);
    if(prefab==routes.end()||meta==routes.end())return false;
    const auto data=readMountAsset(prefab->second),description=readMountAsset(meta->second);
    if(data.compare(0,6,std::string("\xff\xff\x04\0\0\0",6))!=0||
       data.find("character/model/1_pc/2_phw/nude/phw_00_nude_00_0001_damian.pac")==std::string::npos||
       data.find("Underwear")==std::string::npos||description.find("1_pc/2_phw/PHW_01.pab")==std::string::npos)return false;
    meshRemoved=data.find("character/model/1_pc/2_phw/armor/38_underwear/")==std::string::npos;
    return true;
}

BodyMode classifyBodyRoutes(const std::map<std::string,std::filesystem::path>& routes,uint32_t* cloneWitches,uint32_t* unavailableWitches,
                           std::vector<MountedBodyIssue>* issues,const std::filesystem::path& game){
    bool meshRemoved=false;
    const bool npcUnderwear=npcUnderwearReady(routes,meshRemoved);
    const bool isolatedVenus=isolatedVenusReady(routes);
    uint32_t venus=0,vanilla=0,clones=0,removerBodies=0,unavailable=0;
    std::vector<MountedBodyIssue> stockRoutes;
    for(const auto& row:bodyAppearances){
        const auto found=routes.find(row.path);
        const auto body=found==routes.end()?std::string{}:appearanceBody(found->second);
        const uint32_t bit=1u<<row.witch;
        if(body==row.vanilla){
            vanilla|=bit;
            if(issues)stockRoutes.push_back({row.witch,row.path,"Vanilla body selected by this appearance.",true});
        }
        else if(body==venusPrefab||(isolatedVenus&&body==isolatedVenusPrefab)){
            venus|=bit;
            if(issues)stockRoutes.push_back({row.witch,row.path,"Venus Body for Damiane selected by this appearance.",true});
        }
        else if(npcUnderwear&&body==npcUnderwearPrefab)removerBodies|=bit;
        else if(mountedPhwBodyClone(routes,body))clones|=bit;
        else if(found==routes.end()&&venus&&!vanilla&&checkedStockDamianeAppearance(row,game)){
            venus|=bit;
            if(issues)stockRoutes.push_back({row.witch,row.path,"The stock appearance already uses the mounted Damiane body.",true});
        }else{
            unavailable|=bit;
            if(issues){
                const auto reason=found==routes.end()?"No active file in the DMM mount.":
                    body.empty()?"The mounted appearance is unreadable or does not declare a body prefab.":
                    "Body prefab "+body+" has no compatible mounted model and skeleton route.";
                issues->push_back({row.witch,row.path,reason});
            }
        }
    }
    if(cloneWitches)*cloneWitches=clones;
    if(!vanilla)venus|=removerBodies;
    if(unavailableWitches)*unavailableWitches=unavailable;
    if(vanilla&&venus){
        if(issues)issues->insert(issues->end(),stockRoutes.begin(),stockRoutes.end());
        return BodyMode::Invalid;
    }
    if(unavailable&&!unavailableWitches)return BodyMode::Invalid;
    if(vanilla&~unavailable)return BodyMode::Vanilla;
    if(venus&&((venus|clones)&~unavailable))return BodyMode::Venus;
    return BodyMode::Invalid;
}

}

std::map<std::string,std::filesystem::path> readMountedRoutes(const std::filesystem::path& manifest,
                                                            bool* supported,std::string* formatWarning){
    if(supported)*supported=false;
    std::ifstream in(manifest);
    const auto directory=manifest.parent_path();
    std::map<std::string,std::filesystem::path> routes;
    std::set<std::string> ambiguous;
    for(std::string line;std::getline(in,line);){
        if(!line.empty()&&line.back()=='\r')line.pop_back();
        if(line.rfind("dmm-runtime\t",0)==0){
            if(supported)*supported=true;
            if(line!="dmm-runtime\t1"&&formatWarning)*formatWarning=unverifiedDependency("DMM's active file-list format");
        }
        if(line.rfind("file\t",0)!=0)continue;
        const auto tab=line.find('\t',5);
        if(tab==std::string::npos||tab==5||tab+1==line.size()||line.find('\0')!=line.npos)continue;
        auto key=line.substr(5,tab-5);
        std::transform(key.begin(),key.end(),key.begin(),[](unsigned char c){return c=='\\'?'/':char(std::tolower(c));});
        if(ambiguous.count(key))continue;
        try{
            auto path=std::filesystem::u8path(line.substr(tab+1));
            if(path.is_relative())path=directory/path;
            path=path.lexically_normal();
            auto [row,inserted]=routes.try_emplace(key,path);
            if(!inserted&&CompareStringOrdinal(row->second.c_str(),-1,path.c_str(),-1,TRUE)!=CSTR_EQUAL){
                routes.erase(row);
                ambiguous.insert(key);
            }
        }catch(const std::filesystem::filesystem_error&){}
    }
    return routes;
}

uint32_t mountedNpcUnderwearWitches(const std::map<std::string,std::filesystem::path>& routes,bool& meshRemoved){
    if(!npcUnderwearReady(routes,meshRemoved))return 0;
    uint32_t mask=0,other=0;
    for(const auto& row:bodyAppearances){
        const auto app=routes.find(row.path);
        if(app!=routes.end()&&appearanceBody(app->second)==npcUnderwearPrefab)mask|=1u<<row.witch;
        else other|=1u<<row.witch;
    }
    return mask&~other;
}
BodyMode mountedBodyMode(const std::filesystem::path& manifest,uint32_t* cloneWitches,uint32_t* unavailableWitches){
    return mountedBodyMode(readMountedRoutes(manifest),cloneWitches,unavailableWitches);
}
BodyMode mountedBodyMode(const std::map<std::string,std::filesystem::path>& routes,uint32_t* cloneWitches,uint32_t* unavailableWitches,
                         std::vector<MountedBodyIssue>* issues,const std::filesystem::path& game){
    return classifyBodyRoutes(routes,cloneWitches,unavailableWitches,issues,game);
}
bool mountedDamianeVenus(const std::map<std::string,std::filesystem::path>& routes){
    const auto appearance=routes.find("character/appearance/1_pc/2_phw/cd_phw_damian/cd_phw_damian_00000.app_xml");
    if(appearance!=routes.end()&&appearanceBody(appearance->second)!=venusPrefab)return false;
    const auto prefab=routes.find(venusPrefabPath);
    if(prefab!=routes.end()){
        const auto data=readMountAsset(prefab->second);
        if(data.compare(0,6,std::string("\xff\xff\x04\0\0\0",6))!=0||data.find(venusModelPath)==data.npos)return false;
    }
    const auto body=routes.find(venusModelPath);
    return body!=routes.end()&&!isOwnModContent(body->second)&&readableDependency(body->second,"PAR ")&&
        venusBodyVersion(sha256(body->second,true));
}
std::string unverifiedDependency(const char* name){return std::string("This version of ")+name+" has not been verified. Functionality is not guaranteed. You can still use these controls.";}
bool readableDependency(const std::filesystem::path& path,const char* signature){if(path.empty())return false;auto name=std::filesystem::absolute(path).lexically_normal().wstring();if(name.rfind(L"\\\\?\\",0)!=0)name=name.rfind(L"\\\\",0)==0?L"\\\\?\\UNC\\"+name.substr(2):L"\\\\?\\"+name;std::ifstream in(std::filesystem::path(name),std::ios::binary);if(!in)return false;char header[4]{};in.read(header,sizeof header);return in.gcount()>0&&(!signature||(in.gcount()==4&&!std::memcmp(header,signature,4)));}
bool verifyMountedDependencies(const std::filesystem::path& game,std::string& error) {
    return verifyMountedDependencies(game,game/L"bin64/dmm/manifest.tsv",error);
}
bool verifyMountedDependencies(const std::filesystem::path& game,const std::filesystem::path& manifest,std::string& error) {
    std::string warning;return verifyMountedDependencies(game,manifest,error,warning);
}
bool verifyMountedDependencies(const std::filesystem::path& game,const std::filesystem::path& manifest,std::string& error,std::string& warning,uint32_t* unavailableWitches,std::string* note,
                               std::array<std::string,6>* witchWarnings,std::string* skipped) {
    error.clear();warning.clear();
    if(unavailableWitches)*unavailableWitches=0;
    if(note)note->clear();
    if(witchWarnings)for(auto& text:*witchWarnings)text.clear();
    if(skipped)skipped->clear();
    // Warnings accumulate. A witch's own appearance or body is reported for her alone when the
    // caller asks for per-witch warnings, so it no longer marks every character's controls.
    const auto append=[](std::string& out,const std::string& text){
        if(text.empty()||out.find(text)!=std::string::npos)return;
        if(!out.empty())out+="\n\n";
        out+=text;
    };
    const auto witchWarning=[&](int witch,const std::string& text){
        if(witchWarnings&&witch>=0&&witch<int(witchWarnings->size()))append((*witchWarnings)[witch],text);
        else append(warning,text);
    };
    bool version=false;
    const auto routes=readMountedRoutes(manifest,&version,&warning);
    if(!version){
        error=readableDependency(manifest)?"The selected active file list is not a supported DMM runtime manifest.":
            "Mount one Hex Appeal body option through Definitive Mod Manager first.";
        return false;
    }
    const auto mode=classifyBodyRoutes(routes,nullptr,unavailableWitches,nullptr,game);
    if(mode==BodyMode::Invalid){error="The mounted witch bodies are missing or mixed. Enable one Hex Appeal body option, remove any stale older entry and remount.";return false;}
    const char* bodyPaths[]={
        venusModelPath,
        "character/model/1_pc/2_phw/nude/cd_phw_00_nude_00_0001_damian.pac"
    };
    constexpr const char* vanillaHashes[]={
        "fcb7bab556c63957c7e77b4ecbe31c74daf7df3710dd0624fac2dc13dfa64a85",
        "9e1cce6355d4b0770b6f9ac7ce7bd7971cd441d72e9f213fd401afa79609a850",
        "32c4ea3aec9643fc23dc1f49a8e1e66408e0491e65174eed9e2a62e87fe39c0b",
        "d601ec63f2b16d5e9f8f2826f8d3cee96c48fc0483fb0af08a5424aa6ad79806",
        "01d6739d0be27364ca6a6754443d00a1caecf011577e2ade1e019478d8953418",
        "5597fb26d299f6d0f794250cd0e24984c1767f41aec357e63739b9d3a9941a23",
        "84f1d2933299532a9d57529fc39ffd75f4bc3e8df00ad0e21b887b66ca55b79e"
    };
    if(mode==BodyMode::Venus)for(const auto path:bodyPaths) {
        const auto found=routes.find(path);
        if(found==routes.end()&&path==bodyPaths[1])continue;
        if(found==routes.end()||isOwnModContent(found->second)||!readableDependency(found->second,"PAR ")) {
            error="The mounted Venus Body for Damiane path is missing, unreadable or not a PAC file. Check body priority and remount.";return false;
        }
        if(!venusBodyVersion(sha256(found->second,true)))append(warning,unverifiedDependency("Venus Body for Damiane"));
    }
    if(mode==BodyMode::Vanilla){
        const std::pair<const char*,const char*> bodies[]={
            {"hvw_00_nude_00_0001","d51fb429448e147d0eabd512f092a8e7b99524ba24ad183ed231fee1e14da7bb"},
            {"hvw_00_nude_00_0001_damian","bad6e5d6b9f5b35efb35ad70e647a738fc4fde80f5993fe2802282a9ca909f75"},
            {"hvw_00_nude_00_0002","58c491a59da39ece03a2a96f021757bdbe5508a730edaa4267b7c80f20d3f909"}};
        for(const auto& row:bodies){
            const auto path="character/model/ha_p/2_phw/nude/"+std::string(row.first)+".pac";const auto body=routes.find(path);
            if(body==routes.end()||!readableDependency(body->second,"PAR ")){error="A mounted original-witch body is missing or unreadable. Reinstall the Vanilla option and remount.";return false;}
            if(sha256(body->second,true)!=row.second)append(warning,unverifiedDependency("the original-witch body resource"));
            for(const auto& resource:{"character/modelproperty/ha_p/2_phw/nude/"+std::string(row.first)+".pac_xml","character/bin__/meshphysics/ha_p/2_phw/nude/"+std::string(row.first)+".hkx"}){
                const auto found=routes.find(resource);if(found==routes.end()||!readableDependency(found->second)){error="Original-witch body metadata or physics is missing. Reinstall the Vanilla option and remount.";return false;}
            }
        }
    }
    struct Appearance {const char* path;const char* hash;const char* isolatedHash=nullptr;const char* vanillaName="hvw_00_nude_00_0001";const char* npcUnderwearHash=nullptr;};
    const Appearance appearances[]={
        {"character/appearance/2_mon/cd_m0001_00_twofeet/cd_m0001_00_phw_north/cd_m0001_00_phw_north_44001.app_xml","5c0541a8ef1c686af58e7f6b7370bdeb59293bc71ef5a9bf1c8c4ab2f70d44c0"},
        {"character/appearance/3_npc/cd_npc_north/cd_nhw_north/cd_nhw_north_20026.app_xml","051af75fb41cf669ef2f31713df82a93be1eae9a1ff8c8eaba5f9db517f09143",nullptr,"hvw_00_nude_00_0301"},
        {"character/appearance/3_npc/cd_npc_south/cd_nhw_south/cd_nhw_south_20165.app_xml","94e56c70dc522126106d95765da1befbfb77ef38a889b2c857f2aafb1da18d70","680d69e548f58e18a83e6ec217230e5017738919d7bb85b8cb94b60e3095472e"},
        {"character/appearance/3_npc/cd_npc_south/cd_nhw_south/cd_nhw_south_20166.app_xml","e20de4404c04793b358127f159b84c3bf6b4cfbf55200cec59fe5fc40080f20b"},
        {"character/appearance/3_npc/cd_npc_south/cd_nhw_south/cd_nhw_south_20167.app_xml","da205bbe4a5ce12558de51a5feff531d8d1ca8db56d1b7a48ee3f6fa9037facc","3d0943c11ed5b831b873a785e0ba8da50773c9ce0fdee47f924fe5962cd04b0e"},
        {"character/appearance/3_npc/cd_npc_south/cd_nhw_south/cd_nhw_south_20168.app_xml","099cdc89ffa8e47e8883e7af8766a3e3b86fad73bd9b75663b0e221f44e431de","ff7160fc7a043f8d1ce9070f782b0a767f0802348380ca0e4358021953d1f5b6"},
        {"character/appearance/3_npc/cd_npc_south/cd_nhw_south/cd_nhw_south_20169.app_xml","da7a172a21e53c682fac70504a4edc508b569e5541e1f56e7b85a77f2cdacc9a",nullptr,"hvw_00_nude_00_0001_damian","883bf1462ca78a9efc155e5817d6e69c3b6152e5c48f3300c364a28ac038b295"}
    };
    bool meshRemoved=false;
    const bool npcUnderwear=npcUnderwearReady(routes,meshRemoved);
    const bool npcUnderwearAssetsVerified=npcUnderwear&&verifiedNpcUnderwearAssets(routes);
    const bool isolatedVenus=mode==BodyMode::Venus&&isolatedVenusReady(routes);
    static_assert(std::size(appearances)==std::size(bodyAppearances));
    static_assert(std::size(vanillaHashes)==std::size(appearances));
    uint32_t mountedMidpoint=0;
    for(size_t index=0;index<std::size(appearances);++index) {
        const auto& appearance=appearances[index];
        if(unavailableWitches&&(*unavailableWitches&(1u<<bodyAppearances[index].witch)))continue;
        const auto found=routes.find(appearance.path);
        if(found==routes.end()&&mode==BodyMode::Venus&&checkedStockDamianeAppearance(bodyAppearances[index],game))continue;
        if(found==routes.end()||!readableDependency(found->second)) {
            error="A mounted witch appearance path is missing or unreadable. Remount the body component.";return false;
        }
        const auto body=appearanceBody(found->second);
        const bool usesNpcUnderwear=npcUnderwear&&body==npcUnderwearPrefab;
        if(isolatedVenus&&body==isolatedVenusPrefab)continue;
        const auto* selectedBody=mode==BodyMode::Vanilla?appearance.vanillaName:venusPrefab;
        const int witch=bodyAppearances[index].witch;
        if(body!=selectedBody&&!usesNpcUnderwear){
            bool sharesVenusModel=false;
            if(!mountedPhwBodyClone(routes,body,nullptr,&sharesVenusModel)){
                error="A witch appearance does not use a compatible body. Check the selected body option and remount.";return false;
            }
            if(mode==BodyMode::Vanilla)mountedMidpoint|=1u<<witch;
            // A VENUS clone that draws the checked Venus model with the same rig is that body;
            // only its underwear part differs, which the underwear provider already reports.
            if(!(mode==BodyMode::Venus&&sharesVenusModel))witchWarning(witch,unverifiedDependency("the mounted witch body replacement"));
            continue;
        }
        const auto digest=sha256(found->second,true);
        if(usesNpcUnderwear){
            if(mode==BodyMode::Vanilla)mountedMidpoint|=1u<<witch;
            if(!npcUnderwearAssetsVerified||!appearance.npcUnderwearHash||digest!=appearance.npcUnderwearHash)
                witchWarning(witch,unverifiedDependency("No Underwear – NPCs"));
        }else if(mode==BodyMode::Vanilla?digest!=vanillaHashes[index]:
                  digest!=appearance.hash&&(!appearance.isolatedHash||digest!=appearance.isolatedHash)){
            witchWarning(witch,unverifiedDependency("the mounted witch appearance replacement"));
        }
    }
    if(isolatedVenus){
        const auto alternate=routes.find(isolatedVenusModelPath);
        const auto current=routes.find(venusModelPath);
        if(alternate!=routes.end()&&current!=routes.end()&&sha256(alternate->second,true)!=sha256(current->second,true))
            append(warning,"The local witch skin package uses a different Venus Body for Damiane model. Rebuild the local package after updating Venus Body for Damiane.");
    }
    if(mountedMidpoint&&note){
        *note="Shape uses the mounted replacement body as the midpoint for ";
        const char* separator="";
        for(size_t i=0;i<witches.size();++i)if(mountedMidpoint&(1u<<i)){
            *note+=separator;*note+=witches[i].name;separator=", ";
        }
        *note+=".";
    }
    if(unavailableWitches&&*unavailableWitches){
        std::string text="Shape skipped for ";
        const char* separator="";
        for(size_t i=0;i<witches.size();++i)if(*unavailableWitches&(1u<<i)){
            text+=separator;text+=witches[i].name;separator=", ";
        }
        text+=". Check their mounted body in DMM.";
        // Each skipped witch already shows her own reason; the summary is for ALL CHARACTERS and reports.
        if(skipped)*skipped=std::move(text);
        else append(warning,text);
    }
    return true;
}
}
