#include "character_addons.h"
#include "model.h"
#include "wardrobe_snapshot.h"
#include "mounted_dependencies.h"
#include "runtime.h"
#include "core/log.h"
#include "record_flags.h"
#include "release_version.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <mutex>
#include <sstream>
#include <unordered_map>
#include <windows.h>

namespace hex {
namespace {
constexpr size_t maxPackSize=32*1024;
constexpr size_t maxPacks=characterCapacity;
constexpr std::string_view dmmCharacterPrefix="character/hexappeal/characters/";
constexpr std::string_view packStart="<HexAppealCharacters version=\"1\"><![CDATA[",packEnd="]]></HexAppealCharacters>";
std::vector<CharacterAddon> addons;
std::vector<CharacterAddonPack> packs;
std::vector<int> addonOrder;
RecordFlags<maxPacks> enabledPacks;
RecordFlags<maximumAddonCharacters> underwearMask,venusBodies,venusUnderwear;
std::map<std::string,int,std::less<>> recordTargets;
struct AppearanceOrder {
    bool operator()(const std::string& left,const std::string& right) const noexcept {
        return _stricmp(left.c_str(),right.c_str())<0;
    }
    bool operator()(const std::string& left,const char* right) const noexcept {
        return _stricmp(left.c_str(),right)<0;
    }
    bool operator()(const char* left,const std::string& right) const noexcept {
        return _stricmp(left,right.c_str())<0;
    }
    using is_transparent=void;
};
std::map<std::string,std::vector<int>,AppearanceOrder> appearanceTargets;
std::filesystem::path preferences;
std::mutex profileMutex;
std::array<AddonBodyProfile,maximumAddonCharacters> profiles;
std::array<std::string,maximumAddonCharacters> shapeDetails;
// Records that share one character's appearance form a group led by the main entry.
// A group shares its settings unless its bit here asks for separate settings per record.
std::vector<size_t> owners;
RecordFlags<maximumAddonCharacters> separateGroups;
// Every saved shape gets a new revision, so switching a record between shared and
// separate settings always counts as a change for the shaping runtime.
uint64_t profileRevisions=0;
size_t settingsIndex(size_t index){
    const auto owner=owners[index];
    return separateGroups.test(owner)?index:owner;
}

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
              std::vector<CharacterAddon>& result,std::string& error,std::vector<std::string>& messages){
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
    int multiCharacter=-1;
    bool hasDefaultEnabled=false;
    for(;std::getline(in,line);){
        ++lineNumber;
        if(mounted&&line==packEnd){
            in>>std::ws;
            closed=in.eof()&&!in.bad();
            break;
        }
        if(line.empty()||(line.front()=='#'&&line.rfind("#patch\t",0)!=0&&line.rfind("#variant\t",0)!=0&&
           line!="#multiCharacter"&&line.rfind("#multiCharacter\t",0)!=0&&
           line!="#defaultEnabled"&&line.rfind("#defaultEnabled\t",0)!=0))continue;
        const auto row=fields(line);
        error="invalid entry on line "+std::to_string(lineNumber);
        if(row[0]=="#patch"){
            Version version{},minimum{};
            if(row.size()!=4||!result.empty()||!pack.version.empty()||!label(row[1])||
               !parseVersion(row[2],version)||!parseVersion(row[3],minimum))return false;
            pack.label=row[1];pack.version=row[2];pack.minimumVersion=row[3];
            continue;
        }
        if(row[0]=="#multiCharacter"){
            if(row.size()!=2||multiCharacter>=0||!result.empty()||(row[1]!="0"&&row[1]!="1")){
                error="use #multiCharacter followed by one tab and 0 or 1 before the character entries";
                return false;
            }
            multiCharacter=row[1]=="1"?1:0;
            continue;
        }
        if(row[0]=="#defaultEnabled"){
            if(row.size()!=2||hasDefaultEnabled||!result.empty()||(row[1]!="0"&&row[1]!="1")){
                messages.push_back(pack.id+": ignored #defaultEnabled on line "+std::to_string(lineNumber)+
                    ". Use it once before the character entries, followed by one tab and 0 or 1.");
            }else{
                pack.defaultEnabled=row[1]=="1";
                hasDefaultEnabled=true;
            }
            continue;
        }
        if(row[0]=="character"||row[0]=="#variant"){
            const bool variant=row[0]=="#variant";
            if(row.size()!=6||result.size()>=maximumAddonCharacters||!identifier(row[1],64)||!label(row[2])||
               !identifier(row[4],95,true)||!(variant?identifier(row[5],64):appearance(row[5])))return false;
            uint32_t key=0;
            const auto parsed=std::from_chars(row[3].data(),row[3].data()+row[3].size(),key);
            if(parsed.ec!=std::errc{}||parsed.ptr!=row[3].data()+row[3].size()||!key)return false;
            CharacterAddon entry;
            entry.id=row[1];entry.label=row[2];entry.recordKey=key;entry.record=row[4];
            if(variant){
                const auto parent=std::find_if(result.begin(),result.end(),[&](const auto& candidate){
                    return candidate.id==row[5]&&candidate.appearanceOwner==candidate.id;
                });
                if(parent==result.end())return false;
                entry.appearance=parent->appearance;
                entry.appearanceOwner=parent->id;
                parent->requiresRecord=true;
                entry.requiresRecord=true;
            }else{
                entry.appearance=row[5];
                entry.appearanceOwner=entry.id;
            }
            if(matchesBuiltIn(entry)){error="line "+std::to_string(lineNumber)+" overlaps a built-in witch";return false;}
            result.push_back(std::move(entry));
            continue;
        }
        if(row.size()!=4||result.empty()||(row[0]!="default"&&row[0]!="item")||!label(row[2])||
           !validOutfitPart(row[3])||protectedOutfitPart(row[3]))return false;
        const auto slot=outfitSlotForPart(row[3]);
        if(slot.empty())return false;
        const auto owner=std::find_if(result.begin(),result.end(),[&](const auto& entry){return entry.id==row[1];});
        if(owner==result.end()||owner->appearanceOwner!=owner->id||owner->items.size()>=64||std::any_of(owner->items.begin(),owner->items.end(),
            [&](const auto& item){return item.prefab==row[3];}))return false;
        if(row[0]=="default"){
            if(outfitSlotParts(owner->defaults).size()>=24)return false;
            owner->defaults[slot].insert(row[3]);
        }
        owner->items.push_back({row[2],slot,row[3]});
    }
    if(in.bad()||!closed||result.empty()){error="the character list is incomplete or unreadable";return false;}
    const auto roots=std::count_if(result.begin(),result.end(),[](const auto& entry){return entry.id==entry.appearanceOwner;});
    const bool hasVariants=size_t(roots)!=result.size();
    if(multiCharacter==1&&hasVariants){error="#multiCharacter 1 uses separate character rows. Use 0 for #variant rows.";return false;}
    if(multiCharacter==0&&roots!=1){error="#multiCharacter 0 uses one character row and optional #variant rows.";return false;}
    pack.multiCharacter=multiCharacter==1||(multiCharacter<0&&!hasVariants&&roots>1);
    for(auto& entry:result){
        if(entry.appearanceOwner==entry.id)continue;
        const auto parent=std::find_if(result.begin(),result.end(),[&](const auto& candidate){
            return candidate.id==entry.appearanceOwner&&candidate.appearanceOwner==candidate.id;
        });
        entry.defaults=parent->defaults;
        entry.items=parent->items;
    }
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
    std::sort(files.begin(),files.end(),[](const auto& left,const auto& right){return left.name<right.name;});
    if(files.size()>maxPacks){
        files.resize(maxPacks);
        result.messages.push_back("Skipped character manifests after the first "+std::to_string(maxPacks)+" in filename order.");
    }
    std::vector<CharacterAddon> all;
    Version core{};
    const bool knownCore=parseVersion(coreVersion.substr(0,coreVersion.find('-')),core);
    for(const auto& file:files){
        const size_t index=result.packs.size();
        auto& pack=result.packs.emplace_back();
        pack.id=file.name.substr(0,file.name.size()-4);
        std::vector<CharacterAddon> loaded;
        std::string error;
        if(!readPack(file.path,file.mounted,pack,loaded,error,result.messages)){
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
    struct AppearanceOwner {const CharacterAddon* first=nullptr;bool conflict=false;};
    std::unordered_map<std::string_view,AppearanceOwner> appearances;
    std::unordered_map<std::string_view,size_t> ids,recordNames;
    std::unordered_map<uint32_t,size_t> recordKeys;
    for(const auto& entry:all){
        ++ids[entry.id];
        ++recordNames[entry.record];
        ++recordKeys[entry.recordKey];
        auto& owner=appearances[entry.appearance];
        if(!owner.first)owner.first=&entry;
        else if(owner.first->pack!=entry.pack||owner.first->appearanceOwner!=entry.appearanceOwner)owner.conflict=true;
    }
    for(const auto& entry:all){
        auto& pack=result.packs[entry.pack];
        const bool conflict=ids.at(entry.id)>1||recordKeys.at(entry.recordKey)>1||recordNames.at(entry.record)>1||
            appearances.at(entry.appearance).conflict;
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
        recordTargets.clear();
        appearanceTargets.clear();
        for(size_t i=0;i<addons.size();++i){
            const auto target=firstAddonCharacter+int(i);
            addonOrder.push_back(target);
            recordTargets.emplace(addons[i].record,target);
            appearanceTargets[addons[i].appearance].push_back(target);
        }
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
        enabledPacks.clear();
        for(size_t i=0;i<packs.size();++i){
            const auto& pack=packs[i];
            if(!pack.accepted)continue;
            const std::wstring id(pack.id.begin(),pack.id.end());
            const bool enabled=preferences.empty()?pack.defaultEnabled:
                GetPrivateProfileIntW(L"Enabled",id.c_str(),pack.defaultEnabled,preferences.c_str())!=0;
            enabledPacks.exchange(i,enabled);
        }
        owners.assign(addons.size(),0);
        for(size_t i=0;i<addons.size();++i){
            owners[i]=i;
            for(size_t j=0;j<addons.size();++j)
                if(addons[j].id==addons[i].appearanceOwner){owners[i]=j;break;}
        }
        underwearMask.clear();
        separateGroups.clear();
        profiles={};
        shapeDetails={};
        for(size_t i=0;i<addons.size()&&!preferences.empty();++i){
            const auto& key=addons[i].id;
            const std::wstring id(key.begin(),key.end());
            if(GetPrivateProfileIntW(L"HideBaseUnderwear",id.c_str(),0,preferences.c_str()))underwearMask.exchange(i,true);
            if(addons[i].requiresRecord&&owners[i]==i&&!GetPrivateProfileIntW(L"ShareVariants",id.c_str(),1,preferences.c_str()))
                separateGroups.exchange(i,true);
            wchar_t value[192]{};
            if(GetPrivateProfileStringW(L"Shape",id.c_str(),L"",value,192,preferences.c_str()))
                profiles[i]={readProfile(value),++profileRevisions};
        }
        venusBodies.clear();
        venusUnderwear.clear();
        for(const auto& entry:addons)LOG("Character add-on: %s (%s), record %u, appearance %s.",
            entry.label.c_str(),entry.id.c_str(),entry.recordKey,entry.appearance.c_str());
    }catch(const std::exception& error){LOG_ERR("Character add-ons could not be loaded: %s",error.what());}
}
const std::vector<CharacterAddon>& characterAddons(){return addons;}
const std::vector<CharacterAddonPack>& characterAddonPacks(){return packs;}
bool characterAddonPackEnabled(size_t pack){
    return pack<packs.size()&&enabledPacks.test(pack);
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
    enabledPacks.exchange(pack,enabled);
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
    if(!enabledPacks.any())return targets;
    targets.reserve(addonOrder.size());
    for(const auto target:addonOrder)
        if(characterAddonEnabled(target))targets.push_back(target);
    return targets;
}
int enabledAddonTargetForAppearance(const char* appearance){
    const auto candidates=appearanceTargets.find(appearance);
    if(candidates==appearanceTargets.end())return -1;
    int match=-1;
    for(const auto target:candidates->second){
        const auto& addon=*characterAddon(target);
        if(!characterAddonPackEnabled(addon.pack))continue;
        if(match>=0||addon.requiresRecord)return -1;
        match=target;
    }
    return match;
}
bool addonUnderwearHidden(int target){
    return characterAddon(target)&&underwearMask.test(settingsIndex(size_t(target-firstAddonCharacter)));
}
int addonGroupOwner(int target){
    return characterAddon(target)?firstAddonCharacter+int(owners[size_t(target-firstAddonCharacter)]):-1;
}
std::vector<int> addonGroupTargets(int owner){
    std::vector<int> targets;
    if(addonGroupOwner(owner)!=owner)return targets;
    targets.push_back(owner);
    for(size_t i=0;i<addons.size();++i)
        if(owners[i]==size_t(owner-firstAddonCharacter)&&firstAddonCharacter+int(i)!=owner)targets.push_back(firstAddonCharacter+int(i));
    return targets;
}
bool addonGroupShared(int owner){
    return addonGroupOwner(owner)==owner&&owner>=firstAddonCharacter&&!separateGroups.test(size_t(owner-firstAddonCharacter));
}
int addonSettingsTarget(int target){
    return characterAddon(target)?firstAddonCharacter+int(settingsIndex(size_t(target-firstAddonCharacter))):target;
}
std::vector<int> addonSettingsGroup(int target){
    const int owner=addonGroupOwner(target);
    if(owner<0)return {};
    return addonGroupShared(owner)?addonGroupTargets(owner):std::vector<int>{target};
}
std::vector<int> rosterAddonTargets(){
    auto targets=enabledAddonTargets();
    targets.erase(std::remove_if(targets.begin(),targets.end(),[](int target){return addonGroupOwner(target)!=target;}),targets.end());
    return targets;
}
std::vector<int> enabledAddonSettingsTargets(){
    auto targets=enabledAddonTargets();
    targets.erase(std::remove_if(targets.begin(),targets.end(),[](int target){return addonSettingsTarget(target)!=target;}),targets.end());
    return targets;
}
bool setAddonGroupShared(int owner,bool shared,std::string& error){
    const auto members=addonGroupTargets(owner);
    if(members.size()<2||!characterAddonEnabled(owner)){error="This character has no other game records.";return false;}
    const auto* main=characterAddon(owner);
    const std::wstring ownerKey(main->id.begin(),main->id.end());
    if(!preferences.empty()&&!WritePrivateProfileStringW(L"ShareVariants",ownerKey.c_str(),shared?L"1":L"0",preferences.c_str())){
        error="Could not save the shared settings preference.";
        return false;
    }
    const auto ownerIndex=size_t(owner-firstAddonCharacter);
    separateGroups.exchange(ownerIndex,!shared);
    if(!shared){
        // Each record starts from the shared look unless it already has its own saved settings.
        for(const int target:members){
            if(target==owner)continue;
            const auto* addon=characterAddon(target);
            const std::wstring key(addon->id.begin(),addon->id.end());
            wchar_t existing[192]{};
            const bool ownShape=!preferences.empty()&&GetPrivateProfileStringW(L"Shape",key.c_str(),L"",existing,192,preferences.c_str());
            AddonBodyProfile ownerProfile;
            {std::lock_guard<std::mutex> lock(profileMutex);ownerProfile=profiles[ownerIndex];}
            std::string ignored;
            if(!ownShape&&ownerProfile.revision)saveAddonBodyProfile(target,ownerProfile.profile,ignored);
            const bool ownUnderwear=!preferences.empty()&&GetPrivateProfileIntW(L"HideBaseUnderwear",key.c_str(),-1,preferences.c_str())!=-1;
            if(!ownUnderwear&&underwearMask.test(ownerIndex))saveAddonUnderwearHidden(target,true,ignored);
        }
    }
    {
        std::lock_guard<std::mutex> lock(profileMutex);
        for(const int target:members){
            auto& profile=profiles[size_t(target-firstAddonCharacter)];
            if(profile.revision)profile.revision=++profileRevisions;
            shapeDetails[size_t(target-firstAddonCharacter)].clear();
        }
    }
    ++Runtime::get().underwearVersion;
    LOG("Character add-on: %s %s its settings across %zu game records.",main->label.c_str(),shared?"shares":"no longer shares",members.size());
    return true;
}
void recordAddonVenusBody(int target,bool hasBaseUnderwear){
    if(!characterAddon(target))return;
    const auto index=size_t(target-firstAddonCharacter);
    if(hasBaseUnderwear)venusUnderwear.exchange(index,true);
    venusBodies.exchange(index,true);
}
bool addonVenusBodyActive(int target){
    return characterAddon(target)&&venusBodies.test(size_t(target-firstAddonCharacter));
}
bool addonVenusUnderwearPresent(int target){
    return characterAddon(target)&&venusUnderwear.test(size_t(target-firstAddonCharacter));
}
bool saveAddonUnderwearHidden(int target,bool hidden,std::string& error){
    if(!characterAddon(target)||!characterAddonEnabled(target)){error="Enable this character patch first.";return false;}
    target=addonSettingsTarget(target);
    const auto* addon=characterAddon(target);
    const std::wstring key(addon->id.begin(),addon->id.end());
    if(!preferences.empty()&&!WritePrivateProfileStringW(L"HideBaseUnderwear",key.c_str(),
        hidden?L"1":L"0",preferences.c_str())){
        error="Could not save the character's underwear setting.";
        return false;
    }
    underwearMask.exchange(size_t(target-firstAddonCharacter),hidden);
    ++Runtime::get().underwearVersion;
    return true;
}
AddonBodyProfile addonBodyProfile(int target){
    std::lock_guard<std::mutex> lock(profileMutex);
    return characterAddon(target)?profiles[settingsIndex(size_t(target-firstAddonCharacter))]:AddonBodyProfile{};
}
bool saveAddonBodyProfile(int target,const Profile& profile,std::string& error){
    if(!characterAddon(target)||!characterAddonEnabled(target)||!profile.valid()||!profile.autoFit){
        error="Choose an enabled character and a valid shape.";
        return false;
    }
    const auto group=addonSettingsGroup(target);
    target=addonSettingsTarget(target);
    const auto* addon=characterAddon(target);
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
    current={profile,++profileRevisions};
    for(const int member:group)shapeDetails[size_t(member-firstAddonCharacter)].clear();
    return true;
}
void reportAddonShape(int target,uint64_t revision,std::string detail){
    if(!characterAddon(target))return;
    std::lock_guard<std::mutex> lock(profileMutex);
    const auto index=size_t(target-firstAddonCharacter);
    if(profiles[settingsIndex(index)].revision==revision)shapeDetails[index]=std::move(detail);
}
std::string addonShapeDetail(int target){
    if(!characterAddon(target))return {};
    const auto group=addonSettingsGroup(target);
    std::lock_guard<std::mutex> lock(profileMutex);
    // Shared settings report whichever record is currently shaped, starting with the main entry.
    for(const int member:group)
        if(!shapeDetails[size_t(member-firstAddonCharacter)].empty())return shapeDetails[size_t(member-firstAddonCharacter)];
    return shapeDetails[size_t(target-firstAddonCharacter)];
}
int addonTargetForRecord(std::string_view record){
    const auto it=recordTargets.find(record);
    return it==recordTargets.end()?-1:it->second;
}
}
