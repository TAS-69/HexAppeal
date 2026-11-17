#include "mounted_dependencies.h"
#include "runtime.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <sstream>
namespace fs=std::filesystem;
static void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static std::string quoted(const fs::path& p){std::string out="\"";for(char c:p.u8string()){if(c=='\\'||c=='\"')out+='\\';out+=c;}return out+'\"';}
static void write(const fs::path& p,const std::string& text){fs::create_directories(p.parent_path());std::ofstream f(p,std::ios::binary);f<<text;require(bool(f),"write test fixture");}
int wmain(int argc,wchar_t** argv){try{
    require(argc==3,"game and external fixture path required");const fs::path game=argv[1],temp=argv[2];
    const auto actual=hex::inspectDependencies(game,{});
    require(actual.bodyVerified&&actual.hex.valid&&actual.dancer.valid,"installed external fixtures must be valid");
    const auto storage=temp/L"custom staging \u00e9\u6f22",preset=storage/L"renamed resource"/actual.hex.path.filename();
    fs::create_directories(preset.parent_path());fs::copy_file(actual.hex.path,preset,fs::copy_options::overwrite_existing);
    const auto config=temp/L"portable manager"/L"config.json";
    write(config,"{\"other\":{\"modsPath\":\"ignore\"},\"gamePath\":"+quoted(game)+",\"modsPath\":"+quoted(storage)+"}");
    hex::DependencyPaths paths;paths.managerConfig=config;paths.manifest=actual.manifest.path;paths.dancerSource=actual.dancer.path;
    auto found=hex::inspectDependencies(game,paths);
    require(found.manager.valid&&found.hex.valid&&found.hex.path==preset&&found.bodyVerified,"portable config, renamed mods and Unicode storage detection");
    paths.hexPreset=temp/L"not installed.ini";found=hex::inspectDependencies(game,paths);require(!found.hex.valid&&found.bodyVerified,"bad explicit preset cannot fall back silently");
    paths.hexPreset=actual.hex.path;paths.venusBody=actual.venus.path;paths.manifest=temp/L"no mounted file list.tsv";
    found=hex::inspectDependencies(game,paths);require(found.venus.valid&&!found.bodyVerified,"installed source alone cannot prove active body");
    paths.manifest=actual.manifest.path;paths.hexPreset=temp/L"wrong preset.ini";write(paths.hexPreset,"unsupported preset");
    require(!hex::inspectDependencies(game,paths).hex.valid,"malformed preset file rejected");
    paths.hexPreset=temp/L"unverified preset.ini";{std::ifstream source(actual.hex.path,std::ios::binary);std::ostringstream contents;contents<<source.rdbuf();auto text=contents.str();const auto start=text.find("InstantSurgeryOutfit="),end=text.find('\n',start);require(start!=std::string::npos,"source preset marker");text.replace(start,end-start,"InstantSurgeryOutfit=1:synthetic-unverified-preset");write(paths.hexPreset,text);}
    found=hex::inspectDependencies(game,paths);require(found.hex.valid&&!found.hex.verified&&found.hex.detail.find("not been verified")!=std::string::npos,"readable unverified preset warns without blocking");
    paths.venusBody=temp/L"unverified body.pac";fs::copy_file(actual.venus.path,paths.venusBody,fs::copy_options::overwrite_existing);{std::ofstream out(paths.venusBody,std::ios::binary|std::ios::app);out.put(0);}
    found=hex::inspectDependencies(game,paths);require(found.venus.valid&&!found.venus.verified&&found.bodyVerified,"readable unverified body source remains usable");paths.venusBody=actual.venus.path;
    paths.hexPreset=temp/L"Hex Appeal"/L"preset-f-HentaiBodyV1_2Venus.ini";fs::create_directories(paths.hexPreset.parent_path());fs::copy_file(actual.hex.path,paths.hexPreset,fs::copy_options::overwrite_existing);
    require(!hex::inspectDependencies(game,paths).hex.valid,"bundled dependency inside own mod rejected");
    paths.hexPreset=actual.hex.path;paths.dancerSource=temp/L"missing dancer.pac";found=hex::inspectDependencies(game,paths);
    require(!found.dancer.valid&&found.hex.valid&&found.bodyVerified,"missing optional outfit leaves base and Hex valid");
    hex::Profile weighted;weighted.weight=1;require(hex::profileDependencyIssue(weighted,found.bodyVerified,true,true).empty(),"missing outfit source does not block saved Weight");
    std::string error;const auto settings=temp/L"settings"/L"paths.ini";
    paths.modsDirectory=storage;require(hex::saveDependencyPaths(settings,paths,error),"save manual paths");hex::DependencyPaths loaded;
    require(hex::loadDependencyPaths(settings,loaded,error)&&loaded.modsDirectory==storage&&loaded.hexPreset==actual.hex.path&&loaded.dancerSource==paths.dancerSource,"Unicode manual paths round trip");
    write(settings,"WeightReference="+actual.dancer.path.u8string()+"\n");require(hex::loadDependencyPaths(settings,loaded,error)&&loaded.dancerSource==actual.dancer.path,"legacy WeightReference preserved for Dancer only");
    for(const auto& text:{"DancerSource="+paths.dancerSource.u8string()+"\nWeightReference="+actual.dancer.path.u8string()+"\n","WeightReference="+actual.dancer.path.u8string()+"\nDancerSource="+paths.dancerSource.u8string()+"\n"}){write(settings,text);require(hex::loadDependencyPaths(settings,loaded,error)&&loaded.dancerSource==paths.dancerSource,"explicit DancerSource wins over the legacy key");}
    loaded.hexPreset=actual.hex.path;
    const auto saved=loaded.hexPreset;write(settings,"HexPreset=invalid\tpath\n");require(!hex::loadDependencyPaths(settings,loaded,error)&&loaded.hexPreset==saved,"invalid settings do not partly replace valid paths");
    paths.hexPreset=fs::path(L"bad\npath");require(!hex::saveDependencyPaths(settings,paths,error),"line-break injection rejected");
    hex::Profile profile;require(hex::profileDependencyIssue(profile,true,false,false).empty(),"VENUS requires neither optional Hex nor weight");
    profile.bottom=hex::Preset::Flat;require(hex::profileDependencyIssue(profile,true,false,false).empty(),"FLAT requires neither optional Hex nor weight");
    profile.top=hex::Preset::Hex;require(!hex::profileDependencyIssue(profile,true,false,true).empty(),"saved Hex must wait for preset");
    profile.topBlend=0;require(hex::profileDependencyIssue(profile,true,false,true).empty(),"zero Hex blend is baseline");
    profile.weight=1;require(!hex::profileDependencyIssue(profile,true,true,false).empty(),"unavailable native Weight cannot silently flatten");
    require(!hex::profileDependencyIssue(profile,false,true,true).empty(),"missing base blocks every shape");
    std::cout<<"PASS: external portable/Unicode storage, manual paths, active-route verification, unsupported/bundled rejection and feature-specific dependencies.\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
