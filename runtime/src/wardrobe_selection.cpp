#include "wardrobe_selection.h"
#include <Windows.h>
#include <fstream>
#include <set>

namespace hex {
namespace {
bool write(const std::filesystem::path& path,const WardrobeSelectionSnapshot& state,std::string& error){
    if(path.empty()){error="Wardrobe settings folder is unavailable.";return false;}
    std::error_code ec;std::filesystem::create_directories(path.parent_path(),ec);
    if(ec){error="Wardrobe settings folder is unavailable.";return false;}
    const auto temporary=std::filesystem::path(path.wstring()+L"."+std::to_wstring(GetCurrentProcessId())+L".tmp");
    std::ofstream out(temporary,std::ios::binary|std::ios::trunc);
    if(!out){error="Wardrobe choices could not be saved.";return false;}
    out<<"[Wardrobe]\nVersion=2\n";
    for(size_t i=0;i<state.choices.size();++i){const auto& c=state.choices[i];out<<"\n[Witch"<<i<<"]\nOutfit="<<int(c.outfit)<<"\nUnderwearColour="<<int(c.underwear)<<"\nDancerColour="<<int(c.legacyColour)<<'\n';if(c.outfit==WitchOutfit::Provider)out<<"ProviderOutfit="<<c.provider<<'\n';}
    out.flush();const bool saved=bool(out);out.close();
    if(!saved||!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){error="Wardrobe choices could not be saved atomically.";return false;}
    return true;
}
bool number(const std::string& value,int& out){
    if(value.empty()||value.size()>2)return false;out=0;
    for(const unsigned char c:value){if(c<'0'||c>'9')return false;out=out*10+c-'0';}return true;
}
}
bool WardrobeSelection::load(const std::filesystem::path& path,std::string& error){
    std::lock_guard<std::mutex> lock(mutex);std::error_code ec;
    if(!std::filesystem::exists(path,ec)&&!ec){file=path;state={};state.generations.fill(1);return true;}
    const auto size=std::filesystem::file_size(path,ec);if(ec||size>16384){error="Wardrobe settings could not be read.";return false;}
    std::ifstream input(path,std::ios::binary);if(!input){error="Wardrobe settings could not be read.";return false;}
    WardrobeSelectionSnapshot next;next.generations.fill(1);std::string line,section;std::set<std::string> keys;bool version=false;
    while(std::getline(input,line)){
        if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty())continue;
        if(line.front()=='['&&line.back()==']'){section=line.substr(1,line.size()-2);continue;}
        const auto equals=line.find('=');if(equals==std::string::npos){error="Wardrobe settings contain an invalid row.";return false;}
        const auto name=line.substr(0,equals),value=line.substr(equals+1);int parsed=0;
        if(!keys.insert(section+":"+name).second||(name!="ProviderOutfit"&&!number(value,parsed))){error="Wardrobe settings contain an invalid or duplicate value.";return false;}
        if(section=="Wardrobe"&&name=="Version"){if(parsed!=1&&parsed!=2){error="This wardrobe settings version is unsupported.";return false;}version=true;continue;}
        if(section.size()!=6||section.rfind("Witch",0)||section[5]<'0'||section[5]>'5'){error="Wardrobe settings name an unsupported witch.";return false;}
        auto& c=next.choices[size_t(section[5]-'0')];
        if(name=="Outfit"&&parsed<=2)c.outfit=WitchOutfit(parsed);
        else if(name=="ProviderOutfit"&&validWardrobeId(value))c.provider=value;
        else if(name=="UnderwearColour"&&parsed<=5)c.underwear=WitchColour(parsed);
        else if(name=="DancerColour"&&parsed<=5)c.legacyColour=WitchColour(parsed);
        else{error="Wardrobe settings contain an unsupported choice.";return false;}
    }
    if(!version||input.bad()){error="Wardrobe settings are incomplete.";return false;}
    for(const auto& choice:next.choices)if((choice.outfit==WitchOutfit::Provider)!=!choice.provider.empty()){error="Wardrobe provider selection is incomplete.";return false;}
    file=path;state=next;return true;
}
WardrobeSelectionSnapshot WardrobeSelection::snapshot()const{std::lock_guard<std::mutex> lock(mutex);return state;}
bool WardrobeSelection::change(int target,WardrobeField field,int value,const WardrobeCapabilities& available,std::string& error){
    std::lock_guard<std::mutex> lock(mutex);
    if(target< -1||target>5){error="Choose a supported witch.";return false;}
    if(field!=WardrobeField::Outfit||value!=0){error="Choose Default or a registered provider outfit.";return false;}
    if(!available.switchReady){error="The native outfit switch is unavailable.";return false;}
    auto next=state;bool changed=false;
    for(size_t i=0;i<next.choices.size();++i){if(target>=0&&size_t(target)!=i)continue;auto& choice=next.choices[i];const auto before=choice;
        choice.outfit=WitchOutfit::Default;choice.provider.clear();
        if(!(before==choice)){++next.generations[i];changed=true;}
    }
    if(!changed)return true;
    if(!write(file,next,error))return false;
    state=next;return true;
}
bool WardrobeSelection::providerOutfit(int target,const std::string& id,std::string& error){
    std::lock_guard<std::mutex> lock(mutex);if(target< -1||target>5||!validWardrobeId(id)||id.rfind("hexappeal/",0)==0){error="Choose a registered provider outfit.";return false;}
    auto next=state;bool changed=false;for(int i=0;i<6;++i)if(target<0||target==i){auto& choice=next.choices[i];if(choice.outfit!=WitchOutfit::Provider||choice.provider!=id){choice.outfit=WitchOutfit::Provider;choice.provider=id;++next.generations[i];changed=true;}}
    if(!changed)return true;if(!write(file,next,error))return false;state=next;return true;
}
void WardrobeSelection::invalidate(int target,const std::string& id){std::lock_guard<std::mutex> lock(mutex);for(int i=0;i<6;++i)if((target<0||target==i)&&wardrobeOutfitId(state.choices[i])==id)++state.generations[i];}
}
