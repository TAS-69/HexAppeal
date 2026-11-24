#include "character_addons.h"
#include "model.h"
#include "wardrobe_snapshot.h"
#include "mounted_dependencies.h"
#include "runtime.h"
#include "core/log.h"
#include "release_version.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <fstream>
#include <iomanip>
#include <limits>
#include <mutex>
#include <sstream>
#include <windows.h>

namespace hex {
namespace {
constexpr size_t maxPackSize=32*1024;
constexpr std::string_view dmmCharacterPrefix="character/hexappeal/characters/";
constexpr std::string_view packStart="<HexAppealCharacters version=\"1\"><![CDATA[",packEnd="]]></HexAppealCharacters>";
std::vector<CharacterAddon> addons;
std::vector<CharacterAddonPack> packs;
std::vector<int> addonOrder;
std::atomic<uint64_t> enabledPacks{0};
std::atomic<uint32_t> underwearMask{0};
std::atomic<uint32_t> venusBodies{0},venusUnderwear{0};
std::filesystem::path preferences;
std::mutex profileMutex;
std::array<AddonBodyProfile,maximumAddonCharacters> profiles;
std::array<std::string,maximumAddonCharacters> shapeDetails;

using Version=std::array<unsigned,3>;
bool parseVersion(std::string_view text,Version& version){
    for(size_t i=0;i<version.size();++i){
        const auto end=text.find('.');
        if((end==text.npos)!=(i+1==version.size()))return false;
        const auto field=text.substr(0,end);
        if(field.empty())return false;
        const auto parsed=std::from_chars(field.data(),field.data()+field.size(),version[i]);
        if(parsed.ec!=std::errc{}||parsed.ptr!=field.data()+field.size())return false;
        if(end!=text.npos)text.remove_prefix(end+1);
    }
    return true;
}
Profile readProfile(const wchar_t* value){
    std::wistringstream in(value);
    in.imbue(std::locale::classic());
    unsigned top=0,bottom=0;
    Profile profile;
    if(!(in>>top>>bottom>>profile.topBlend>>profile.bottomBlend>>profile.height>>profile.weight)||
       top>2||bottom>2)return {};
    profile.top=Preset(top);profile.bottom=Preset(bottom);
    in>>std::ws;
    return in.eof()&&profile.valid()?profile:Profile{};
}

bool identifier(std::string_view value,size_t length,bool allowUpper=false){
    return !value.empty()&&value.size()<=length&&std::all_of(value.begin(),value.end(),[=](char c){
        return (c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||(allowUpper&&c>='A'&&c<='Z');
    });
}
bool label(std::string_view value){
    return !value.empty()&&value.size()<=63&&value.find("##")==value.npos&&
        std::all_of(value.begin(),value.end(),[](unsigned char c){return c>=32&&c<=126;});
}
bool appearance(std::string_view value){
    constexpr std::string_view prefix="character/appearance/",suffix=".app_xml";
    if(value.size()<=prefix.size()+suffix.size()||value.size()>=256||value.substr(0,prefix.size())!=prefix||
       value.substr(value.size()-suffix.size())!=suffix)return false;
    for(size_t start=0,end=value.find('/');end!=value.npos;start=end+1,end=value.find('/',start))
        if(!identifier(value.substr(start,end-start),96))return false;
    const auto leaf=value.substr(value.rfind('/')+1);
    return leaf.size()>suffix.size()&&identifier(leaf.substr(0,leaf.size()-suffix.size()),96);
}
std::vector<std::string> fields(const std::string& line){
    std::vector<std::string> result;
    size_t begin=0;
    for(auto next=line.find('\t');next!=line.npos;next=line.find('\t',begin)){
        result.push_back(line.substr(begin,next-begin));
        begin=next+1;
    }
    result.push_back(line.substr(begin));
    return result;
}
bool matchesBuiltIn(const CharacterAddon& entry){
    for(const auto& witch:witches)
        for(uint32_t i=0;i<witch.target.keyCount;++i)
            if(witch.target.characterRecordKeys[i]==entry.recordKey)return true;
    return wardrobeWitchForAppearance(entry.appearance.c_str())>=0;
}
bool readPack(const std::filesystem::path& path,bool mounted,CharacterAddonPack& pack,
              std::vector<CharacterAddon>& result,std::string& error){
    std::error_code ec;
    if(std::filesystem::file_size(path,ec)>maxPackSize||ec){error="could not read a character list under 32 KiB";return false;}
    std::ifstream in(path);
    std::string line;
    if(!std::getline(in,line)){error="the character list is empty or unreadable";return false;}
    constexpr std::string_view utf8Bom="\xef\xbb\xbf";
    if(line.compare(0,utf8Bom.size(),utf8Bom)==0)line.erase(0,utf8Bom.size());
    if(mounted&&(line!=packStart||!std::getline(in,line))){error="expected the mounted character add-on format";return false;}
    if(line!="HexAppealCharacters\t1"){error="expected a HexAppealCharacters version 1 file";return false;}
    unsigned lineNumber=mounted?2:1;
    bool closed=!mounted;
    for(;std::getline(in,line);){
        ++lineNumber;
        if(mounted&&line==packEnd){
            in>>std::ws;
            closed=in.eof()&&!in.bad();
            break;
        }
        if(line.empty()||(line.front()=='#'&&line.rfind("#patch\t",0)!=0))continue;
        const auto row=fields(line);
        error="invalid entry on line "+std::to_string(lineNumber);
        if(row[0]=="#patch"){
            Version version{},minimum{};
            if(row.size()!=4||!result.empty()||!pack.version.empty()||!label(row[1])||
               !parseVersion(row[2],version)||!parseVersion(row[3],minimum))return false;
            pack.label=row[1];pack.version=row[2];pack.minimumVersion=row[3];
            continue;
        }
        if(row[0]=="character"){
            if(row.size()!=6||result.size()>=maximumAddonCharacters||!identifier(row[1],64)||!label(row[2])||
               !identifier(row[4],95,true)||!appearance(row[5]))return false;
            uint32_t key=0;
            const auto parsed=std::from_chars(row[3].data(),row[3].data()+row[3].size(),key);
            if(parsed.ec!=std::errc{}||parsed.ptr!=row[3].data()+row[3].size()||!key)return false;
            CharacterAddon entry;
            entry.id=row[1];entry.label=row[2];entry.recordKey=key;entry.record=row[4];entry.appearance=row[5];
            if(matchesBuiltIn(entry)){error="line "+std::to_string(lineNumber)+" overlaps a built-in witch";return false;}
            result.push_back(std::move(entry));
            continue;
        }
        if(row.size()!=4||result.empty()||(row[0]!="default"&&row[0]!="item")||!label(row[2])||
           !validOutfitPart(row[3])||protectedOutfitPart(row[3]))return false;
        const auto slot=outfitSlotForPart(row[3]);
        if(slot.empty())return false;
        const auto owner=std::find_if(result.begin(),result.end(),[&](const auto& entry){return entry.id==row[1];});
        if(owner==result.end()||owner->items.size()>=64||std::any_of(owner->items.begin(),owner->items.end(),
            [&](const auto& item){return item.prefab==row[3];}))return false;
        if(row[0]=="default"){
            if(outfitSlotParts(owner->defaults).size()>=24)return false;
            owner->defaults[slot].insert(row[3]);
        }
        owner->items.push_back({row[2],slot,row[3]});
    }
    if(in.bad()||!closed||result.empty()){error="the character list is incomplete or unreadable";return false;}
    if(std::any_of(result.begin(),result.end(),[](const auto& entry){return entry.defaults.empty();})){
        error="every character needs at least one checked default outfit part";return false;
    }
    return true;
}
}

CharacterAddonScan scanCharacterAddons(const std::filesystem::path& folder,const std::filesystem::path& dmmManifest,
                                      std::string_view coreVersion){
    CharacterAddonScan result;
    struct Pack {std::filesystem::path path;std::string name;bool mounted;};
    std::vector<Pack> files;
    std::set<std::string> mountedNames;
    if(!dmmManifest.empty()){
        const auto routes=readMountedRoutes(dmmManifest);
        for(const auto& [route,path]:routes){
            if(route.compare(0,dmmCharacterPrefix.size(),dmmCharacterPrefix)!=0)continue;
            const std::string_view leaf=std::string_view(route).substr(dmmCharacterPrefix.size());
            if(leaf.size()<=4||leaf.size()>68||leaf.substr(leaf.size()-4)!=".xml")continue;
            const auto id=leaf.substr(0,leaf.size()-4);
            if(id.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_-")!=id.npos)continue;
            mountedNames.emplace(id);
            files.push_back({path,std::string(leaf),true});
        }
    }
    std::error_code ec;
    if(!folder.empty()&&std::filesystem::exists(folder,ec)){
        for(std::filesystem::directory_iterator it(folder,ec),end;!ec&&it!=end;it.increment(ec)){
            const auto& path=it->path();
            if(path.extension()!=L".tsv"||!it->is_regular_file(ec))continue;
            const auto id=path.stem().u8string();
            if(id.empty()||id.size()>64||id.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_-")!=id.npos||
               mountedNames.count(id))continue;
            files.push_back({path,path.filename().u8string(),false});
        }
    }
    if(ec)result.messages.push_back("Could not scan the Hex Appeal Characters folder.");
    if(files.size()>64){result.messages.push_back("The Characters folder contains more than 64 manifests.");return result;}
    std::sort(files.begin(),files.end(),[](const auto& left,const auto& right){return left.name<right.name;});
    std::vector<CharacterAddon> all;
    Version core{};
    const bool knownCore=parseVersion(coreVersion.substr(0,coreVersion.find('-')),core);
    for(const auto& file:files){
        const size_t index=result.packs.size();
        auto& pack=result.packs.emplace_back();
        pack.id=file.name.substr(0,file.name.size()-4);
        std::vector<CharacterAddon> loaded;
        std::string error;
        if(!readPack(file.path,file.mounted,pack,loaded,error)){
            if(pack.label.empty())pack.label=pack.id;
            pack.detail=error;
            result.messages.push_back(file.name+": "+error);
            continue;
        }
        pack.declared=loaded.size();
        if(pack.label.empty()){
            pack.label=loaded.front().label;
            if(loaded.size()==2)pack.label+=" and "+loaded[1].label;
            else if(loaded.size()>2)pack.label+=" and "+std::to_string(loaded.size()-1)+" more";
        }
        Version minimum{};
        if(knownCore&&parseVersion(pack.minimumVersion,minimum)&&core<minimum){
            pack.detail="Requires Hex Appeal "+pack.minimumVersion+" or later.";
            result.messages.push_back(pack.label+": "+pack.detail);
            continue;
        }
        if(pack.version.empty())pack.detail="Legacy patch without version metadata. Its character records passed validation.";
        else pack.detail="Character records passed validation.";
        for(auto& entry:loaded){entry.pack=index;all.push_back(std::move(entry));}
    }
    for(size_t i=0;i<all.size();++i){
        const auto& entry=all[i];
        auto& pack=result.packs[entry.pack];
        bool conflict=false;
        for(size_t j=0;j<all.size();++j){
            const auto& other=all[j];
            if(i!=j&&(other.id==entry.id||other.recordKey==entry.recordKey||other.record==entry.record||
               other.appearance==entry.appearance)){conflict=true;break;}
        }
        if(conflict||result.characters.size()>=maximumAddonCharacters)
            result.messages.push_back(pack.id+": skipped "+entry.label+
                (conflict?" because its identity conflicts with another add-on.":" because the character limit was reached."));
        else {result.characters.push_back(entry);++pack.accepted;}
    }
    for(auto& pack:result.packs)if(pack.accepted<pack.declared&&pack.detail.find("Requires Hex Appeal ")!=0)
        pack.detail+=(pack.accepted?" Some entries were skipped":" Entries were skipped")+
            std::string(" because of conflicting identities or the character limit.");
    return result;
}
void initializeCharacterAddons(const std::filesystem::path& settings,const std::filesystem::path& dmmManifest){
    if(settings.empty()&&dmmManifest.empty())return;
    try{
        auto scan=scanCharacterAddons(settings.empty()?settings:settings/L"Characters",dmmManifest,HEX_APPEAL_BUILD_VERSION);
        for(const auto& message:scan.messages)LOG_ERR("Character add-on: %s",message.c_str());
        addons=std::move(scan.characters);
        packs=std::move(scan.packs);
        addonOrder.clear();
        addonOrder.reserve(addons.size());
        for(size_t i=0;i<addons.size();++i)addonOrder.push_back(firstAddonCharacter+int(i));
        const auto folded=[](std::string value){
            for(auto& c:value)if(c>='A'&&c<='Z')c=char(c-'A'+'a');
            return value;
        };
        std::sort(addonOrder.begin(),addonOrder.end(),[&](int left,int right){
            const auto& a=*characterAddon(left);
            const auto& b=*characterAddon(right);
            const auto x=folded(a.label),y=folded(b.label);
            return x!=y?x<y:a.id<b.id;
        });
        preferences=settings.empty()?settings:settings/L"CharacterAddons.ini";
        uint64_t enabled=0;
        for(size_t i=0;i<packs.size();++i){
            const auto& pack=packs[i];
            const std::wstring id(pack.id.begin(),pack.id.end());
            if(pack.accepted&&(preferences.empty()||GetPrivateProfileIntW(L"Enabled",id.c_str(),1,preferences.c_str())))
                enabled|=1ull<<i;
        }
        enabledPacks.store(enabled);
        uint32_t hidden=0;
        profiles={};
        shapeDetails={};
        for(size_t i=0;i<addons.size()&&!preferences.empty();++i){
            const auto& key=addons[i].id;
            const std::wstring id(key.begin(),key.end());
            if(GetPrivateProfileIntW(L"HideBaseUnderwear",id.c_str(),0,preferences.c_str()))hidden|=1u<<i;
            wchar_t value[192]{};
            if(GetPrivateProfileStringW(L"Shape",id.c_str(),L"",value,192,preferences.c_str()))
                profiles[i]={readProfile(value),1};
        }
        underwearMask.store(hidden);
        venusBodies=0;venusUnderwear=0;
        for(const auto& entry:addons)LOG("Character add-on: %s (%s), record %u, appearance %s.",
            entry.label.c_str(),entry.id.c_str(),entry.recordKey,entry.appearance.c_str());
    }catch(const std::exception& error){LOG_ERR("Character add-ons could not be loaded: %s",error.what());}
}
const std::vector<CharacterAddon>& characterAddons(){return addons;}
const std::vector<CharacterAddonPack>& characterAddonPacks(){return packs;}
bool characterAddonPackEnabled(size_t pack){
    return pack<packs.size()&&(enabledPacks.load()&(1ull<<pack))!=0;
}
bool setCharacterAddonPackEnabled(size_t pack,bool enabled,std::string& error){
    if(pack>=packs.size()||!packs[pack].accepted){error="This character patch has no usable entries.";return false;}
    if(!preferences.empty()){
        const auto& key=packs[pack].id;
        const std::wstring id(key.begin(),key.end());
        if(!WritePrivateProfileStringW(L"Enabled",id.c_str(),enabled?L"1":L"0",preferences.c_str())){
            error="Could not save the character patch preference.";
            return false;
        }
    }
    if(enabled)enabledPacks.fetch_or(1ull<<pack);
    else enabledPacks.fetch_and(~(1ull<<pack));
    return true;
}
const CharacterAddon* characterAddon(int target){
    return target>=firstAddonCharacter&&size_t(target-firstAddonCharacter)<addons.size()?&addons[target-firstAddonCharacter]:nullptr;
}
bool characterAddonEnabled(int target){
    const auto* addon=characterAddon(target);
    return addon&&characterAddonPackEnabled(addon->pack);
}
std::vector<int> enabledAddonTargets(){
    std::vector<int> targets;
    if(!enabledPacks.load())return targets;
    targets.reserve(addonOrder.size());
    for(const auto target:addonOrder)
        if(characterAddonEnabled(target))targets.push_back(target);
    return targets;
}
int enabledAddonTargetForAppearance(const char* appearance){
    const auto enabled=enabledPacks.load();
    if(!enabled)return -1;
    for(const auto target:addonOrder){
        const auto& addon=*characterAddon(target);
        if((enabled&(1ull<<addon.pack))&&_stricmp(appearance,addon.appearance.c_str())==0)return target;
    }
    return -1;
}
bool addonUnderwearHidden(int target){
    return characterAddon(target)&&(underwearMask.load()&(1u<<(target-firstAddonCharacter)))!=0;
}
void recordAddonVenusBody(int target,bool hasBaseUnderwear){
    if(!characterAddon(target))return;
    const auto bit=1u<<(target-firstAddonCharacter);
    if(hasBaseUnderwear)venusUnderwear.fetch_or(bit);
    venusBodies.fetch_or(bit);
}
bool addonVenusBodyActive(int target){
    return characterAddon(target)&&(venusBodies.load()&(1u<<(target-firstAddonCharacter)))!=0;
}
bool addonVenusUnderwearPresent(int target){
    return characterAddon(target)&&(venusUnderwear.load()&(1u<<(target-firstAddonCharacter)))!=0;
}
bool saveAddonUnderwearHidden(int target,bool hidden,std::string& error){
    const auto* addon=characterAddon(target);
    if(!addon||!characterAddonEnabled(target)){error="Enable this character patch first.";return false;}
    const std::wstring key(addon->id.begin(),addon->id.end());
    if(!preferences.empty()&&!WritePrivateProfileStringW(L"HideBaseUnderwear",key.c_str(),
        hidden?L"1":L"0",preferences.c_str())){
        error="Could not save the character's underwear setting.";
        return false;
    }
    const auto bit=1u<<(target-firstAddonCharacter);
    if(hidden)underwearMask.fetch_or(bit);
    else underwearMask.fetch_and(~bit);
    ++Runtime::get().underwearVersion;
    return true;
}
AddonBodyProfile addonBodyProfile(int target){
    std::lock_guard<std::mutex> lock(profileMutex);
    return characterAddon(target)?profiles[target-firstAddonCharacter]:AddonBodyProfile{};
}
bool saveAddonBodyProfile(int target,const Profile& profile,std::string& error){
    const auto* addon=characterAddon(target);
    if(!addon||!characterAddonEnabled(target)||!profile.valid()||!profile.autoFit){
        error="Choose an enabled character and a valid shape.";
        return false;
    }
    std::lock_guard<std::mutex> lock(profileMutex);
    auto& current=profiles[target-firstAddonCharacter];
    if(current.profile==profile)return true;
    const std::wstring key(addon->id.begin(),addon->id.end());
    std::wostringstream value;
    value.imbue(std::locale::classic());
    value<<std::setprecision(std::numeric_limits<float>::max_digits10)<<unsigned(profile.top)<<L' '<<unsigned(profile.bottom)
         <<L' '<<profile.topBlend<<L' '<<profile.bottomBlend<<L' '<<profile.height<<L' '<<profile.weight;
    if(!preferences.empty()&&!WritePrivateProfileStringW(L"Shape",key.c_str(),value.str().c_str(),preferences.c_str())){
        error="Could not save the character's shape.";
        return false;
    }
    current={profile,current.revision+1};
    shapeDetails[target-firstAddonCharacter].clear();
    return true;
}
void reportAddonShape(int target,uint64_t revision,std::string detail){
    if(!characterAddon(target))return;
    std::lock_guard<std::mutex> lock(profileMutex);
    const auto index=target-firstAddonCharacter;
    if(profiles[index].revision==revision)shapeDetails[index]=std::move(detail);
}
std::string addonShapeDetail(int target){
    if(!characterAddon(target))return {};
    std::lock_guard<std::mutex> lock(profileMutex);
    return shapeDetails[target-firstAddonCharacter];
}
int addonTargetForRecord(std::string_view record){
    const auto it=std::find_if(addons.begin(),addons.end(),[&](const auto& entry){return entry.record==record;});
    return it==addons.end()?-1:firstAddonCharacter+int(std::distance(addons.begin(),it));
}
}
