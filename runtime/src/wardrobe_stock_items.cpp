#include "wardrobe_items.h"
#include "game_archive.h"
#include "mounted_dependencies.h"
#include "winrt_runtime.h"
#include <wincodec.h>
#include <wrl/client.h>
#include <algorithm>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <unordered_map>

namespace hex {
namespace {
using Microsoft::WRL::ComPtr;
using std::string_view;

uint32_t u32(string_view bytes,size_t offset){
    uint32_t value=0;
    std::memcpy(&value,bytes.data()+offset,sizeof(value));
    return value;
}
bool digit(char value){return value>='0'&&value<='9';}
std::string lower(string_view value){
    std::string result(value);
    for(auto& ch:result)if(ch>='A'&&ch<='Z')ch+=char('a'-'A');
    return result;
}
struct StaticInfoRecord {uint32_t id;string_view data;string_view name;bool indexed;};
std::vector<StaticInfoRecord> records(string_view header,string_view body){
    if(header.size()<2||body.empty())return {};
    const auto count=static_cast<unsigned char>(header[0])+size_t(static_cast<unsigned char>(header[1]))*256;
    if(!count)return {};
    std::vector<StaticInfoRecord> result;
    result.reserve(count);
    if(header.size()!=2+count*8){
        // 2.06 puts the resource name and an explicit body span in each
        // header entry. The body starts with the ID and ordinal.
        size_t at=2,end=0;
        for(size_t i=0;i<count;++i){
            if(header.size()-at<16)return {};
            const auto id=u32(header,at),nameSize=u32(header,at+4);
            if(nameSize>header.size()-at-16)return {};
            const auto name=header.substr(at+8,nameSize);
            at+=8+nameSize;
            const auto offset=u32(header,at),size=u32(header,at+4);
            at+=8;
            if(offset!=end||size<8||size>body.size()-end||u32(body,offset)!=id||u32(body,offset+4)!=i)return {};
            result.push_back({id,body.substr(offset,size),name,true});
            end+=size;
        }
        if(at!=header.size()||end!=body.size())return {};
        return result;
    }
    std::vector<std::pair<uint32_t,uint32_t>> offsets;
    offsets.reserve(count);
    for(size_t offset=2;offset<header.size();offset+=8)
        offsets.emplace_back(u32(header,offset+4),u32(header,offset));
    std::sort(offsets.begin(),offsets.end());
    if(offsets.front().first!=0||offsets.back().first>=body.size())return {};
    for(size_t i=0;i<offsets.size();++i){
        const auto [start,id]=offsets[i];
        const auto end=i+1<offsets.size()?size_t(offsets[i+1].first):body.size();
        if(end<=start)return {};
        result.push_back({id,body.substr(start,end-start),{},false});
    }
    return result;
}
std::unordered_map<uint32_t,std::string> itemIcons(string_view header,string_view body){
    std::unordered_map<uint32_t,std::string> result;
    for(const auto& [id,record,name,indexed]:records(header,body)){
        constexpr size_t bufferOffset=13;
        if(record.size()<=bufferOffset||record.size()-bufferOffset>191||
           u32(record,indexed?0:4)!=id||u32(record,9)!=record.size()-bufferOffset)continue;
        auto ref=lower(record.substr(bufferOffset));
        if(ref.compare(0,9,"itemicon_")==0&&validOutfitPart(ref))result.try_emplace(id,std::move(ref));
    }
    return result;
}
bool label(string_view value){
    return !value.empty()&&value.size()<=200&&value.find_first_of("<>\r\n\t") == value.npos&&
        std::none_of(value.begin(),value.end(),[](unsigned char c){return c<32;})&&
        MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),int(value.size()),nullptr,0)>0;
}
std::unordered_map<string_view,string_view> itemLabels(string_view bytes){
    std::unordered_map<string_view,string_view> result;
    for(size_t at=4;at+8<=bytes.size();){
        const auto keySize=u32(bytes,at);
        if(keySize<9||keySize>20||at+8+keySize>bytes.size()){++at;continue;}
        const auto key=bytes.substr(at+4,keySize);
        if(!std::all_of(key.begin(),key.end(),digit)){++at;continue;}
        const auto textSize=u32(bytes,at+4+keySize);
        const auto start=at+8+keySize;
        if(textSize>bytes.size()-start){++at;continue;}
        const auto value=bytes.substr(start,textSize);
        if(label(value))result.try_emplace(key,value);
        at=start+textSize;
    }
    return result;
}
string_view nameFor(string_view record,size_t start,const std::unordered_map<string_view,string_view>& names){
    const auto fields=record.substr(start,202);
    for(size_t at=0;at<fields.size();){
        if(!digit(fields[at])){++at;continue;}
        const auto first=at;
        while(at<fields.size()&&digit(fields[at]))++at;
        if(at-first<9||at-first>20)continue;
        const auto found=names.find(fields.substr(first,at-first));
        if(found!=names.end())return found->second;
    }
    return {};
}
std::vector<uint32_t> stockIcon(std::string data,IWICImagingFactory& factory){
    if(data.size()<128||data.compare(0,4,"DDS ")!=0)return {};
    ComPtr<IWICStream> stream;
    ComPtr<IWICBitmapDecoder> decoder;
    if(FAILED(factory.CreateStream(stream.GetAddressOf()))||
       FAILED(stream->InitializeFromMemory(reinterpret_cast<BYTE*>(data.data()),DWORD(data.size())))||
       FAILED(factory.CreateDecoderFromStream(stream.Get(),nullptr,WICDecodeMetadataCacheOnDemand,decoder.GetAddressOf())))return {};
    return wardrobeIconPixels(*decoder.Get(),factory);
}
}

bool WardrobeItemCatalog::loadStock(const std::filesystem::path& game,const std::filesystem::path& manifest) try{
    namespace fs=std::filesystem;
    const auto mounted=manifest.empty()?std::map<std::string,fs::path>{}:readMountedRoutes(manifest);
    constexpr auto modelPrefix="character/bin__/prefab/1_pc/02_phw/armor";
    const auto open=[&](const fs::path& folder,const char* prefix)->std::optional<GameArchive>{
        try{return std::optional<GameArchive>(std::in_place,folder,prefix);}
        catch(const std::runtime_error&){return {};}
        catch(const std::out_of_range&){return {};}
    };
    const auto read=[](const std::optional<GameArchive>& archive,const std::string& route){
        if(!archive)return std::string{};
        try{return archive->read(route);}
        catch(const std::runtime_error&){return std::string{};}
        catch(const std::out_of_range&){return std::string{};}
    };
    const auto baseModels=open(game/L"0009",modelPrefix);
    const auto baseIcons=open(game/L"0012","ui/texture/icon");
    const auto baseStrings=open(game/L"0020","gamedata/stringtable/binary__/eng");
    const auto baseTables=open(game/L"0008","gamedata/binarystaticinfo__/bin");
    std::unordered_map<uint32_t,std::string> prefabs;
    const auto recordPrefab=[&](string_view path,string_view suffix){
        if(path.size()<=suffix.size()||path.substr(path.size()-suffix.size())!=suffix)return;
        const auto file=path.substr(path.find_last_of('/')+1);
        const auto part=file.substr(0,file.size()-suffix.size());
        if(!validOutfitPart(part)||protectedOutfitPart(part)||outfitSlotForPart(part).empty())return;
        auto [entry,added]=prefabs.emplace(gameArchiveNameHash(part),part);
        if(!added&&entry->second!=part)entry->second.clear();
    };
    if(baseModels)for(const auto path:baseModels->routes())recordPrefab(path,".prefab");
    for(const auto& [path,source]:mounted){
        std::error_code ec;
        if((path.find("/02_phw/armor/")!=path.npos||path.find("/2_phw/armor/")!=path.npos)&&
            fs::is_regular_file(source,ec)){
            recordPrefab(path,".prefab");
            recordPrefab(path,".pac");
        }
    }

    const WinrtApartment apartment;
    ComPtr<IWICImagingFactory> factory;
    if(apartment)CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(factory.GetAddressOf()));
    WardrobeItemCatalog next;
    std::map<std::pair<uint32_t,std::string>,size_t> itemIndexes;
    const std::string baseItem="gamedata/binarystaticinfo__/bin/iteminfo";
    const std::string baseName="gamedata/stringtable/binary__/eng/item";
    const std::string baseString="gamedata/binarystaticinfo__/bin/stringinfo";
    const auto active=[&](const std::string& route){
        const auto found=mounted.find(route);
        return found==mounted.end()?std::string{}:readLooseGameAsset(found->second);
    };

    const auto activeHeader=active(baseString+".staticinfoheader"),activeBody=active(baseString+".staticinfobody");
    auto baseIconKeys=itemIcons(activeHeader,activeBody);
    for(auto& [id,icon]:itemIcons(read(baseTables,baseString+".staticinfoheader"),read(baseTables,baseString+".staticinfobody")))
        baseIconKeys.try_emplace(id,std::move(icon));
    const auto collect=[&](const std::optional<GameArchive>& tables,const std::optional<GameArchive>& localization,
                           const std::optional<GameArchive>& icons,const std::string& suffix){
        const auto localizationRoute=baseName+suffix+".paloc";
        const auto mountedLabels=active(localizationRoute);
        const auto stockLabels=read(localization,localizationRoute);
        const auto baseLabels=suffix.empty()?std::string{}:read(baseStrings,baseName+".paloc");
        auto names=itemLabels(mountedLabels);
        for(const auto* backup:{&stockLabels,&baseLabels})
            for(const auto& [key,value]:itemLabels(*backup))names.try_emplace(key,value);
        const auto route=baseItem+suffix+".staticinfo";
        const auto stockHeader=read(tables,route+"header"),stockBody=read(tables,route+"body");
        const auto activeHeader=active(route+"header"),activeBody=active(route+"body");
        auto entries=records(stockHeader,stockBody);
        const auto mountedEntries=records(activeHeader.empty()?stockHeader:activeHeader,
                                          activeBody.empty()?stockBody:activeBody);
        // Read stock first so verified active records can replace individual stock items.
        if(!activeHeader.empty()||!activeBody.empty())
            entries.insert(entries.end(),mountedEntries.begin(),mountedEntries.end());
        const auto iconRoute=baseString+suffix+".staticinfo";
        auto iconKeys=baseIconKeys;
        if(!suffix.empty())for(const auto& [id,icon]:itemIcons(read(tables,iconRoute+"header"),read(tables,iconRoute+"body")))
            iconKeys.insert_or_assign(id,icon);
        const auto readIcon=[&](const std::string& name){
            if(!factory)return std::vector<uint32_t>{};
            const auto route="ui/texture/icon/"+name+".dds";
            auto result=stockIcon(active(route),*factory.Get());
            if(result.empty())result=stockIcon(read(icons,route),*factory.Get());
            if(result.empty()&&!suffix.empty())result=stockIcon(read(baseIcons,route),*factory.Get());
            return result;
        };
        for(const auto& [id,record,headerName,indexed]:entries){
            if(!id||record.size()<8)continue;
            const size_t length=indexed?0:u32(record,0);
            if(!indexed&&(length>200||length+8>=record.size()||u32(record,length+4)!=id))continue;
            const size_t bodyStart=indexed?0:length+4;
            const auto display=nameFor(record,bodyStart+4,names);

            OutfitSlots parts;
            const std::string* iconName=nullptr;
            bool severalIcons=false;
            for(size_t at=bodyStart+16;at+4<=record.size();++at){
                const auto key=u32(record,at);
                const auto part=prefabs.find(key);
                if(part!=prefabs.end()&&!part->second.empty())
                    parts[outfitSlotForPart(part->second)].insert(part->second);
                const auto icon=iconKeys.find(key);
                if(icon==iconKeys.end())continue;
                if(iconName&&*iconName!=icon->second)severalIcons=true;
                iconName=&icon->second;
            }
            if(parts.empty())continue;
            WardrobeItem item;
            item.id=id;
            const auto internal=indexed?headerName:record.substr(4,length);
            item.name=display.empty()?"Equipment #"+std::to_string(id):std::string(display);
            if(iconName&&!severalIcons)item.icon=readIcon(*iconName);
            const auto stem=lower(internal);
            if(item.icon.empty()&&validOutfitPart(stem))item.icon=readIcon("itemicon_"+stem);
            for(auto& [slot,members]:parts){
                if(members.size()>8)continue;
                auto row=item;
                row.slot=slot;
                row.parts=std::move(members);
                row.tags=internal;
                if(slot=="Feet")row.tags+=" boot footwear shoes";
                if(slot=="Headwear")row.tags+=" helmet hat headgear";
                if(slot=="Upper body")row.tags+=" armour armor chest coat dress";
                if(slot=="Lower body")row.tags+=" pants trousers skirt";
                if(slot=="Cloak")row.tags+=" cape cloak";
                for(const auto& part:row.parts){
                    if(!row.icon.empty())break;
                    row.icon=readIcon("itemicon_prefab_"+part);
                }
                const auto [index,added]=itemIndexes.try_emplace({id,slot},next.items.size());
                if(added)next.items.push_back(std::move(row));
                else next.items[index->second]=std::move(row);
            }
        }
    };
    collect(baseTables,baseStrings,baseIcons,{});
    std::error_code ec;
    for(fs::directory_iterator it(game/L"dlc",fs::directory_options::skip_permission_denied,ec),end;!ec&&it!=end;it.increment(ec))try{
        const auto folder=it->path();
        const auto name=lower(folder.filename().u8string());
        if(name.rfind("dlc_",0)!=0||!validOutfitPart(name)||!it->is_directory(ec))continue;
        const auto tables=open(folder/L"0008","gamedata/binarystaticinfo__/bin");
        const auto strings=open(folder/L"0020","gamedata/stringtable/binary__/eng");
        const auto icons=open(folder/L"0012","ui/texture/icon");
        const auto models=open(folder/L"0009",modelPrefix);
        if(models)for(auto path:models->routes())recordPrefab(path,".prefab");
        collect(tables,strings,icons,"."+name);
    }catch(const std::runtime_error&){}catch(const std::out_of_range&){}
    if(next.empty())return false;
    std::sort(next.items.begin(),next.items.end(),[](const auto& a,const auto& b){return a.name<b.name;});
    next.indexParts();
    *this=std::move(next);
    return true;
}catch(const std::runtime_error&){return false;}
 catch(const std::out_of_range&){return false;}
}
