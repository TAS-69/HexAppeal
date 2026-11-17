#include "wardrobe_resources.h"
#include "runtime.h"
#include "dancer_palette.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int wmain(int argc,wchar_t** argv){try{
    require(argc==3,"game and isolated fixture folder required");
    const auto dependencies=hex::inspectDependencies(argv[1],{});
    const auto actual=hex::inspectDancerResources(dependencies,true);
    require(actual.ready,actual.detail.c_str());
    require(!hex::inspectDancerResources(dependencies,false).ready,"loaded runtime required");
    auto missing=dependencies;missing.bodyVerified=false;
    require(!hex::inspectDancerResources(missing,true).ready,"active body required");
    missing=dependencies;missing.dancer={};
    require(!hex::inspectDancerResources(missing,true).ready,"separate optional source required");
    const auto fixture=std::filesystem::absolute(std::filesystem::path(argv[2])).make_preferred();
    auto file=fixture;for(int i=0;i<12;++i)file/=L"long dependency folder \u00e9\u6f22";file/=L"hash.txt";
    const auto extended=std::filesystem::path(L"\\\\?\\"+file.wstring());
    std::filesystem::create_directories(extended.parent_path());
    {std::ofstream out(extended,std::ios::binary);out<<"Hex Appeal long path fixture\n";require(bool(out),"create long Unicode path fixture");}
    require(file.wstring().size()>300&&hex::sha256(file)=="581f9beb8b2d749110d76dd344e101641b1fa327333c4ad2167719e603bd741b","long Unicode dependency hash");
    missing=dependencies;missing.manifest.path=fixture/L"missing.tsv";
    require(!hex::inspectDancerResources(missing,true).ready,"missing mounted resources rejected");
    std::cout<<"PASS: actual separate Dancer resources, loaded-runtime/body/source gates, absent routes and hashes through long Unicode paths.\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
