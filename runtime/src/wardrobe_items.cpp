#include "wardrobe_items.h"
#include <Windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <algorithm>
#include <charconv>
#include <fstream>
#include <sstream>

namespace hex {
namespace {
std::string folded(std::string_view value){
    std::string result(value);
    for(auto& ch:result){if(ch>='A'&&ch<='Z')ch+=char('a'-'A');else if(ch=='_'||ch=='-')ch=' ';}
    return result;
}
std::vector<std::string> words(std::string_view query){
    std::istringstream stream(folded(query));
    std::vector<std::string> found;
    for(std::string word;stream>>word;)found.push_back(std::move(word));
    return found;
}
std::string_view slotNoun(std::string_view slot){
    constexpr std::pair<std::string_view,std::string_view> nouns[]={
        {"Upper body","top"},{"Lower body","bottoms"},{"Headwear","headwear"},{"Hands","gloves"},
        {"Feet","boots"},{"Cloak","cloak"},{"Accessories","accessory"},{"Shoulders","shoulder piece"},
        {"Waist","belt"},{"Armour","armour"}};
    for(const auto& [name,noun]:nouns)if(name==slot)return noun;
    return {};
}
}
std::string unlistedPartName(std::string_view part,std::string_view slot,
                             const std::vector<std::string>& wearers,const std::string& copyOf){
    // The part ID stays in the name: it is what search and fit reports use.
    const std::string suffix=" ("+std::string(part)+")";
    if(!copyOf.empty())return copyOf+" (witch copy)";
    const auto noun=slotNoun(slot);
    if(wearers.empty()||noun.empty())return std::string(slot)+suffix;
    if(wearers.size()==1)return wearers.front()+"'s default "+std::string(noun)+suffix;
    std::string names;
    for(size_t i=0;i<wearers.size();++i)
        names+=(i==0?"":i+1==wearers.size()?" and ":", ")+wearers[i];
    return "Default "+std::string(noun)+" of "+names+suffix;
}
std::vector<uint32_t> wardrobeIconPixels(IWICBitmapDecoder& decoder,IWICImagingFactory& factory){
    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    Microsoft::WRL::ComPtr<IWICBitmapScaler> scaler;
    Microsoft::WRL::ComPtr<IWICFormatConverter> convert;
    UINT width=0,height=0;
    if(FAILED(decoder.GetFrame(0,frame.GetAddressOf()))||FAILED(frame->GetSize(&width,&height))||
       !width||!height||width>2048||height>2048||
       FAILED(factory.CreateBitmapScaler(scaler.GetAddressOf()))||
       FAILED(scaler->Initialize(frame.Get(),wardrobeIconSize,wardrobeIconSize,WICBitmapInterpolationModeFant))||
       FAILED(factory.CreateFormatConverter(convert.GetAddressOf()))||
       FAILED(convert->Initialize(scaler.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom)))return {};
    constexpr auto count=wardrobeIconSize*wardrobeIconSize;
    std::vector<uint32_t> pixels(count);
    if(FAILED(convert->CopyPixels(nullptr,wardrobeIconSize*4,count*4,reinterpret_cast<BYTE*>(pixels.data())))||
       std::none_of(pixels.begin(),pixels.end(),[](auto pixel){return pixel>>24;}))return {};
    return pixels;
}
bool WardrobeItemCatalog::load(const std::filesystem::path& root){
    const auto index=root/L"items.tsv";
    std::error_code ec;
    if(std::filesystem::file_size(index,ec)>4*1024*1024||ec)return false;
    std::ifstream stream(index,std::ios::binary);
    std::string line;
    if(!std::getline(stream,line))return false;
    if(!line.empty()&&line.back()=='\r')line.pop_back();
    if(line!="HexAppealItems 1")return false;
    WardrobeItemCatalog next;
    std::set<std::pair<uint32_t,std::string>> keys;
    while(std::getline(stream,line)){
        if(next.items.size()>=4096)break;
        if(!line.empty()&&line.back()=='\r')line.pop_back();
        if(line.size()>1500)continue;
        std::istringstream fields(line);
        std::string id,parts;
        WardrobeItem item;
        if(!std::getline(fields,id,'\t')||!std::getline(fields,item.name,'\t')||!std::getline(fields,item.slot,'\t')||
           !std::getline(fields,item.tags,'\t')||!std::getline(fields,parts)||parts.find('\t')!=std::string::npos)continue;
        const auto parsed=std::from_chars(id.data(),id.data()+id.size(),item.id);
        if(parsed.ec!=std::errc{}||parsed.ptr!=id.data()+id.size()||!item.id||
           item.name.empty()||item.name.size()>200||!validOutfitSlot(item.slot))continue;
        std::istringstream prefabs(parts);
        bool valid=true;
        for(std::string part;std::getline(prefabs,part,',');){
            if(!validOutfitPart(part)||protectedOutfitPart(part)||item.parts.size()>=8){valid=false;break;}
            item.parts.insert(std::move(part));
        }
        if(!valid||item.parts.empty()||!keys.emplace(item.id,item.slot).second)continue;
        const auto iconPath=root/L"icons"/(std::to_wstring(item.id)+L".rgba");
        constexpr auto bytes=wardrobeIconSize*wardrobeIconSize*sizeof(uint32_t);
        if(std::filesystem::file_size(iconPath,ec)==bytes&&!ec){
            std::ifstream icon(iconPath,std::ios::binary);
            item.icon.resize(wardrobeIconSize*wardrobeIconSize);
            if(!icon.read(reinterpret_cast<char*>(item.icon.data()),bytes))item.icon.clear();
        }
        for(const auto& part:item.parts)next.byPart.try_emplace(part,next.items.size());
        next.items.push_back(std::move(item));
    }
    if(next.items.empty())return false;
    *this=std::move(next);
    return true;
}
const WardrobeItem* WardrobeItemCatalog::forPart(std::string_view part) const{
    const auto found=byPart.find(part);
    return found==byPart.end()?nullptr:&items[found->second];
}
void WardrobeItemCatalog::indexParts(){
    byPart.clear();
    for(size_t i=0;i<items.size();++i)
        for(const auto& part:items[i].parts)byPart.try_emplace(part,i);
}
bool WardrobeItemCatalog::hasMissingIcons() const{
    return std::any_of(items.begin(),items.end(),[](const auto& item){return item.icon.empty();});
}
void WardrobeItemCatalog::fillMissingIcons(const WardrobeItemCatalog& stock){
    for(auto& item:items){
        if(!item.icon.empty()||item.parts.empty())continue;
        const auto* original=stock.forPart(*item.parts.begin());
        if(original&&original->id==item.id&&original->slot==item.slot&&original->parts==item.parts)
            item.icon=original->icon;
    }
}
void WardrobeItemCatalog::supplement(const WardrobeItemCatalog& local){
    std::set<std::pair<uint32_t,std::string>> known;
    for(const auto& item:items)known.emplace(item.id,item.slot);
    for(const auto& item:local.items)
        if(known.emplace(item.id,item.slot).second)items.push_back(item);
    fillMissingIcons(local);
    indexParts();
}
void WardrobeItemCatalog::retainMounted(const WardrobeItemCatalog& previous){
    for(const auto& old:previous.items){
        if(old.source.empty())continue;
        auto match=std::find_if(items.begin(),items.end(),[&](const auto& item){
            return item.slot==old.slot&&(old.id?item.id==old.id:item.parts==old.parts);
        });
        if(match==items.end())items.push_back(old);
        else if(match->source.empty())*match=old;
    }
    indexParts();
}
std::vector<const WardrobeItem*> WardrobeItemCatalog::find(std::string_view query,std::string_view slot,size_t limit,bool requireIcon) const{
    const auto terms=words(query);
    const auto exact=folded(query);
    std::vector<const WardrobeItem*> result;
    result.reserve(std::min(limit,items.size()));
    for(const auto& item:items){
        if(result.size()==limit)break;
        if((!slot.empty()&&item.slot!=slot)||(requireIcon&&item.icon.empty()))continue;
        if(terms.empty()){
            result.push_back(&item);
            continue;
        }
        const auto id=item.id?std::to_string(item.id):std::string{};
        auto searchable=" "+folded(item.name+" "+item.slot+" "+item.tags+" "+item.source)+" ";
        bool exactPart=false;
        for(const auto& part:item.parts){
            auto name=folded(part);
            exactPart=exactPart||name==exact;
            searchable+=name;
            searchable+=' ';
        }
        if(exactPart||std::all_of(terms.begin(),terms.end(),[&](const auto& term){
            const auto digits=term.find_first_not_of("0123456789")==std::string::npos;
            if(!digits)return searchable.find(term)!=std::string::npos;
            if(id==term)return true;
            // Versioned mod names and prefab paths can contain independent numeric terms.
            return terms.size()>1&&searchable.find(" "+term+" ")!=std::string::npos;
        }))result.push_back(&item);
    }
    return result;
}
}
