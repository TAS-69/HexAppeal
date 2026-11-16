#include "mounted_dependencies.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

int wmain(int argc,wchar_t** argv) {
    if(argc!=3)return 1;
    const std::filesystem::path game=argv[1],temporary=argv[2];std::string error;
    if(!hex::verifyMountedDependencies(game,error)){std::cerr<<error<<'\n';return 1;}
    std::ifstream input(game/L"bin64/dmm/manifest.tsv");std::ostringstream original;original<<input.rdbuf();
    const auto manifest=temporary/L"bin64/dmm/manifest.tsv";
    std::filesystem::create_directories(manifest.parent_path());
    std::ofstream badBody(temporary/L"overriding-body.pac",std::ios::binary);badBody<<"different installed body";badBody.close();
    auto altered=original.str();const std::string route="file\tcharacter/model/1_pc/2_phw/nude/cd_phw_00_nude_00_0001_damian.pac\t";
    const auto start=altered.find(route);if(start==std::string::npos)return 1;
    const auto path=start+route.size(),end=altered.find('\n',path);
    altered.replace(path,end-path,(temporary/L"overriding-body.pac").u8string());
    std::ofstream out(manifest,std::ios::binary);out<<altered;out.close();
    if(hex::verifyMountedDependencies(temporary,error)){std::cerr<<"Conflicting body was accepted.\n";return 1;}
    if(!hex::findWeightReference(temporary).empty()){std::cerr<<"Missing optional reference was accepted.\n";return 1;}
    const auto optional=hex::findWeightReference(game);
    if(!optional.empty()){
        const auto copy=temporary/L"mods/Sexy_Witches_fixture/files/0009/character/model/1_pc/2_phw/nude/cd_phw_00_nude_00_0001_damian.pac";
        std::filesystem::create_directories(copy.parent_path());std::filesystem::copy_file(optional,copy,std::filesystem::copy_options::overwrite_existing);
        if(hex::findWeightReference(temporary).empty()){std::cerr<<"Checked optional reference was not detected.\n";return 1;}
        std::ofstream altered(copy,std::ios::binary|std::ios::trunc);altered<<"different body";altered.close();
        if(!hex::findWeightReference(temporary).empty()){std::cerr<<"Unrecognized optional body was accepted.\n";return 1;}
    }
    std::cout<<"PASS: actual mounted VENUS and seven witch routes verified; conflicting renamed body rejected despite the correct installed source.\n";
    return 0;
}
