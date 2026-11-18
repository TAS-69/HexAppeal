#include "mounted_dependencies.h"
#include "runtime.h"
#include "venus_bodies.h"
#include "dependency_fixture.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

int wmain(int argc,wchar_t** argv) {
    if(argc!=3&&argc!=4)return 1;
    const std::filesystem::path game=argv[1],temporary=argv[2];std::string error;
    if(!hex::verifyMountedDependencies(game,error)){std::cerr<<error<<'\n';return 1;}
    const auto reference=hex::test::venusManifestFixture(game/L"bin64/dmm/manifest.tsv",temporary/L"venus-reference");
    std::ifstream input(reference);std::ostringstream original;original<<input.rdbuf();
    if(argc==4){
        const auto human=std::filesystem::absolute(argv[3]);const auto* version=hex::venusBodyVersion(hex::sha256(human));
        if(!version||std::string(version->variant)!="Human Body"){std::cerr<<"Supply the external checked VENUS 2.6 Human Body.\n";return 1;}
        std::istringstream rows(original.str());std::ostringstream changed;std::string line;unsigned aliases=0;
        while(std::getline(rows,line)){
            if(!line.empty()&&line.back()=='\r')line.pop_back();
            const auto tab=line.find('\t',5);
            if(line.rfind("file\t",0)==0&&tab!=std::string::npos){
                const auto key=line.substr(5,tab-5);
                if(key=="character/model/1_pc/2_phw/nude/phw_00_nude_00_0001_damian.pac"||key=="character/model/1_pc/2_phw/nude/cd_phw_00_nude_00_0001_damian.pac"){line.replace(tab+1,std::string::npos,human.u8string());++aliases;}
            }
            changed<<line<<'\n';
        }
        if(aliases<1){std::cerr<<"The primary active body route is required.\n";return 1;}
        const auto humanManifest=temporary/L"human-body/bin64/dmm/manifest.tsv";std::filesystem::create_directories(humanManifest.parent_path());
        {std::ofstream out(humanManifest,std::ios::binary);out<<changed.str();}
        std::string warning;
        if(!hex::verifyMountedDependencies(game,humanManifest,error,warning)||!warning.empty()){std::cerr<<"Human Body routes: "<<error<<warning<<'\n';return 1;}
        hex::DependencyPaths paths;paths.manifest=humanManifest;
        const auto humanStatus=hex::inspectDependencies(game,paths);
        if(!humanStatus.bodyVerified||!humanStatus.venus.verified||humanStatus.venus.path!=human||humanStatus.venus.detail.find("Human Body")==std::string::npos){std::cerr<<"Human Body detection was not verified.\n";return 1;}
        paths.venusBody=human;const auto manual=hex::inspectDependencies(game,paths);
        if(!manual.bodyVerified||!manual.venus.verified){std::cerr<<"Explicit Human Body path was rejected.\n";return 1;}
        std::istringstream humanRows(changed.str());std::ostringstream primary;
        const std::string alias="file\tcharacter/model/1_pc/2_phw/nude/cd_phw_00_nude_00_0001_damian.pac\t";
        while(std::getline(humanRows,line))if(line.rfind(alias,0)!=0)primary<<line<<'\n';
        const auto primaryManifest=temporary/L"human-primary-only.tsv";
        {std::ofstream out(primaryManifest,std::ios::binary);out<<primary.str();}
        if(!hex::verifyMountedDependencies(game,primaryManifest,error,warning)||!warning.empty()){std::cerr<<"Valid primary-only Human Body installation was blocked.\n";return 1;}
        const auto aliasManifest=temporary/L"human-with-alias.tsv";
        {std::ofstream out(aliasManifest,std::ios::binary);out<<primary.str()<<alias<<human.u8string()<<'\n';}
        if(!hex::verifyMountedDependencies(game,aliasManifest,error,warning)||!warning.empty()){std::cerr<<"Valid Human Body with optional alias was blocked.\n";return 1;}
        std::cout<<"PASS: original external Human Body, primary-only and aliased installations, checked automatic/manual detection and unchanged witch appearance routes.\n";
    }
    const auto manifest=temporary/L"bin64/dmm/manifest.tsv";
    std::filesystem::create_directories(manifest.parent_path());
    {
        const auto folder=temporary/L"vanilla";std::filesystem::create_directories(folder);
        const auto selected=folder/L"manifest.tsv";
        const auto save=[&](const std::filesystem::path& path,const std::string& text){std::ofstream file(path,std::ios::binary);file<<text;};
        std::istringstream rows(original.str());std::ostringstream changed;std::string line;unsigned looks=0;
        while(std::getline(rows,line)){
            if(!line.empty()&&line.back()=='\r')line.pop_back();
            const auto tab=line.find('\t',5);
            if(line.rfind("file\t",0)==0&&tab!=std::string::npos){
                const auto key=line.substr(5,tab-5);
                if(key.find("character/model/1_pc/2_phw/nude/")==0||key=="character/descriptors/hexappeal/body-mode.txt")continue;
                if(key.find("character/appearance/")==0&&key.find("damian")==std::string::npos){
                    if(key.find("44001.app_xml")==std::string::npos&&key.find("20026.app_xml")==std::string::npos&&key.find("2016")==std::string::npos){changed<<line<<'\n';continue;}
                    const auto app=folder/(std::to_wstring(looks++)+L".app_xml");
                    const std::string body=key.find("20026.app_xml")!=std::string::npos?"hvw_00_nude_00_0301":key.find("20169.app_xml")!=std::string::npos?"hvw_00_nude_00_0001_damian":"hvw_00_nude_00_0001";
                    save(app,"<Appearance>\n<Nude>\n<Prefab CharacterScale='0.97' Name = '"+body+"'/>\n</Nude>\n</Appearance>");
                    line.replace(tab+1,std::string::npos,app.u8string());
                }
            }
            changed<<line<<'\n';
        }
        if(looks!=7){std::cerr<<"Expected seven mounted witch appearances in vanilla fixture.\n";return 1;}
        for(const std::string stem:{"hvw_00_nude_00_0001","hvw_00_nude_00_0001_damian","hvw_00_nude_00_0002"}){
            const auto model=folder/std::filesystem::u8path(stem+".pac");save(model,"PAR test fixture, never a model output");
            changed<<"file\tcharacter/model/ha_p/2_phw/nude/"<<stem<<".pac\t"<<model.u8string()<<'\n';
            const auto metadata=folder/std::filesystem::u8path(stem+".fixture");save(metadata,"test metadata fixture");
            changed<<"file\tcharacter/modelproperty/ha_p/2_phw/nude/"<<stem<<".pac_xml\t"<<metadata.u8string()<<'\n';
            changed<<"file\tcharacter/bin__/meshphysics/ha_p/2_phw/nude/"<<stem<<".hkx\t"<<metadata.u8string()<<'\n';
        }
        save(selected,changed.str());
        std::string warning;
        if(!hex::verifyMountedDependencies(game,selected,error,warning)){std::cerr<<"Explicit vanilla option needs no VENUS body: "<<error<<'\n';return 1;}
        hex::DependencyPaths paths;paths.manifest=selected;const auto status=hex::inspectDependencies(game,paths);
        if(!status.bodyVerified||!status.bodySourceReady()||status.venus.valid||status.bodyMode!=hex::BodyMode::Vanilla){std::cerr<<"Vanilla body discovery/gating failed.\n";return 1;}
        save(folder/L"0.app_xml","<Appearance><Nude><Prefab Name=\"phw_00_nude_00_0001\"/></Nude></Appearance>");
        if(hex::verifyMountedDependencies(game,selected,error,warning)){std::cerr<<"Missing VENUS silently selected a stock body outside the explicit Vanilla option.\n";return 1;}
        save(folder/L"0.app_xml","<Appearance><Nude><Prefab Name=\"phw_00_nude_00_0001_damian\"/></Nude></Appearance>");
        if(hex::verifyMountedDependencies(game,selected,error,warning)){std::cerr<<"Wrong mounted body was accepted for vanilla.\n";return 1;}
        save(folder/L"0.app_xml","<Appearance><Nude><Prefab Name=\"hvw_00_nude_00_0001\"/></Nude></Appearance>");
        const auto overrideBody=folder/L"hvw_00_nude_00_0001.pac";save(overrideBody,"PAR unverified test fixture, never a model output");
        if(!hex::verifyMountedDependencies(game,selected,error,warning)||warning.empty()){std::cerr<<"Unverified readable vanilla override should remain usable with warning.\n";return 1;}
        save(overrideBody,"unreadable model fixture");
        if(hex::verifyMountedDependencies(game,selected,error,warning)){std::cerr<<"Invalid vanilla override accepted.\n";return 1;}
        std::cout<<"PASS: explicit original-witch resources, no VENUS dependency, mixed-body rejection and soft unverified override.\n";
    }
    std::ofstream badBody(temporary/L"overriding-body.pac",std::ios::binary);badBody<<"different installed body";badBody.close();
    auto altered=original.str();const std::string route="file\tcharacter/model/1_pc/2_phw/nude/phw_00_nude_00_0001_damian.pac\t";
    const auto start=altered.find(route);if(start==std::string::npos)return 1;
    const auto path=start+route.size(),end=altered.find('\n',path);
    altered.replace(path,end-path,(temporary/L"overriding-body.pac").u8string());
    std::ofstream out(manifest,std::ios::binary);out<<altered;out.close();
    if(hex::verifyMountedDependencies(temporary,error)){std::cerr<<"Conflicting body was accepted.\n";return 1;}
    std::cout<<"PASS: actual mounted VENUS and seven witch routes verified; conflicting renamed body rejected despite the correct installed source.\n";
    return 0;
}
