#include "wardrobe_selection.h"
#include "wardrobe_refresh.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
static void check(bool value,const char* text){if(!value)throw std::runtime_error(text);}
int wmain(int argc,wchar_t** argv){try{
    check(argc==2,"fixture path required");const std::filesystem::path path=argv[1];std::error_code ec;
    std::filesystem::create_directories(path.parent_path());
    {std::ofstream reset(path);reset<<"[Wardrobe]\nVersion=1\n";}
    hex::WardrobeSelection choices;std::string error;check(choices.load(path,error),"load defaults");
    const hex::WardrobeCapabilities ready{true,true,true,true},none{};
    auto baseline=choices.snapshot();check(!choices.change(3,hex::WardrobeField::Outfit,1,none,error)&&choices.snapshot().choices==baseline.choices,"missing native route preserves every choice");
    auto missingAsset=ready;missingAsset.dancerAssets=false;
    check(!choices.change(3,hex::WardrobeField::Outfit,1,missingAsset,error),"Dancer requires its external assets");
    check(choices.change(3,hex::WardrobeField::Outfit,1,ready,error),"select Elowen Dancer");
    check(choices.change(3,hex::WardrobeField::UnderwearColour,2,ready,error),"select independent underwear colour");
    check(choices.change(-1,hex::WardrobeField::DancerColour,3,ready,error),"all-witch dress colour");
    auto state=choices.snapshot();
    for(int i=0;i<6;++i){check(state.choices[i].dancer==hex::WitchColour::Crimson,"all targets updated");check(state.choices[i].underwear==(i==3?hex::WitchColour::Black:hex::WitchColour::Original),"all-witch edit retains individual underwear choice");check(state.choices[i].outfit==(i==3?hex::WitchOutfit::Dancer:hex::WitchOutfit::Default),"colour edit retains individual outfit");}
    hex::WardrobeSelection restored;check(restored.load(path,error)&&restored.snapshot().choices==state.choices,"all choices reload");
    check(choices.change(3,hex::WardrobeField::Outfit,0,missingAsset,error),"returning to Default remains possible without Dancer assets");
    check(choices.providerOutfit(3,"example/dress",error)&&restored.load(path,error)&&hex::wardrobeOutfitId(restored.snapshot().choices[3])=="example/dress","provider stable ID survives settings reload");
    check(choices.change(3,hex::WardrobeField::Outfit,0,missingAsset,error)&&choices.snapshot().choices[3].provider.empty(),"Default clears the provider choice");
    const auto before=choices.snapshot();
    {std::ofstream corrupt(path);corrupt<<"[Wardrobe]\nVersion=1\n[Witch3]\nOutfit=1\nUnderwearColour=99\n";}
    check(!choices.load(path,error)&&choices.snapshot().choices==before.choices,"late malformed row cannot partially load");
    const auto blocked=path.parent_path()/L"save-failure-blocker";std::filesystem::remove(blocked,ec);
    hex::WardrobeSelection failed;check(failed.load(blocked/L"wardrobe.ini",error),"nonexistent child path loads defaults");
    {std::ofstream file(blocked);file<<"file, not a folder";}
    const auto old=failed.snapshot();check(!failed.change(2,hex::WardrobeField::Outfit,1,ready,error)&&failed.snapshot().choices==old.choices&&failed.snapshot().generations==old.generations,"failed save cannot activate or advance a choice");
    check(!choices.change(6,hex::WardrobeField::Outfit,1,ready,error),"unknown witch rejected");
    check(!choices.change(0,hex::WardrobeField::DancerColour,-1,ready,error),"unknown colour rejected");
    hex::WardrobeRefreshCycle refresh;refresh.request(10);check(!refresh.begin(false)&&refresh.requested,"external or absent outfits cannot start a refresh");
    check(refresh.begin(true)&&refresh.phase==hex::WardrobeRefreshCycle::Original,"refresh removes the owned replacement first");
    check(!refresh.acknowledged(true)&&refresh.phase==hex::WardrobeRefreshCycle::Original,"no second leg before native original acknowledgement");
    refresh.request(10);check(refresh.acknowledged(false)&&refresh.phase==hex::WardrobeRefreshCycle::Selected,"acknowledged original arms selected outfit restoration");
    refresh.applyingSelected();check(!refresh.requested,"rapid changes during the first leg coalesce into latest resources");
    refresh.request(10);check(refresh.acknowledged(true)&&refresh.pending()&&refresh.begin(true),"a colour saved during the second leg schedules another refresh");
    refresh.sync(11);check(!refresh.pending()&&refresh.phase==hex::WardrobeRefreshCycle::Idle,"new user selection cancels both refresh legs");
    refresh.request(11);check(refresh.begin(true)&&refresh.acknowledged(false),"start a clean refresh");refresh.applyingSelected();check(refresh.acknowledged(true)&&!refresh.pending(),"completed refresh settles without a repeated swap");
    refresh.request(12,1000,1500);check(refresh.begin(true)&&refresh.acknowledged(false,1100),"acknowledged original starts the resource settling interval");
    check(!refresh.readyToRestore(2599)&&refresh.readyToRestore(2600),"restoration waits for the complete interval without sleeping the game thread");
    refresh.request(12,2000,1500);check(!refresh.readyToRestore(3499)&&refresh.readyToRestore(3500),"an edit while settling extends the interval for the latest resources");
    refresh.applyingSelected();check(refresh.acknowledged(true,3501)&&!refresh.pending(),"delayed restoration settles once");
    std::cout<<"PASS: per-witch and All outfit/colour persistence, independent fields, dependency gates, restore without optional assets and atomic failure preservation.\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
