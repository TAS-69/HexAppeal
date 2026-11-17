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
    const hex::WardrobeCapabilities ready{true},none{};
    auto baseline=choices.snapshot();check(!choices.change(3,hex::WardrobeField::Outfit,0,none,error)&&choices.snapshot().choices==baseline.choices,"missing native route preserves every choice");
    check(!choices.change(3,hex::WardrobeField::Outfit,1,ready,error),"removed built-in replacement cannot be selected");
    check(choices.providerOutfit(3,"example/dress",error),"select registered outfit after provider validation");
    auto state=choices.snapshot();hex::WardrobeSelection restored;
    check(restored.load(path,error)&&restored.snapshot().choices==state.choices,"provider choices reload");
    check(choices.providerOutfit(-1,"example/alternate",error),"All witches selects one provider");
    for(const auto& choice:choices.snapshot().choices)check(hex::wardrobeOutfitId(choice)=="example/alternate","all provider IDs updated");
    check(choices.change(3,hex::WardrobeField::Outfit,0,ready,error)&&choices.snapshot().choices[3].provider.empty(),"Default clears only that provider choice");
    {std::ofstream legacy(path);legacy<<"[Wardrobe]\nVersion=2\n[Witch3]\nOutfit=1\nUnderwearColour=2\nDancerColour=3\n";}
    check(choices.load(path,error)&&choices.snapshot().choices[3].outfit==hex::WitchOutfit::Legacy,"legacy selection loads without activating a built-in outfit");
    check(choices.providerOutfit(0,"example/dress",error)&&restored.load(path,error),"editing another witch preserves old preferences");
    const auto oldChoice=restored.snapshot().choices[3];
    check(oldChoice.outfit==hex::WitchOutfit::Legacy&&oldChoice.legacyColour==hex::WitchColour::Crimson&&oldChoice.underwear==hex::WitchColour::Black,"legacy outfit and colour values survive an unrelated save");
    check(choices.change(3,hex::WardrobeField::Outfit,0,ready,error),"Default restores an unavailable legacy selection");
    const auto before=choices.snapshot();
    {std::ofstream corrupt(path);corrupt<<"[Wardrobe]\nVersion=1\n[Witch3]\nOutfit=1\nUnderwearColour=99\n";}
    check(!choices.load(path,error)&&choices.snapshot().choices==before.choices,"late malformed row cannot partially load");
    const auto blocked=path.parent_path()/L"save-failure-blocker";std::filesystem::remove(blocked,ec);
    hex::WardrobeSelection failed;check(failed.load(blocked/L"wardrobe.ini",error),"nonexistent child path loads defaults");
    {std::ofstream file(blocked);file<<"file, not a folder";}
    const auto old=failed.snapshot();check(!failed.providerOutfit(2,"example/dress",error)&&failed.snapshot().choices==old.choices&&failed.snapshot().generations==old.generations,"failed save cannot activate or advance a choice");
    check(!choices.change(6,hex::WardrobeField::Outfit,0,ready,error),"unknown witch rejected");
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
    for(const uint32_t delay:{250u,500u}){
        refresh.request(13,1000,delay);check(refresh.begin(true)&&refresh.acknowledged(false,1100),"short refresh starts only after the original assembly is acknowledged");
        check(!refresh.readyToRestore(1100+delay-1)&&refresh.readyToRestore(1100+delay),"short refresh respects its exact nonblocking boundary");
        refresh.request(13,1150,delay);check(!refresh.readyToRestore(1150+delay-1)&&refresh.readyToRestore(1150+delay),"a later edit restarts the short window instead of restoring stale resources");
        refresh.applyingSelected();check(refresh.acknowledged(true,1150+delay)&&!refresh.pending(),"short refresh completes once after selected assembly acknowledgement");
    }
    std::cout<<"PASS: per-witch and All provider persistence, inert legacy preferences, Default restoration and atomic failure preservation.\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
