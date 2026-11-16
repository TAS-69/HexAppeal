#include "dancer_palette.h"
#include "wardrobe_resources.h"
#include "runtime.h"
#include <Windows.h>
#include <fstream>
#include <iostream>
#include <stdexcept>
static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static std::filesystem::path extended(const std::filesystem::path& p){return L"\\\\?\\"+std::filesystem::absolute(p).lexically_normal().make_preferred().wstring();}
int wmain(int argc,wchar_t** argv){try{
    check(argc==3,"game and isolated fixture required");const auto dependencies=hex::inspectDependencies(argv[1],{});const auto actual=hex::inspectDancerResources(dependencies,true);check(actual.ready,"actual Dancer resources");std::string error;check(hex::verifyDancerPalette(actual.sourceRoot,error),error.c_str());
    const auto root=std::filesystem::absolute(argv[2]);const auto texture=root/L"files/0009"/hex::dancerTexture;const auto fixtureSource=root/L"external-source";std::filesystem::create_directories(extended(texture.parent_path()));
    for(const auto& colour:hex::dancerColours){const auto from=actual.sourceRoot/colour.folder/hex::dancerTexture,to=fixtureSource/colour.folder/hex::dancerTexture;std::filesystem::create_directories(extended(to.parent_path()));check(CopyFileW(extended(from).c_str(),extended(to).c_str(),TRUE)!=0,"copy read-only dependency into isolated fixture");}
    const auto originalHashes=[&](bool fixtures){for(const auto& colour:hex::dancerColours){check(hex::sha256(actual.sourceRoot/colour.folder/hex::dancerTexture)==colour.sha256,"original upstream DDS remains byte-for-byte unchanged");if(fixtures)check(hex::sha256(fixtureSource/colour.folder/hex::dancerTexture)==colour.sha256,"fixture source content remains unchanged by binding");}};
    const auto source=fixtureSource/hex::dancerColours[0].folder/hex::dancerTexture;check(CreateHardLinkW(extended(texture).c_str(),extended(source).c_str(),nullptr)!=0,"create isolated source link");
    check(!hex::changeDancerBinding(texture,fixtureSource,1,error),"unmarked folder is never changed");
    {std::ofstream marker(extended(root/L"HEX-APPEAL-DANCER-BINDINGS.txt"),std::ios::binary);marker<<hex::dancerBindingMarker;}
    for(int i=1;i<6;++i){const int colour=i%5;check(hex::changeDancerBinding(texture,fixtureSource,colour,error),error.c_str());check(hex::sha256(texture)==hex::dancerColours[colour].sha256,"active global colour matches supplied texture");originalHashes(true);}
    const auto locked=CreateFileW(extended(texture).c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);check(locked!=INVALID_HANDLE_VALUE,"lock active texture fixture");check(!hex::changeDancerBinding(texture,fixtureSource,2,error)&&hex::sha256(texture)==hex::dancerColours[0].sha256,"locked binding cannot change or alter source");CloseHandle(locked);
    check(!hex::changeDancerBinding(texture,fixtureSource,9,error),"invalid colour rejected");
    const auto updated=fixtureSource/hex::dancerColours[1].folder/hex::dancerTexture;{std::ofstream file(extended(updated),std::ios::binary|std::ios::app);file.put(0);}
    const auto changedHash=hex::sha256(updated);check(changedHash!=hex::dancerColours[1].sha256&&hex::verifyDancerPalette(fixtureSource,error),"unverified but readable DDS palette is usable");
    check(hex::changeDancerBinding(texture,fixtureSource,1,error)&&hex::sha256(texture)==changedHash&&hex::sha256(updated)==changedHash,"unverified source binding uses its actual digest without altering source contents");
    originalHashes(false);
    std::cout<<"PASS: five supplied colours, atomic managed-link switching, unmarked/locked/invalid rejection, all upstream textures preserved.\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
