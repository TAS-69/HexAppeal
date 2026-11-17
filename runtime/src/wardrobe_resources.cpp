#include "wardrobe_resources.h"
#include "runtime.h"
#include "dancer_palette.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
namespace hex {
DancerResourceStatus inspectDancerResources(const DependencyStatus& dependencies,bool runtimeLoaded){
    DancerResourceStatus result;
    struct Resource {const char* path;const char* mounted;const char* upstream;};
    const Resource files[]={
        {"character/model/1_pc/2_phw/armor/9_upperbody/cd_phw_00_ub_00_0191.pac","04bfe6c78263c56a390b9e98f70af9e7320826ce59276a24f38fad7e60c0971c","a81ba8af6d42774c2d39bf0efd12845920633268be9c246065bb91043f2139ee"},
        {"character/model/1_pc/2_phw/armor/10_lowerbody/cd_phw_00_lb_00_0191.pac","191e2cfc981173ad03b3e6e78b9c2e68e7fc5a37638cdf00ec93ed69008f8952","11cac17e563dc5e6a9efbfb26072fedb238e8b2ebffd88395d5f13ec6f7eb2e5"},
        {"character/modelproperty/1_pc/2_phw/armor/9_upperbody/cd_phw_00_ub_00_0191.pac_xml","9914560195a89e5cb885691f1284a69449d2d51e7c4954dce8d55627874124d6","9914560195a89e5cb885691f1284a69449d2d51e7c4954dce8d55627874124d6"},
        {"character/texture/cd_phw_00_ub_00_0135_00_02_01_o.dds","1547547f672fc339019269fe52714273439f7ae7a63f90a14a2194be83ff1056","1547547f672fc339019269fe52714273439f7ae7a63f90a14a2194be83ff1056"}};
    std::map<std::string,std::filesystem::path> routes;std::ifstream input(dependencies.manifest.path);std::string line;
    while(std::getline(input,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.rfind("file\t",0))continue;const auto tab=line.find('\t',5);if(tab==std::string::npos)continue;auto key=line.substr(5,tab-5);std::replace(key.begin(),key.end(),'\\','/');std::transform(key.begin(),key.end(),key.begin(),[](unsigned char c){return char(std::tolower(c));});auto source=std::filesystem::u8path(line.substr(tab+1));routes[key]=source.is_absolute()?source:dependencies.manifest.path.parent_path()/source;}
    bool mounted=dependencies.bodyVerified&&runtimeLoaded;
    for(const auto& resource:files){const auto row=routes.find(resource.path);if(row==routes.end()){result.detail=std::string("Missing mounted route: ")+resource.path;mounted=false;break;}const auto digest=sha256(row->second);const bool texture=resource.path==std::string(dancerTexture);if(texture){result.texture=row->second;result.colour=dancerColourForHash(digest);}const char* signature=texture?"DDS ":std::filesystem::path(resource.path).extension()==L".pac"?"PAR ":nullptr;if(!readableDependency(row->second,signature)){result.detail=std::string("Mounted resource is unreadable or has the wrong file format: ")+resource.path;mounted=false;break;}if(texture?result.colour<0:(digest!=resource.mounted&&digest!=resource.upstream))result.verified=false;}
    bool external=false;std::string externalFailure;
    if(dependencies.dancer.valid){auto parent=dependencies.dancer.path.parent_path();for(int depth=0;depth<10&&!parent.empty();++depth){
        bool matches=true,verified=true;unsigned matched=0;for(const auto& resource:files){const auto path=parent/resource.path;const auto digest=sha256(path);const char* signature=path.extension()==L".pac"?"PAR ":path.extension()==L".dds"?"DDS ":nullptr;if(!readableDependency(path,signature)){matches=false;if(matched)externalFailure=resource.path;break;}verified=verified&&digest==resource.upstream;++matched;}
        if(matches){external=true;result.verified=result.verified&&verified;result.sourceRoot=parent.parent_path();break;}const auto previous=parent;parent=parent.parent_path();if(previous==parent)break;
    }}
    result.mounted=mounted;result.external=external;result.ready=mounted&&external;
    if(result.ready){for(size_t i=0;i<dancerColours.size();++i){const auto digest=sha256(result.sourceRoot/dancerColours[i].folder/dancerTexture);if(digest!=dancerColours[i].sha256)result.verified=false;if(result.colour<0&&digest==sha256(result.texture))result.colour=int(i);}if(!result.verified)result.warning=unverifiedDependency("Sexy Witches of Pywel Reshaped");}
    std::filesystem::path bindingRoot;
    result.paletteReady=result.ready&&managedDancerBinding(result.texture,bindingRoot)&&verifyDancerPalette(result.sourceRoot,result.paletteDetail);
    if(result.paletteReady)for(const auto& resource:files){const auto route=routes.find(resource.path);if(route==routes.end()||_wcsicmp(route->second.lexically_normal().c_str(),(bindingRoot/L"files/0009"/resource.path).lexically_normal().c_str())){result.paletteReady=false;break;}}
    if(!result.paletteReady&&result.paletteDetail.empty())result.paletteDetail="Prepare the managed external Dancer colour bindings first.";
    if(!runtimeLoaded)result.detail="DMM runtime is not loaded.";else if(!dependencies.bodyVerified)result.detail="The active VENUS body route is unavailable.";else if(!external)result.detail="External Sexy Witches source path is missing or unreadable: "+(externalFailure.empty()?dependencies.dancer.path.u8string():externalFailure);else if(result.ready)result.detail=result.verified?"Dancer resources verified":result.warning;
    return result;
}
}
