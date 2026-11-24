#include "runtime.h"
#include "shaping.h"
#include <iostream>
#include <stdexcept>

static void checkStatureInPlace(){
    hex::Profile profile;
    profile.height=.7f;
    const hex::PoseMatrix matrix{1,0,0,0,0,1,0,0,0,0,1,0,3,2,1,1};
    const std::vector<hex::PoseMatrix> base{matrix,matrix};
    auto inPlace=base;
    std::vector<hex::PoseMatrix> separate;
    std::string error;
    if(!hex::applyStature(profile,base,separate,error)||!hex::applyStature(profile,inPlace,inPlace,error)||
       inPlace!=separate||inPlace==base)throw std::runtime_error("in-place stature differs from the checked pose");
    inPlace[1][3]=1.0f;
    const auto before=inPlace;
    if(hex::applyStature(profile,inPlace,inPlace,error)||inPlace!=before)
        throw std::runtime_error("a rejected stature pose partially changed its input");
}

int wmain(int argc,wchar_t** argv){try{
    if(argc!=2&&argc!=3)throw std::runtime_error("dependency-copy path required");
    const bool onlyProbe=argc==3&&std::wstring_view(argv[2])==L"--probe-only";
    checkStatureInPlace();
    if(!onlyProbe&&hex::sha256(argv[1])!="92d7cab4e244004ea433d1079c42288a985627fce9cb585844030282e591f009")throw std::runtime_error("unrecognized dependency");
    HMODULE m=LoadLibraryExW(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES);
    if(!m)throw std::runtime_error("test image mapping failed");
    std::string error;
    if(!hex::probeSurgeryObserver(m,error)||!hex::probeSurgeryShaping(m,error))throw std::runtime_error(error);
    if(onlyProbe){
        auto* instruction=reinterpret_cast<unsigned char*>(m)+0xe960;
        DWORD protection=0;
        if(!VirtualProtect(instruction,1,PAGE_EXECUTE_READWRITE,&protection))throw std::runtime_error("fixture page could not be written");
        const auto original=*instruction;
        *instruction=static_cast<unsigned char>(original^1);
        const bool isolated=hex::probeSurgeryObserver(m,error)&&!hex::probeSurgeryShaping(m,error);
        *instruction=original;
        DWORD unused=0;
        VirtualProtect(instruction,1,protection,&unused);
        FreeLibrary(m);
        if(!isolated)throw std::runtime_error("changed shaping interface disabled an unrelated observer");
        std::cout<<"PASS: unknown checksum accepted with compatible native signatures; a changed shaping signature leaves the observer usable. No plugin code executed.\n";
        return 0;
    }
    auto fn=reinterpret_cast<void*>(GetProcAddress(m,"ModHubMenuV1"));
    if(!fn)throw std::runtime_error("menu export absent");
    auto base=reinterpret_cast<unsigned char*>(m);
    auto& initialized=*reinterpret_cast<unsigned char*>(base+0x1CD4A0);
    auto& window=*reinterpret_cast<uintptr_t*>(base+0x1CD5E0);
    auto& visible=*reinterpret_cast<unsigned char*>(base+0x1CD028);
    initialized=0;window=0;visible=0;
    if(hex::callMenu(fn,0,0)!=0||hex::callMenu(fn,3,0)!=0||visible!=0)throw std::runtime_error("not-ready guard failed");
    initialized=1;window=1;
    if(hex::callMenu(fn,0,0)!=1)throw std::runtime_error("ready command failed");
    if(hex::callMenu(fn,3,0)!=1||visible!=1)throw std::runtime_error("open command failed");
    if(hex::callMenu(fn,4,0)!=1||visible!=0)throw std::runtime_error("close command failed");
    FreeLibrary(m);
    std::cout<<"PASS: actual Instant Surgery 0.9.6 menu export ABI and ready/open/close behavior in an isolated image.\nDllMain, game hooks, editor rendering and body deformation were not executed.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
