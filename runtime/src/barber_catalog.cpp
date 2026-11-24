#include "barber_catalog.h"
#include "wardrobe_snapshot.h"
#include <Windows.h>
#include <Shlwapi.h>
#include <XmlLite.h>
#include <wrl/client.h>
#include <string_view>

namespace hex {
namespace {
std::string slotKey(size_t index) {
    return std::string("Barber")+(index<10?"0":"")+std::to_string(index);
}
std::wstring attribute(IXmlReader* reader,const wchar_t* name){
    if(reader->MoveToAttributeByName(name,nullptr)!=S_OK)return {};
    const wchar_t* value=nullptr;
    UINT length=0;
    std::wstring result;
    if(reader->GetValue(&value,&length)==S_OK&&value)result.assign(value,length);
    reader->MoveToElement();
    return result;
}
int indexOf(const std::wstring& value,int limit){
    if(value.empty()||value.size()>3)return -1;
    int number=0;
    for(auto digit:value){if(digit<L'0'||digit>L'9')return -1;number=number*10+digit-L'0';}
    return number<limit?number:-1;
}
std::string prefabName(const std::wstring& value){
    if(value.empty()||value.size()>=192)return {};
    std::string result;
    for(const auto ch:value){
        if(!((ch>=L'a'&&ch<=L'z')||(ch>=L'A'&&ch<=L'Z')||(ch>=L'0'&&ch<=L'9')||ch==L'_'||ch==L'-'||ch==L'.'))return {};
        result+=char(ch);
    }
    return result;
}
}
OutfitSlots readDefaultWardrobeSlots(const std::filesystem::path& path,int witch){
    std::error_code error;
    if(path.empty()||std::filesystem::file_size(path,error)>2*1024*1024||error)return {};
    Microsoft::WRL::ComPtr<IStream> source;
    if(FAILED(SHCreateStreamOnFileEx(path.c_str(),STGM_READ|STGM_SHARE_DENY_NONE,0,FALSE,nullptr,source.GetAddressOf())))return {};
    Microsoft::WRL::ComPtr<IXmlReader> parser;
    if(FAILED(CreateXmlReader(__uuidof(IXmlReader),reinterpret_cast<void**>(parser.GetAddressOf()),nullptr))||
       FAILED(parser->SetProperty(XmlReaderProperty_DtdProcessing,DtdProcessing_Prohibit))||FAILED(parser->SetInput(source.Get())))return {};
    OutfitSlots result;
    XmlNodeType type;
    HRESULT step;
    bool inArmour=false,garment=false;
    while((step=parser->Read(&type))==S_OK){
        if(type!=XmlNodeType_Element)continue;
        UINT depth=0,length=0;
        const wchar_t* value=nullptr;
        if(FAILED(parser->GetDepth(&depth))||FAILED(parser->GetLocalName(&value,&length)))return {};
        const std::wstring_view tag(value,length);
        if(depth==0&&tag!=L"Appearance")return {};
        if(depth==1)inArmour=tag==L"Armor";
        if(depth!=2||!inArmour||tag!=L"Prefab")continue;
        auto part=prefabName(attribute(parser.Get(),L"Name"));
        if(part.empty()||protectedOutfitPart(part)||!defaultOutfitPart(witch,part.c_str()))continue;
        garment|=wardrobePartKind(witch,part.c_str())==WardrobePartKind::DefaultGarment;
        auto slot=outfitSlotForPart(part);
        result[slot.empty()?"Armour":slot].insert(std::move(part));
    }
    if(step!=S_FALSE||!garment)return {};
    if(witch==0){
        const auto feet=result.find("Feet");
        if(feet!=result.end()&&feet->second.count("cd_phw_00_foot_00_0185"))
            feet->second.erase("cd_phw_00_foot_0092");
    }
    return result;
}
bool BarberCatalog::load(const std::filesystem::path& path){
    std::error_code error;
    if(path.empty()||std::filesystem::file_size(path,error)>2*1024*1024||error)return false;
    Microsoft::WRL::ComPtr<IStream> source;
    if(FAILED(SHCreateStreamOnFileEx(path.c_str(),STGM_READ|STGM_SHARE_DENY_NONE,0,FALSE,nullptr,source.GetAddressOf())))return false;
    Microsoft::WRL::ComPtr<IXmlReader> parser;
    if(FAILED(CreateXmlReader(__uuidof(IXmlReader),reinterpret_cast<void**>(parser.GetAddressOf()),nullptr))||
       FAILED(parser->SetProperty(XmlReaderProperty_DtdProcessing,DtdProcessing_Prohibit))||FAILED(parser->SetInput(source.Get())))return false;

    BarberCatalog next;
    XmlNodeType type;
    HRESULT step=S_OK;
    int slot=-1,option=-1;
    bool root=false;
    while((step=parser->Read(&type))==S_OK){
        if(type!=XmlNodeType_Element)continue;
        UINT depth=0,length=0;
        const wchar_t* value=nullptr;
        if(FAILED(parser->GetDepth(&depth))||FAILED(parser->GetLocalName(&value,&length)))return false;
        const std::wstring_view tag(value,length);
        if(depth==0){if(tag!=L"MeshParam")return false;root=true;}
        if(depth==1){
            slot=-1;option=-1;
            if(tag!=L"ParamDesc"||attribute(parser.Get(),L"Category")!=L"Armor")continue;
            slot=indexOf(attribute(parser.Get(),L"Index"),int(next.slots.size()));
            if(slot<0||next.slots[slot].armor)return false;
            next.slots[slot].armor=true;
        }
        if(depth==2&&slot>=0){
            option=-1;
            if(tag!=L"MeshSet")continue;
            option=indexOf(attribute(parser.Get(),L"Index"),255);
            if(option<0||!next.slots[slot].options.try_emplace(uint8_t(option)).second)return false;
        }
        if(depth==3&&tag==L"MeshList"&&slot>=0&&option>=0){
            auto prefab=prefabName(attribute(parser.Get(),L"MeshFileName"));
            if(prefab.empty())return false;
            next.slots[slot].options.at(uint8_t(option)).insert(prefab);
            next.prefabs.insert(std::move(prefab));
        }
    }
    if(step!=S_FALSE||!root||next.prefabs.empty())return false;
    *this=std::move(next);
    return true;
}
bool BarberCatalog::has(const std::string& prefab) const{return prefabs.count(prefab)!=0;}
bool BarberCatalog::isArmorSlot(size_t slot) const {
    return slot<slots.size()&&slots[slot].armor;
}
bool BarberCatalog::matchesIdentity(const std::array<uint8_t,16>& first,const std::array<uint8_t,16>& second) const {
    bool specific=false;
    for(size_t i=0;i<slots.size();++i){
        if(isArmorSlot(i))continue;
        if(first[i]!=second[i])return false;
        specific|=first[i]!=255;
    }
    return specific;
}
std::string BarberCatalog::slotFor(const std::string& prefab) const {
    if(!has(prefab))return {};
    for(size_t i=0;i<slots.size();++i)
        for(const auto& option:slots[i].options)
            if(option.second.count(prefab))return slotKey(i);
    return {};
}
bool BarberCatalog::resolveSlots(const std::array<uint8_t,16>& choices,OutfitSlots& clothing) const{
    OutfitSlots result;
    size_t numPieces=0;
    for(size_t i=0;i<slots.size();++i){
        const auto& slot=slots[i];
        if(!slot.armor||choices[i]==255)continue;
        const auto found=slot.options.find(choices[i]);
        if(found==slot.options.end())return false;
        if((numPieces+=found->second.size())>24)return false;
        if(!found->second.empty())result.emplace(slotKey(i),found->second);
    }
    clothing=std::move(result);
    return true;
}
bool BarberCatalog::resolve(const std::array<uint8_t,16>& choices,std::set<std::string>& clothing) const {
    OutfitSlots slots;
    if(!resolveSlots(choices,slots))return false;
    clothing=outfitSlotParts(slots);
    return true;
}
OutfitParts BarberCatalog::combatFallbacks(std::array<uint8_t,16> choices) const {
    for(auto& choice:choices)if(!choice)choice=255;
    OutfitParts parts;
    resolve(choices,parts);
    return parts;
}
}
