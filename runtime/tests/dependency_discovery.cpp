#include "mounted_dependencies.h"
#include "runtime.h"
#include "dependency_fixture.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <Windows.h>
namespace fs=std::filesystem;
static void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static std::string quoted(const fs::path& p){std::string out="\"";for(char c:p.u8string()){if(c=='\\'||c=='\"')out+='\\';out+=c;}return out+'\"';}
static void write(const fs::path& p,const std::string& text){fs::create_directories(p.parent_path());std::ofstream f(p,std::ios::binary);f<<text;require(bool(f),"write test fixture");}
int wmain(int argc,wchar_t** argv){try{
    require(argc==3,"game and external fixture path required");const fs::path game=argv[1],temp=argv[2];
    auto actual=hex::inspectDependencies(game,{});
    require(actual.bodyVerified&&actual.hex.valid,"installed external fixtures must be valid");
    hex::DependencyPaths referencePaths;referencePaths.manifest=hex::test::venusManifestFixture(actual.manifest.path,temp/L"venus-reference");actual=hex::inspectDependencies(game,referencePaths);
    const auto storage=temp/L"custom staging \u00e9\u6f22",preset=storage/L"renamed resource"/actual.hex.path.filename();
    fs::create_directories(preset.parent_path());fs::copy_file(actual.hex.path,preset,fs::copy_options::overwrite_existing);
    const auto config=temp/L"portable manager"/L"config.json";
    write(config,"{\"other\":{\"modsPath\":\"ignore\"},\"gamePath\":"+quoted(game)+",\"modsPath\":"+quoted(storage)+"}");
    hex::DependencyPaths paths;paths.managerConfig=config;paths.manifest=actual.manifest.path;paths.legacyOutfitSource=temp/L"old-outfit-reference.pac";
    auto found=hex::inspectDependencies(game,paths);
    if(!found.manager.valid||!found.hex.valid||!fs::equivalent(found.hex.path,preset)||!found.bodyVerified){std::cerr<<"Expected preset: "<<preset.u8string()<<"\nDetected preset: "<<found.hex.path.u8string()<<"\nSource preset: "<<actual.hex.path.u8string()<<"\nManager: "<<found.manager.detail<<"\nBody: "<<found.bodyDetail<<'\n';throw std::runtime_error("portable config, renamed mods and Unicode storage detection");}
    paths.hexPreset=fs::path(L"  \""+preset.wstring()+L"\"  ");found=hex::inspectDependencies(game,paths);
    require(found.hex.valid&&fs::equivalent(found.hex.path,preset),"quoted Copy as path input is accepted");
    SetEnvironmentVariableW(L"HEX_APPEAL_TEST_PRESET",preset.c_str());paths.hexPreset=L"%HEX_APPEAL_TEST_PRESET%";found=hex::inspectDependencies(game,paths);SetEnvironmentVariableW(L"HEX_APPEAL_TEST_PRESET",nullptr);
    require(found.hex.valid&&fs::equivalent(found.hex.path,preset),"environment variable preset path is accepted");
    const auto deep=temp/L"nested preset"/L"one/two/three/four/five/six/RENAMED.INI";
    fs::create_directories(deep.parent_path());fs::copy_file(actual.hex.path,deep,fs::copy_options::overwrite_existing);
    paths.hexPreset=temp/L"nested preset";found=hex::inspectDependencies(game,paths);
    require(found.hex.valid&&found.hex.verified&&fs::equivalent(found.hex.path,deep),"folder discovery handles deep nesting, renamed checked files and uppercase extensions");
    paths.hexPreset=temp/L"download.zip";write(paths.hexPreset,"archive fixture");found=hex::inspectDependencies(game,paths);
    require(!found.hex.valid&&found.hex.detail.find("Extract")!=std::string::npos,"archive input explains extraction instead of reporting a missing preset");
    const auto isolated=temp/L"isolated game",library=temp/L"portable library",libraryPreset=library/L"Hex download"/actual.hex.path.filename();
    fs::create_directories(libraryPreset.parent_path());fs::copy_file(actual.hex.path,libraryPreset,fs::copy_options::overwrite_existing);
    write(library/L"Mounted resource/mod.json","{\"modinfo\":{\"title\":\"Fixture\"}}");write(library/L"Mounted resource/files/0009/fixture.pac","fixture");
    const auto isolatedBody=temp/L"external body/reference.pac";fs::create_directories(isolatedBody.parent_path());fs::copy_file(actual.venus.path,isolatedBody,fs::copy_options::overwrite_existing);
    const auto isolatedManifest=isolated/L"bin64/dmm/manifest.tsv";
    std::ifstream sourceManifest(actual.manifest.path);std::string manifestLine,manifestText="dmm-runtime\t1\n";
    while(std::getline(sourceManifest,manifestLine)){if(!manifestLine.empty()&&manifestLine.back()=='\r')manifestLine.pop_back();
        if(manifestLine.rfind("file\tcharacter/appearance/",0)==0)manifestText+=manifestLine+'\n';}
    manifestText+="file\tcharacter/model/1_pc/2_phw/nude/phw_00_nude_00_0001_damian.pac\t"+isolatedBody.u8string()+"\nfile\tfixture.pac\t"+(library/L"Mounted resource/files/0009/fixture.pac").u8string()+"\n";
    write(isolatedManifest,manifestText);hex::DependencyPaths isolatedPaths;isolatedPaths.manifest=isolatedManifest;
    auto inferred=hex::inspectDependencies(isolated,isolatedPaths);
    require(inferred.manager.valid&&inferred.hex.valid&&fs::equivalent(inferred.hex.path,libraryPreset),"DMM active-file paths locate custom storage with the manager closed and no configured mods path");
    const auto libraryConfig=library.parent_path()/L"config.json";write(libraryConfig,"{\"gamePath\":"+quoted(isolated)+",\"modsPath\":"+quoted(library)+"}");
    inferred=hex::inspectDependencies(isolated,isolatedPaths);
    require(inferred.manager.valid&&fs::equivalent(inferred.manager.path,libraryConfig)&&inferred.hex.valid,"active file roots locate and read the manager configuration");
    const auto nativePreset=library/L"Original body preset/renamed-export.INI";
    {std::ifstream source(actual.hex.path,std::ios::binary);std::ostringstream contents;contents<<source.rdbuf();auto text=contents.str();const auto start=text.find("InstantSurgeryOutfit="),end=text.find('\n',start);text.replace(start,end-start,"InstantSurgeryOutfit=1:preset-f-HentaiBodyVanilla");write(nativePreset,text);}
    {std::istringstream rows(manifestText);std::ostringstream changed;std::string line;unsigned count=0;while(std::getline(rows,line)){
        const auto tab=line.find('\t',5);if(line.rfind("file\tcharacter/appearance/",0)==0&&tab!=std::string::npos){const auto key=line.substr(5,tab-5);if(hex::test::witchAppearance(key)){const auto app=temp/L"vanilla-looks"/(std::to_wstring(count++)+L".app_xml");write(app,"<Appearance><Nude><Prefab Name=\""+hex::test::vanillaBodyFor(key)+"\"/></Nude></Appearance>");line.replace(tab+1,std::string::npos,app.u8string());}}
        changed<<line<<'\n';}write(isolatedManifest,changed.str());}
    inferred=hex::inspectDependencies(isolated,isolatedPaths);
    require(inferred.bodyMode==hex::BodyMode::Vanilla&&inferred.hex.valid&&fs::equivalent(inferred.hex.path,nativePreset),"Vanilla prefers its matching external Hex export, even after renaming");
    fs::remove(nativePreset);inferred=hex::inspectDependencies(isolated,isolatedPaths);
    require(inferred.hex.valid&&fs::equivalent(inferred.hex.path,libraryPreset)&&inferred.hex.detail.find("relative")!=std::string::npos,"existing Venus Hex preset remains usable as relative shaping for Vanilla");
    write(isolatedManifest,manifestText);
    fs::remove(libraryPreset);fs::remove(libraryConfig);
    struct LocalEnvironment {std::wstring value;bool present=false;LocalEnvironment(){wchar_t text[32768]{};const auto size=GetEnvironmentVariableW(L"LOCALAPPDATA",text,32768);present=size&&size<32768;if(present)value=text;}~LocalEnvironment(){SetEnvironmentVariableW(L"LOCALAPPDATA",present?value.c_str():nullptr);}} localEnvironment;
    const auto localRoot=temp/L"fake local app data",savedPreset=localRoot/L"InstantSurgery/Outfits"/actual.hex.path.filename();
    fs::create_directories(savedPreset.parent_path());fs::copy_file(actual.hex.path,savedPreset,fs::copy_options::overwrite_existing);SetEnvironmentVariableW(L"LOCALAPPDATA",localRoot.c_str());
    inferred=hex::inspectDependencies(isolated,isolatedPaths);
    require(inferred.hex.valid&&fs::equivalent(inferred.hex.path,savedPreset),"Instant Surgery's external local preset library is detected automatically");
    SetEnvironmentVariableW(L"LOCALAPPDATA",localEnvironment.present?localEnvironment.value.c_str():nullptr);
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
    paths.hexPreset=actual.hex.path;paths.legacyOutfitSource=temp/L"missing dancer.pac";found=hex::inspectDependencies(game,paths);
    require(found.hex.valid&&found.bodyVerified,"missing optional outfit leaves base and Hex valid");
    hex::Profile weighted;weighted.weight=1;require(hex::profileDependencyIssue(weighted,found.bodyVerified,true,true).empty(),"missing outfit source does not block saved Weight");
    std::string error;const auto settings=temp/L"settings"/L"paths.ini";
    paths.modsDirectory=storage;require(hex::saveDependencyPaths(settings,paths,error),"save manual paths");hex::DependencyPaths loaded;
    require(hex::loadDependencyPaths(settings,loaded,error)&&loaded.modsDirectory==storage&&loaded.hexPreset==actual.hex.path&&loaded.legacyOutfitSource==paths.legacyOutfitSource,"Unicode manual paths round trip");
    write(settings,"WeightReference="+(temp/L"legacy-reference.pac").u8string()+"\n");require(hex::loadDependencyPaths(settings,loaded,error)&&loaded.legacyOutfitSource==(temp/L"legacy-reference.pac"),"legacy WeightReference preserved for Dancer only");
    for(const auto& text:{"DancerSource="+paths.legacyOutfitSource.u8string()+"\nWeightReference="+(temp/L"legacy-reference.pac").u8string()+"\n","WeightReference="+(temp/L"legacy-reference.pac").u8string()+"\nDancerSource="+paths.legacyOutfitSource.u8string()+"\n"}){write(settings,text);require(hex::loadDependencyPaths(settings,loaded,error)&&loaded.legacyOutfitSource==paths.legacyOutfitSource,"explicit DancerSource wins over the legacy key");}
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
