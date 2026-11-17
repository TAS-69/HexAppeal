#include "mounted_dependencies.h"
#include "runtime.h"
#include "venus_bodies.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

int wmain(int argc,wchar_t** argv) {
    if(argc!=3&&argc!=4)return 1;
    const std::filesystem::path game=argv[1],temporary=argv[2];std::string error;
    if(!hex::verifyMountedDependencies(game,error)){std::cerr<<error<<'\n';return 1;}
    std::ifstream input(game/L"bin64/dmm/manifest.tsv");std::ostringstream original;original<<input.rdbuf();
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
    std::ofstream badBody(temporary/L"overriding-body.pac",std::ios::binary);badBody<<"different installed body";badBody.close();
    auto altered=original.str();const std::string route="file\tcharacter/model/1_pc/2_phw/nude/phw_00_nude_00_0001_damian.pac\t";
    const auto start=altered.find(route);if(start==std::string::npos)return 1;
    const auto path=start+route.size(),end=altered.find('\n',path);
    altered.replace(path,end-path,(temporary/L"overriding-body.pac").u8string());
    std::ofstream out(manifest,std::ios::binary);out<<altered;out.close();
    if(hex::verifyMountedDependencies(temporary,error)){std::cerr<<"Conflicting body was accepted.\n";return 1;}
    if(!hex::findDancerReference(temporary).empty()){std::cerr<<"Missing optional reference was accepted.\n";return 1;}
    const auto optional=hex::findDancerReference(game);
    if(!optional.empty()){
        const auto copy=temporary/L"mods/Sexy_Witches_fixture/files/0009/character/model/1_pc/2_phw/nude/cd_phw_00_nude_00_0001_damian.pac";
        std::filesystem::create_directories(copy.parent_path());std::filesystem::copy_file(optional,copy,std::filesystem::copy_options::overwrite_existing);
        if(hex::findDancerReference(temporary).empty()){std::cerr<<"Checked optional reference was not detected.\n";return 1;}
        std::ofstream altered(copy,std::ios::binary|std::ios::trunc);altered<<"different body";altered.close();
        if(!hex::findDancerReference(temporary).empty()){std::cerr<<"Unrecognized optional body was accepted.\n";return 1;}
    }
    std::cout<<"PASS: actual mounted VENUS and seven witch routes verified; conflicting renamed body rejected despite the correct installed source.\n";
    return 0;
}
