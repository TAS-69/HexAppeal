#include "runtime.h"
#include "core/state.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int wmain(int argc,wchar_t** argv){try{
    require(argc==2,"profile path required");hex::Runtime runtime;runtime.profiles=argv[1];
    std::filesystem::create_directories(runtime.profiles.parent_path());const auto file=runtime.profiles.parent_path()/L"interface.ini";
    {std::ofstream old(file);old<<"[Interface]\nVersion=1\nCompact=1\nUnderwearHiddenMask=8\n";}
    runtime.loadInterface(file);require(runtime.blockGameInput&&runtime.compact&&runtime.underwearHiddenMask==8,"legacy preferences retain shapes/visibility and default input blocking on");
    for(uint32_t bit=1;bit<=hex::PinDancerColour;bit<<=1){runtime.quickPins=bit;runtime.blockGameInput=false;require(runtime.saveInterface(),"save interface");hex::Runtime restored;restored.loadInterface(file);require(restored.quickPins==bit&&!restored.blockGameInput&&restored.compact&&restored.underwearHiddenMask==8,"each pin and input choice persist independently");}
    {std::ofstream corrupt(file);corrupt<<"[Interface]\nQuickPins=-1\nBlockGameInput=1\n";}
    runtime.loadInterface(file);require(runtime.quickPins==hex::AllQuickPins&&runtime.blockGameInput&&ml::State::Get().blockGameInput,"unknown pin bits bounded and blocking loaded atomically");
    std::cout<<"PASS: safe default blocking, legacy migration, all twelve independent pins, minimise preference and unchanged underwear choices.\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}}
