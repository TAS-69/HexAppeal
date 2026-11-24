#include "wardrobe_items.h"
#include "game_archive.h"
#include "mounted_dependencies.h"
#include <Windows.h>
#include <roapi.h>
#include "winrt_runtime.h"
#include <wincodec.h>
#include <wrl/client.h>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstring>
#include <fstream>

namespace hex {
namespace {
using namespace winrt::Windows::Data::Json;
using Microsoft::WRL::ComPtr;
namespace fs=std::filesystem;

JsonObject readObject(const fs::path& path,uintmax_t limit)try{
    std::error_code ec;
    const auto size=fs::file_size(path,ec);
    if(ec||!size||size>limit)return nullptr;
    std::string contents(size,'\0');
    std::ifstream file(path,std::ios::binary);
    if(!file.read(contents.data(),contents.size()))return nullptr;
    if(contents.compare(0,3,"\xef\xbb\xbf")==0)contents.erase(0,3);
    JsonObject object(nullptr);
    return JsonObject::TryParse(winrt::to_hstring(contents),object)?object:nullptr;
}catch(const winrt::hresult_error&){return nullptr;}
std::string lower(std::string value){
    for(auto& c:value)if(c>='A'&&c<='Z')c+=char('a'-'A');
    return value;
}
bool word(std::string_view text,std::string_view term){
    const auto delimited=[](char c){return !(c>='a'&&c<='z')&&!(c>='0'&&c<='9');};
    for(auto at=text.find(term);at!=text.npos;at=text.find(term,at+term.size()))
        if((at==0||delimited(text[at-1]))&&(at+term.size()==text.size()||delimited(text[at+term.size()])))return true;
    return false;
}
std::string declaredBody(std::string text){
    text=lower(std::move(text));
    const auto venus=word(text,"venus"),vanilla=word(text,"vanilla");
    if(venus==vanilla)return {};
    return venus?"venus":"vanilla";
}
std::string packageTitle(const fs::path& package){
    for(const auto* file:{L"mod.json",L"modinfo.json"})try{
        const auto data=readObject(package/file,512*1024);
        if(!data)continue;
        const auto info=data.GetNamedObject(L"modinfo",data);
        const auto title=winrt::to_string(info.GetNamedString(L"title",L""));
        if(!title.empty()&&title.size()<=200&&title.find_first_of("\t\r\n")==title.npos)return title;
    }catch(const winrt::hresult_error&){}
    return package.filename().u8string();
}
std::vector<uint32_t> readIcon(const fs::path& path,IWICImagingFactory& factory){
    std::error_code ec;
    if(fs::file_size(path,ec)>16*1024*1024||ec)return {};
    ComPtr<IWICBitmapDecoder> decoder;
    if(FAILED(factory.CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnDemand,decoder.GetAddressOf())))return {};
    return wardrobeIconPixels(*decoder.Get(),factory);
}
JsonObject patches(const JsonObject& rule){
    JsonObject result;
    const auto values=rule.GetNamedArray(L"patches",nullptr);
    if(values)for(const auto& item:values)try{
        const auto field=item.GetObject();
        auto key=field.GetNamedString(L"path",L"");
        if(key.empty())key=field.GetNamedString(L"field",L"");
        if(!key.empty()&&field.HasKey(L"new"))result.SetNamedValue(key,field.GetNamedValue(L"new"));
    }catch(const winrt::hresult_error&){}
    return result;
}
uint32_t itemKey(double value){
    if(!(value>0&&value<=UINT32_MAX))return 0;
    const auto key=static_cast<uint32_t>(value);
    return key==value?key:0;
}
uint32_t itemKey(const IJsonValue& value){
    if(!value)return 0;
    if(value.ValueType()==JsonValueType::Number)return itemKey(value.GetNumber());
    if(value.ValueType()!=JsonValueType::String)return 0;
    const auto text=winrt::to_string(value.GetString());
    uint32_t key=0;
    const auto number=std::from_chars(text.data(),text.data()+text.size(),key);
    return number.ec==std::errc{}&&number.ptr==text.data()+text.size()?key:0;
}
std::wstring localizationKey(double value){
    if(value<=0||value>9007199254740991.0)return {};
    const auto key=static_cast<uint64_t>(value);
    return key==value?std::to_wstring(key):std::wstring{};
}
struct MountedGear {
    std::set<fs::path> packages;
    std::map<std::string,fs::path> icons;
    std::map<std::string,std::set<fs::path>,std::less<>> parts;
    std::map<std::string,std::set<fs::path>,std::less<>> models;
    std::map<std::string,fs::path,std::less<>> prefabs;
    fs::path registry;
};
std::vector<fs::path> activePackages(const fs::path& source){
    std::vector<fs::path> packages;
    for(auto parent=source.parent_path();!parent.empty()&&parent!=parent.parent_path();parent=parent.parent_path()){
        const auto container=lower(parent.parent_path().filename().u8string());
        const auto leaf=lower(parent.filename().u8string());
        const bool archive=leaf.size()==4&&leaf.find_first_not_of("0123456789")==leaf.npos;
        if((leaf=="files"&&container!="dmm")||(archive&&container!="files"))
            packages.push_back(parent.parent_path());
        if(container=="mods"){
            if(packages.empty()||packages.back()!=parent)packages.push_back(parent);
            return packages;
        }
        if(container=="dmm"&&leaf=="files")return {};
    }
    return packages;
}
std::map<std::string,fs::path,std::less<>> stagedSources(const fs::path& manifest,
        const std::set<fs::path>& roots){
    std::map<std::string,fs::path,std::less<>> result;
    std::error_code ec;
    const auto mounted=fs::last_write_time(manifest,ec);
    if(ec)return result;
    for(const auto& root:roots){
        const auto log=root.parent_path()/L"mount_log.txt";
        const auto updated=fs::last_write_time(log,ec);
        if(ec||updated<mounted-std::chrono::seconds(90)||updated>mounted+std::chrono::seconds(90)||
           fs::file_size(log,ec)>16*1024*1024||ec)continue;
        std::ifstream in(log);
        for(std::string line;std::getline(in,line);)try{
            constexpr std::string_view prefix="[INJECT] ",owner=", from=";
            if(line.rfind(prefix,0)!=0||line.size()>4096||line.back()!=')')continue;
            const auto routeEnd=line.find(" (runtime,",prefix.size());
            const auto from=line.rfind(owner);
            if(routeEnd==line.npos||from==line.npos||from<routeEnd)continue;
            const auto start=from+owner.size();
            const auto package=fs::u8path(line.substr(start,line.size()-start-1));
            if(package.empty()||package.has_root_path()||
               std::any_of(package.begin(),package.end(),[](const auto& part){return part==L".."||part==L".";}))continue;
            const auto folder=root/package;
            if(!fs::is_directory(folder,ec))continue;
            const auto route=lower(line.substr(prefix.size(),routeEnd-prefix.size()));
            auto [entry,added]=result.try_emplace(route,folder);
            if(!added&&entry->second!=folder)entry->second.clear();
        }catch(const fs::filesystem_error&){}
    }
    return result;
}
MountedGear mountFiles(const fs::path& path,bool& supported){
    MountedGear result;
    std::error_code ec;
    if(fs::file_size(path,ec)>64*1024*1024||ec)return result;
    const auto routes=readMountedRoutes(path,&supported);
    std::set<fs::path> roots;
    for(const auto& [name,source]:routes)try{
        const auto packages=activePackages(source);
        if(!packages.empty()&&lower(packages.back().parent_path().filename().u8string())=="mods")
            roots.insert(packages.back().parent_path());
    }catch(const fs::filesystem_error&){}
    const auto staged=stagedSources(path,roots);
    for(const auto& [name,source]:routes)try{
        const fs::path virtualFile(name);
        const auto stem=virtualFile.stem().u8string();
        const bool female=name.find("/02_phw/")!=name.npos||name.find("/2_phw/")!=name.npos||
            stem.find("_phw_")!=stem.npos;
        const bool accessory=female&&(virtualFile.extension()==L".prefab"||virtualFile.extension()==L".pac"||
            virtualFile.extension()==L".pac_xml")&&
            validOutfitPart(stem)&&!protectedOutfitPart(stem)&&!outfitSlotForPart(stem).empty();
        const bool icon=name.compare(0,16,"ui/texture/icon/")==0&&virtualFile.extension()==L".dds";
        if(name=="character/bin__/partprefabtable.pappt")result.registry=source;
        auto packages=activePackages(source);
        if(packages.empty()){
            const auto owner=staged.find(name);
            const auto relative=source.lexically_relative(path.parent_path()/L"files");
            if(owner!=staged.end()&&!owner->second.empty()&&lower(relative.generic_u8string())==name)
                packages=activePackages(owner->second/L"files"/L"_");
        }
        if(accessory||icon){
            if(!fs::is_regular_file(source,ec))continue;
            if(icon)result.icons.try_emplace(stem,source);
            if(accessory){
                auto& origins=result.parts[stem];
                if(virtualFile.extension()==L".prefab")result.prefabs.emplace(name,source);
                if(!packages.empty())origins.insert(packages.front());
                if(virtualFile.extension()==L".pac"){
                    auto& models=result.models[name];
                    models.insert(origins.begin(),origins.end());
                }
            }
        }
        result.packages.insert(packages.begin(),packages.end());
    }catch(const fs::filesystem_error&){}
    return result;
}
void bindModels(MountedGear& mount,const fs::path& game){
    if(mount.models.empty())return;
    const auto record=[&](std::string_view route,std::string data){
        const auto filename=route.substr(route.find_last_of('/')+1);
        if(filename.size()<=7||filename.substr(filename.size()-7)!=".prefab")return;
        const auto part=filename.substr(0,filename.size()-7);
        if(!validOutfitPart(part)||protectedOutfitPart(part)||outfitSlotForPart(part).empty())return;
        data=lower(std::move(data));
        constexpr std::string_view prefix="character/model/";
        for(size_t at=data.find(prefix);at!=data.npos;at=data.find(prefix,at+prefix.size())){
            const auto end=std::min(data.size(),data.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_-./",at));
            if(end-at>256)continue;
            const auto model=mount.models.find(std::string_view(data).substr(at,end-at));
            if(model!=mount.models.end())
                mount.parts[std::string(part)].insert(model->second.begin(),model->second.end());
        }
    };
    const auto load=[&](const fs::path& root){
        try{
            const GameArchive archive(root/L"0009","character/bin__/prefab/1_pc/02_phw/armor");
            for(const auto route:archive.routes()){
                if(mount.prefabs.count(route)||route.size()<=7||route.substr(route.size()-7)!=".prefab")continue;
                try{record(route,archive.read(std::string(route)));}catch(const std::runtime_error&){}catch(const std::out_of_range&){}
            }
        }catch(const std::runtime_error&){}
    };
    for(const auto& [route,source]:mount.prefabs)record(route,readLooseGameAsset(source));
    if(game.empty())return;
    load(game);
    std::error_code ec;
    for(fs::directory_iterator it(game/L"dlc",fs::directory_options::skip_permission_denied,ec),end;!ec&&it!=end;it.increment(ec))
        if(it->is_directory(ec)&&lower(it->path().filename().u8string()).rfind("dlc_",0)==0)load(it->path());
}
std::set<std::string> registeredGear(const MountedGear& mount) try{
    if(mount.registry.empty())return {};
    const auto bytes=readLooseGameAsset(mount.registry);
    size_t offset=0;
    const auto take=[&](size_t size){
        if(size>bytes.size()-offset)throw std::runtime_error("Truncated prefab table");
        const auto result=std::string_view(bytes).substr(offset,size);
        offset+=size;
        return result;
    };
    const auto byte=[&]{return static_cast<uint8_t>(take(1)[0]);};
    const auto count=[&]{
        uint32_t result;
        std::memcpy(&result,take(4).data(),4);
        if(result>100000)throw std::runtime_error("Invalid prefab count");
        return result;
    };
    const auto name=[&]{
        const auto value=take(byte());
        if(value.empty()||value.find('\0')!=value.size()-1)throw std::runtime_error("Invalid prefab name");
        return value.substr(0,value.size()-1);
    };
    take(8);
    std::map<std::string,std::string> primary;
    for(auto rows=count();rows;--rows){
        const auto part=name(),directory=name();
        if(mount.parts.count(part)){
            auto [entry,added]=primary.try_emplace(std::string(part),directory);
            if(!added)entry->second.clear();
        }
        name();byte();name();byte();
        for(auto children=byte();children;--children){name();byte();}
    }
    std::set<std::string> result;
    for(const auto& [part,directory]:primary)
        if(!directory.empty())result.insert(part);
    std::set<std::string> seen;
    for(auto rows=count();rows;--rows){
        const auto part=name(),directory=name();
        const auto entry=primary.find(std::string(part));
        if(entry==primary.end())continue;
        if(!seen.insert(entry->first).second||directory!=entry->second)result.erase(entry->first);
    }
    return offset==bytes.size()?result:std::set<std::string>{};
}catch(const std::runtime_error&){return {};}
std::string itemName(const JsonObject& data,const std::map<std::wstring,std::string>& names){
    const auto value=data.GetNamedValue(L"item_name",nullptr);
    std::string name;
    if(value&&value.ValueType()==JsonValueType::String)name=winrt::to_string(value.GetString());
    else{
        const auto key=value&&value.ValueType()==JsonValueType::Object?value.GetObject():data;
        const auto index=value?L"index":L"item_name.index";
        const auto fallback=value?L"default":L"item_name.default";
        const auto entry=key.GetNamedValue(index,nullptr);
        const auto label=key.GetNamedString(fallback,L"");
        auto localized=names.find(std::wstring(label));
        if(localized==names.end()&&entry){
            if(entry.ValueType()==JsonValueType::Number)localized=names.find(localizationKey(entry.GetNumber()));
            if(entry.ValueType()==JsonValueType::String)localized=names.find(std::wstring(entry.GetString()));
        }
        name=localized==names.end()?winrt::to_string(label):localized->second;
    }
    if(name.empty()||name.size()>200||name.find_first_of("\t\r\n")!=name.npos||
       name.find_first_not_of("0123456789 ")==name.npos)return {};
    return name;
}
struct RuleGroup {
    std::map<uint32_t,std::string> strings;
    std::map<std::wstring,std::string> names;
    std::vector<std::pair<winrt::hstring,JsonArray>> clones;
};
void addRule(RuleGroup& group,const JsonObject& rule){
    const auto info=rule.GetNamedObject(L"modinfo",nullptr);
    const auto title=info?info.GetNamedString(L"title",L""):winrt::hstring{};
    const auto targets=rule.GetNamedArray(L"targets",nullptr);
    if(!targets)return;
    for(const auto& value:targets)try{
        const auto target=value.GetObject();
        const auto path=fs::path(target.GetNamedString(L"file",L"").c_str());
        const auto kind=lower(path.filename().u8string());
        const auto intents=target.GetNamedArray(L"intents",nullptr);
        if(!intents)continue;
        if(kind=="iteminfo.pabgb"&&!title.empty())group.clones.emplace_back(title,intents);
        if(kind=="stringinfo.pabgb")for(const auto& entry:intents)try{
            const auto item=entry.GetObject();
            const auto id=itemKey(item.GetNamedValue(L"new_key",nullptr));
            auto buffer=patches(item).GetNamedString(L"buffer",L"");
            const auto record=item.GetNamedObject(L"template",nullptr);
            if(buffer.empty()&&record)buffer=record.GetNamedString(L"buffer",L"");
            if(id&&!buffer.empty()){
                const auto text=winrt::to_string(buffer);
                auto [data,inserted]=group.strings.try_emplace(id,text);
                if(!inserted&&data->second!=text)data->second.clear();
            }
        }catch(const winrt::hresult_error&){}
        if(kind=="localizationstring_eng.paloc"||kind=="item.paloc")for(const auto& entry:intents)try{
            const auto item=entry.GetObject();
            group.names.try_emplace(std::wstring(item.GetNamedString(L"entry",L"")),winrt::to_string(item.GetNamedString(L"new",L"")));
        }catch(const winrt::hresult_error&){}
    }catch(const winrt::hresult_error&){}
}
std::string partName(const IJsonValue& value,const RuleGroup& group){
    if(const auto id=itemKey(value)){
        const auto found=group.strings.find(id);
        return found==group.strings.end()?std::string{}:lower(found->second);
    }
    if(value&&value.ValueType()==JsonValueType::String)return lower(winrt::to_string(value.GetString()));
    return {};
}
std::vector<WardrobeItem> itemsFromRules(const RuleGroup& rules,const JsonObject& registered,const MountedGear& mount,
                                      const std::vector<WardrobeItem>& native,IWICImagingFactory* factory){
    std::vector<WardrobeItem> result;
    for(const auto& [title,group]:rules.clones)for(const auto& value:group)try{
        const auto row=value.GetObject();
        const auto id=itemKey(row.GetNamedValue(L"new_key",nullptr));
        const auto owner=registered.GetNamedString(std::to_wstring(id),L"");
        if(!id||owner.empty()||row.GetNamedString(L"op",L"")!=L"clone_record"||
            lower(winrt::to_string(owner))!=lower(winrt::to_string(title)))continue;
        const auto data=patches(row);
        const auto looks=data.GetNamedArray(L"prefab_data_list",nullptr);
        auto name=itemName(data,rules.names);
        if(name.empty())continue;
        WardrobeItem item;
        item.id=id;
        item.name=std::move(name);
        item.source=winrt::to_string(title);
        OutfitSlots slots;
        const auto addParts=[&](const JsonArray& parts){
            if(!parts)return;
            for(const auto& key:parts){
                auto part=partName(key,rules);
                if(part.size()>7&&part.substr(part.size()-7)==".prefab")
                    part=fs::u8path(part).stem().u8string();
                if(!mount.parts.count(part)||protectedOutfitPart(part))continue;
                auto slot=outfitSlotForPart(part);
                if(slot.empty())continue;
                slots[slot].insert(std::move(part));
            }
        };
        if(looks)for(const auto& look:looks){
            const auto prefab=look.GetObject();
            if(prefab.GetNamedNumber(L"prefab_data_type",0)==0)
                addParts(prefab.GetNamedArray(L"prefab_names",nullptr));
        }
        bool overrides=bool(looks);
        for(unsigned i=0;i<32;++i){
            const auto field=L"prefab_data_list["+std::to_wstring(i)+L"]";
            if(!data.HasKey(field+L".prefab_names"))continue;
            overrides=true;
            if(!data.GetNamedNumber(field+L".prefab_data_type",0))
                addParts(data.GetNamedArray(field+L".prefab_names",nullptr));
        }
        if(!overrides){
            const auto donor=itemKey(row.GetNamedValue(L"source_key",nullptr));
            for(const auto& original:native)if(original.id==donor&&donor){
                slots[original.slot]=original.parts;
                if(item.icon.empty())item.icon=original.icon;
            }
        }
        if(slots.empty())continue;
        const auto addIcon=[&](const IJsonValue& value){
            const auto iconPath=partName(value,rules);
            const auto path=mount.icons.find(lower(fs::u8path(iconPath).stem().u8string()));
            if(path!=mount.icons.end())item.icon=readIcon(path->second,*factory);
        };
        try{
            const auto icons=data.GetNamedArray(L"item_icon_list",nullptr);
            if(icons&&factory)for(const auto& icon:icons)try{
                addIcon(icon.GetObject().GetNamedValue(L"icon_path",nullptr));
                if(!item.icon.empty())break;
            }catch(const winrt::hresult_error&){}
            if(factory)for(unsigned i=0;i<16&&item.icon.empty();++i){
                const auto field=L"item_icon_list["+std::to_wstring(i)+L"].icon_path";
                const auto path=data.GetNamedValue(field,nullptr);
                if(path)addIcon(path);
            }
        }catch(const winrt::hresult_error&){}
        for(auto& [slot,parts]:slots){
            if(parts.size()>8)continue;
            item.slot=slot;
            item.parts=std::move(parts);
            result.push_back(item);
        }
    }catch(const winrt::hresult_error&){}
    return result;
}

struct PackageGear {
    std::string title,body;
};
}

size_t WardrobeItemCatalog::addMounted(const fs::path& bin,std::string_view body,const fs::path& manifest,bool* scanComplete){
    if(scanComplete)*scanComplete=false;
    const WinrtApartment apartment;
    if(!apartment)return 0;
    try{
        const auto path=manifest.empty()?bin/L"dmm/manifest.tsv":manifest;
        const auto registered=readObject(path.parent_path()/L"mod_items.json",1024*1024);
        bool supported=false;
        auto mount=mountFiles(path,supported);
        if(!supported)return 0;
        bindModels(mount,bin.parent_path());
        ComPtr<IWICImagingFactory> factory;
        CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(factory.GetAddressOf()));
        size_t found=0;
        std::map<fs::path,PackageGear> packs;
        std::map<fs::path,RuleGroup> groups;
        for(const auto& package:mount.packages)try{
            const auto chain=activePackages(package/L"files"/L"_");
            const auto scope=chain.empty()?package:chain.back();
            auto title=packageTitle(scope);
            auto fit=declaredBody(package.filename().u8string());
            if(fit.empty())fit=declaredBody(packageTitle(package));
            if(fit.empty())fit=declaredBody(title);
            packs.emplace(package,PackageGear{std::move(title),fit});
            if(!registered||!registered.Size())continue;
            std::error_code ec;
            for(fs::directory_iterator it(package,ec),end;!ec&&it!=end;it.increment(ec))try{
                const auto file=lower(it->path().filename().u8string());
                if(it->path().extension()!=L".json"||file=="mod.json"||file=="modinfo.json"||file=="manifest.json")continue;
                const auto rule=readObject(it->path(),8*1024*1024);
                if(rule)addRule(groups[scope],rule);
            }catch(const winrt::hresult_error&){}
        }catch(const winrt::hresult_error&){}

        const auto fitFor=[&](const std::string& part){
            const auto route=mount.parts.find(part);
            if(route==mount.parts.end())return PackageGear{};
            PackageGear result;
            std::set<std::string> titles,modes;
            for(const auto& origin:route->second){
                const auto pack=packs.find(origin);
                if(pack==packs.end())continue;
                titles.insert(pack->second.title);
                if(!pack->second.body.empty())modes.insert(pack->second.body);
            }
            for(const auto& title:titles){
                if(!result.title.empty())result.title+=" / ";
                result.title+=title;
            }
            if(modes.size()==1)result.body=*modes.begin();
            return result;
        };
        const auto checkFit=[&](WardrobeItem& item){
            for(const auto& part:item.parts){
                const auto pack=fitFor(part);
                if(item.source.empty())item.source=pack.title;
                if(mount.parts.count(part)&&(pack.body.empty()||body.empty()||pack.body!=body))
                    item.fitUnverified=true;
            }
        };
        for(auto& item:items){
            checkFit(item);
            if(!item.source.empty())++found;
        }

        OutfitParts listed;
        for(const auto& item:items)listed.insert(item.parts.begin(),item.parts.end());
        for(const auto& [scope,rules]:groups)for(auto& item:itemsFromRules(rules,registered,mount,items,factory.Get())){
            const auto fit=declaredBody(item.source);
            checkFit(item);
            if(body.empty()||(!fit.empty()&&fit!=body)||(fit.empty()&&std::none_of(item.parts.begin(),item.parts.end(),[&](const auto& part){
                return !fitFor(part).body.empty();
            })))item.fitUnverified=true;
            const auto existing=std::find_if(items.begin(),items.end(),[&](const auto& row){
                return row.id==item.id&&row.slot==item.slot;
            });
            if(existing==items.end())items.push_back(std::move(item));
            else{
                if(item.icon.empty()&&existing->parts==item.parts)item.icon=std::move(existing->icon);
                *existing=std::move(item);
            }
            ++found;
        }
        for(const auto& item:items)listed.insert(item.parts.begin(),item.parts.end());
        auto registeredParts=registeredGear(mount);
        // Mounted prefab routes are also candidates when the registry lives in a stock archive.
        for(const auto& [route,source]:mount.prefabs){
            const auto part=fs::path(route).stem().u8string();
            if(!part.empty())registeredParts.insert(part);
        }
        for(const auto& part:registeredParts){
            if(listed.count(part)||part.find("_index")!=part.npos||part.find("_sub")!=part.npos)continue;
            const auto pack=fitFor(part);
            WardrobeItem item;
            item.slot=outfitSlotForPart(part);
            item.name=item.slot+" ("+part+")";
            item.source=pack.title.empty()?"Mounted equipment":pack.title;
            item.parts.insert(part);
            item.fitUnverified=pack.body.empty()||body.empty()||pack.body!=body;
            const auto icon=mount.icons.find("itemicon_prefab_"+part);
            if(factory&&icon!=mount.icons.end())item.icon=readIcon(icon->second,*factory.Get());
            items.push_back(std::move(item));
            ++found;
        }
        std::stable_partition(items.begin(),items.end(),[](const auto& item){return !item.source.empty();});
        indexParts();
        if(scanComplete)*scanComplete=true;
        return found;
    }catch(const winrt::hresult_error&){return 0;}
}
}
