#include "skin_tone.h"
#include "game_archive.h"
#include "skin_dds.h"
#include "mounted_dependencies.h"
#include <Windows.h>
#include <Shlwapi.h>
#include <XmlLite.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <cstring>
#include <fstream>
#include <set>
#include <stdexcept>
#include <string_view>

namespace hex {
namespace {
namespace fs=std::filesystem;
using Microsoft::WRL::ComPtr;
constexpr auto damianeAppearance="character/appearance/1_pc/2_phw/cd_phw_damian/cd_phw_damian_00000.app_xml";
constexpr auto faceMaterials="character/modelproperty/1_pc/2_phw/head/head/";
constexpr auto stockHead="phw_00_head_00_0111";
constexpr auto stockFace="character/texture/phw_00_head_00_0111.dds";
constexpr auto venusBody="phw_00_nude_00_0001_damian";
constexpr auto venusMaterial="character/modelproperty/1_pc/2_phw/nude/phw_00_nude_00_0001_damian.pac_xml";
constexpr auto torsoSlot="cd_phw_00_nude_0001_damian";
using ColourBindings=std::map<std::string,std::string>;
const ColourBindings stockBodyColours{
    {torsoSlot,"character/texture/phw_00_nude_00_0001.dds"},
    {"cd_phm_00_head_0001_01_damian","character/texture/phw_00_head_00_0001_01.dds"},
    {"cd_phm_00_nude_0001_hand_damian","character/texture/phw_00_nude_00_0001_hand.dds"}};

std::string readAsset(const fs::path& path,size_t limit=1024*1024){
    std::error_code error;
    const auto size=fs::file_size(path,error);
    if(error||!size||size>limit)return {};
    std::string content(static_cast<size_t>(size),'\0');
    std::ifstream file(path,std::ios::binary);
    if(!file.read(content.data(),content.size()))return {};
    return content;
}

std::string attribute(IXmlReader& parser,const wchar_t* name){
    if(parser.MoveToAttributeByName(name,nullptr)!=S_OK)return {};
    const wchar_t* value=nullptr;
    UINT length=0;
    std::string result;
    if(parser.GetValue(&value,&length)==S_OK&&length<=256){
        result.reserve(length);
        for(UINT i=0;i<length;++i){
            const auto ch=value[i];
            if(ch>127){result.clear();break;}
            result+=char(ch);
        }
    }
    parser.MoveToElement();
    return result;
}

template<class Visit>
bool visitXml(const std::string& content,Visit visit){
    if(content.empty())return false;
    ComPtr<IStream> stream;
    stream.Attach(SHCreateMemStream(reinterpret_cast<const BYTE*>(content.data()),static_cast<UINT>(content.size())));
    ComPtr<IXmlReader> parser;
    if(!stream||FAILED(CreateXmlReader(__uuidof(IXmlReader),reinterpret_cast<void**>(parser.GetAddressOf()),nullptr))||
       FAILED(parser->SetProperty(XmlReaderProperty_DtdProcessing,DtdProcessing_Prohibit))||
       FAILED(parser->SetInput(stream.Get())))return false;
    XmlNodeType node;
    HRESULT step;
    while((step=parser->Read(&node))==S_OK){
        if(node!=XmlNodeType_Element)continue;
        UINT depth=0,length=0;
        const wchar_t* name=nullptr;
        if(FAILED(parser->GetDepth(&depth))||FAILED(parser->GetLocalName(&name,&length)))return false;
        if(!visit(*parser.Get(),depth,std::wstring_view(name,length)))return false;
    }
    return step==S_FALSE;
}

void lowerAscii(std::string& value){
    for(auto& ch:value)if(ch>='A'&&ch<='Z')ch+=char('a'-'A');
}

std::string appearancePrefab(const std::string& data,std::wstring_view slot){
    std::string part;
    bool inside=false;
    const bool valid=visitXml(data,[&](IXmlReader& parser,UINT depth,std::wstring_view name){
        if(depth==0)return name==L"Appearance";
        if(depth==1)inside=name==slot;
        if(depth!=2||!inside||name!=L"Prefab")return true;
        if(!part.empty())return false;
        part=attribute(parser,L"Name");
        return !part.empty();
    });
    return valid?part:std::string{};
}

std::string headPrefab(const std::string& data,bool privateHead=false){
    auto head=appearancePrefab(data,L"Head");
    auto normalized=head;
    lowerAscii(normalized);
    return normalized.rfind(privateHead?"hxk_":"phw_",0)==0&&normalized.find("_head_")!=normalized.npos&&
           normalized.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_")==normalized.npos?head:std::string{};
}

ColourBindings skinColours(std::string_view material,bool face){
    if(material.empty())return {};
    if(material.substr(0,3)=="\xef\xbb\xbf")material.remove_prefix(3);
    UINT mesh=0,skinMaterial=0,colourSlot=0;
    ColourBindings bindings;
    std::string nameOfMesh;
    const bool valid=visitXml("<Document>"+std::string(material)+"</Document>",[&](IXmlReader& parser,UINT depth,std::wstring_view name){
        if(skinMaterial&&depth<=skinMaterial)skinMaterial=colourSlot=0;
        if(colourSlot&&depth<=colourSlot)colourSlot=0;
        if(mesh&&depth<=mesh){mesh=0;nameOfMesh.clear();}
        if(name==L"SkinnedMeshMaterialWrapper"){
            mesh=depth;
            nameOfMesh=attribute(parser,L"_subMeshName");
            lowerAscii(nameOfMesh);
        }
        if(name==L"Material"&&(face||mesh)&&attribute(parser,L"_materialName")==
           (face?"SkinnedMeshSkinWrinkle":"SkinnedMeshSkin"))skinMaterial=depth;
        if(skinMaterial&&name==L"MaterialParameterTexture"&&attribute(parser,L"_name")=="_baseColorTexture")colourSlot=depth;
        if(!colourSlot||depth!=colourSlot+1||name!=L"ResourceReferencePath_ITexture")return true;
        const auto route=attribute(parser,L"_path");
        auto normalized=route;
        lowerAscii(normalized);
        return normalized.rfind("character/texture/",0)==0&&normalized.size()>22&&
            normalized.compare(normalized.size()-4,4,".dds")==0&&normalized.find("..")==normalized.npos&&
            normalized.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_./-")==normalized.npos&&
            (face||!nameOfMesh.empty())&&bindings.emplace(nameOfMesh,route).second;
    });
    return valid?bindings:ColourBindings{};
}

std::string faceColour(std::string_view material){
    const auto slots=skinColours(material,true);
    return slots.size()==1?slots.begin()->second:std::string{};
}

using TextureSize=std::array<uint32_t,2>;
TextureSize textureSize(std::string_view data){
    std::array<uint32_t,37> header{};
    if(data.size()<128)return {};
    std::memcpy(header.data(),data.data(),std::min(data.size(),sizeof header));
    if(header[0]!=0x20534444||header[1]!=124||header[19]!=32||
       !header[3]||!header[4]||header[3]>16384||header[4]>16384||header[6]>1||
       (header[28]&0x0020fe00u))return {};
    if(header[21]==0x30315844&&(data.size()<148||header[33]!=3||(header[34]&4u)||header[35]!=1))return {};
    return {header[4],header[3]};
}
std::string textureHeader(const fs::path& path){
    std::ifstream file(path,std::ios::binary);
    std::array<char,149> bytes{};
    file.read(bytes.data(),bytes.size());
    return {bytes.data(),static_cast<size_t>(file.gcount())};
}

constexpr const char* witchAppearances[]={
    "character/appearance/2_mon/cd_m0001_00_twofeet/cd_m0001_00_phw_north/cd_m0001_00_phw_north_44001.app_xml",
    "character/appearance/3_npc/cd_npc_north/cd_nhw_north/cd_nhw_north_20026.app_xml",
    "character/appearance/3_npc/cd_npc_south/cd_nhw_south/cd_nhw_south_20165.app_xml",
    "character/appearance/3_npc/cd_npc_south/cd_nhw_south/cd_nhw_south_20166.app_xml",
    "character/appearance/3_npc/cd_npc_south/cd_nhw_south/cd_nhw_south_20167.app_xml",
    "character/appearance/3_npc/cd_npc_south/cd_nhw_south/cd_nhw_south_20168.app_xml",
    "character/appearance/3_npc/cd_npc_south/cd_nhw_south/cd_nhw_south_20169.app_xml"};
constexpr auto stockHeadDirectory="1_pc/02_phw/head/head";
constexpr auto privateHeadDirectory="ha_p/02_phw/head/head";
constexpr auto customizationRoot="character/descriptors/customization/";

struct ToneField {
    size_t offset=0,length=0;
    unsigned value=0;
};

std::optional<ToneField> toneField(const std::string& data,const char* key,unsigned maximum){
    const auto root=data.find_first_not_of("\xef\xbb\xbf \t\r\n");
    const auto from=data.find("<DecorationParam>");
    const auto until=data.find("</>",from);
    if(root==data.npos||data.compare(root,23,"<CharacterCustomization")||
       from==data.npos||until==data.npos)return {};
    const std::string field=std::string("<Data UIKey=\"")+key+"\" Value=\"";
    const auto fieldAt=data.find(field,from);
    if(fieldAt>=until||data.find(field,fieldAt+field.size())<until)return {};
    const auto start=fieldAt+field.size();
    const auto end=data.find('"',start);
    if(end>=until||end-start>3||data.compare(end,3,"\"/>"))return {};
    unsigned value=0;
    const auto parsed=std::from_chars(data.data()+start,data.data()+end,value);
    if(parsed.ec!=std::errc{}||parsed.ptr!=data.data()+end||value>maximum)return {};
    return ToneField{start,end-start,value};
}

std::string witchCustomization(const std::string& route,const std::string& appearance){
    auto name=fs::path(route).stem().string();
    const bool whiteCrow=name=="cd_m0001_00_phw_north_44001";
    if(name=="cd_nhw_north_20026")name="cd_nhw_south_20092";
    const auto expected=std::string(whiteCrow?"cd_mon/":"cd_npc_south/")+name+"_customization";
    bool found=false;
    const bool valid=visitXml(appearance,[&](IXmlReader& parser,UINT depth,std::wstring_view tag){
        if(depth!=1||tag!=L"Customization")return true;
        if(found||attribute(parser,L"CustomizationFile")!=expected)return false;
        found=true;
        return true;
    });
    return valid&&found?customizationRoot+expected+".paccd_xml":std::string{};
}

std::string addonCustomization(const std::string& appearance){
    std::string value;
    const bool valid=visitXml(appearance,[&](IXmlReader& parser,UINT depth,std::wstring_view tag){
        if(depth!=1||tag!=L"Customization")return true;
        if(!value.empty())return false;
        value=attribute(parser,L"CustomizationFile");
        const auto slash=value.find('/');
        return value.rfind("cd_npc_",0)==0&&slash!=value.npos&&slash+1<value.size()&&
            value.find('/',slash+1)==value.npos&&
            value.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_/")==value.npos;
    });
    return valid?value:std::string{};
}

void applyTone(std::string& data,const char* key,unsigned maximum,const std::optional<unsigned>& value){
    if(!value)return;
    const auto field=toneField(data,key,maximum);
    if(!field)throw std::runtime_error(std::string("No valid target ")+key+" setting.");
    data.replace(field->offset,field->length,std::to_string(*value));
}

bool replaceOnce(std::string& data,const std::string& before,const std::string& after){
    if(before.empty())return false;
    const auto found=data.find(before);
    if(found==data.npos||data.find(before,found+before.size())!=data.npos)return false;
    data.replace(found,before.size(),after);
    return true;
}

struct Registry {
    std::string_view bytes;
    uint32_t primary=0,secondary=0;
    size_t secondaryAt=0;
    std::map<std::string,std::pair<std::string,std::string_view>> parts;
    std::map<std::string,std::string> lookups;
    std::set<std::string> ambiguous;
    static std::string text(const std::string& value){return char(value.size()+1)+value+'\0';}
    explicit Registry(std::string_view raw):bytes(raw){
        size_t at=0;
        const auto take=[&](size_t size){
            if(size>bytes.size()-at)throw std::runtime_error("The head prefab registry is truncated.");
            const auto result=bytes.substr(at,size);
            at+=size;
            return result;
        };
        const auto byte=[&]{return static_cast<uint8_t>(take(1)[0]);};
        const auto count=[&]{
            uint32_t size;
            std::memcpy(&size,take(4).data(),4);
            if(size>100000)throw std::runtime_error("The head prefab registry has an invalid count.");
            return size;
        };
        const auto name=[&]{
            const auto size=byte();
            const auto value=take(size);
            if(!size||value.back()!=0||value.find('\0')!=size-1)throw std::runtime_error("The head prefab registry has an invalid name.");
            return std::string(value.substr(0,size-1));
        };
        take(8);
        primary=count();
        for(uint32_t i=0;i<primary;++i){
            auto part=name(),directory=name();
            const auto rest=at;
            name();byte();name();byte();
            for(auto children=byte();children>0;--children){name();byte();}
            lowerAscii(part);lowerAscii(directory);
            if((part.find("_head_")!=part.npos||part.find("_nude_")!=part.npos)&&
               !parts.try_emplace(part,directory,bytes.substr(rest,at-rest)).second)
                ambiguous.insert(part);
        }
        secondaryAt=at;
        secondary=count();
        for(uint32_t i=0;i<secondary;++i){
            auto part=name(),directory=name();
            lowerAscii(part);lowerAscii(directory);
            if((part.find("_head_")!=part.npos||part.find("_nude_")!=part.npos)&&
               !lookups.try_emplace(part,directory).second)
                ambiguous.insert(part);
        }
        if(at!=bytes.size())throw std::runtime_error("The head prefab registry has unexpected trailing data.");
    }
    std::string append(uint32_t count,const std::string& rows,const std::string& lookupRows) const {
        std::string result(bytes.substr(0,8));
        const auto write=[&](uint32_t value){result.append(reinterpret_cast<const char*>(&value),4);};
        write(primary+count);
        result+=bytes.substr(12,secondaryAt-12);
        result+=rows;
        write(secondary+count);
        result+=bytes.substr(secondaryAt+4);
        result+=lookupRows;
        return result;
    }
};
}

SkinToneSource activeDamianeSkin(const std::map<std::string,fs::path>& routes){
    auto head=std::string(stockHead);
    if(const auto appearance=routes.find(damianeAppearance);appearance!=routes.end()){
        head=headPrefab(readAsset(appearance->second));
        lowerAscii(head);
        if(head.empty())return {{},{},"Damiane’s mounted appearance has no unambiguous face binding."};
    }
    auto route=std::string(stockFace);
    if(const auto material=routes.find(faceMaterials+head+".pac_xml");material!=routes.end()){
        route=faceColour(readAsset(material->second));
        lowerAscii(route);
        if(route.empty())return {{},{},"Damiane’s mounted head material has no unambiguous skin colour slot."};
    }else if(head!=stockHead){
        return {{},{},"The active face has no readable mounted head material."};
    }
    const auto file=routes.find(route);
    if(file==routes.end()||isOwnModContent(file->second))return {route,{},"No custom Damiane face texture detected."};
    const auto header=textureHeader(file->second);
    if(!textureSize(header)[0]||header.size()<129||(header.substr(84,4)=="DX10"&&header.size()<149))
        return {route,{},"The mounted Damiane face texture is unreadable or has an unsupported DDS layout."};
    return {route,file->second,"Custom Damiane face texture detected."};
}

SkinToneSource activeVenusDamianeSkin(const std::map<std::string,fs::path>& routes){
    if(mountedBodyMode(routes,nullptr,nullptr)!=BodyMode::Venus)
        return {{},{},"Skin matching requires the VENUS witch body option."};
    if(!mountedDamianeVenus(routes))
        return {{},{},"Skin matching requires a recognised VENUS Body for Damiane to be active."};
    auto source=activeDamianeSkin(routes);
    source.bodyColours=stockBodyColours;
    if(const auto material=routes.find(venusMaterial);material!=routes.end()){
        source.bodyColours=skinColours(readAsset(material->second),false);
        for(auto& [mesh,route]:source.bodyColours)lowerAscii(route);
    }
    for(const auto& [mesh,route]:source.bodyColours){
        const auto file=routes.find(route);
        const auto stock=stockBodyColours.find(mesh);
        source.customBody|=file!=routes.end()&&!isOwnModContent(file->second)&&textureSize(textureHeader(file->second))[0];
        source.customBody|=stock!=stockBodyColours.end()&&stock->second!=route;
    }
    if(const auto mounted=routes.find(damianeCustomizationRoute);mounted!=routes.end()){
        const auto data=readAsset(mounted->second);
        const auto brightness=toneField(data,"skinBrightness",100);
        const auto colour=toneField(data,"skinColor",255);
        if(brightness&&brightness->value!=50)source.brightness=brightness->value;
        if(colour&&colour->value!=0&&colour->value!=255)source.colour=colour->value;
    }
    if(source.brightness||source.colour)
        source.detail=source.texture.empty()?"Custom Damiane skin tone detected.":"Custom Damiane face texture and skin tone detected.";
    if(source.customBody)source.detail="Custom Damiane body colour detected. Witch skin routes will be checked at launch.";
    return source;
}

AddonVenusAssets prepareAddonVenus(const fs::path& game,
    const std::map<std::string,fs::path>& routes,const std::vector<std::string>& appearances){
    AddonVenusAssets plan;
    uint32_t unavailable=0;
    if(appearances.empty()||mountedBodyMode(routes,nullptr,&unavailable)!=BodyMode::Venus)return plan;
    const auto model=routes.find(venusBodyModelRoute);
    if(model==routes.end()||isOwnModContent(model->second)||!readableDependency(model->second,"PAR "))return plan;
    const GameArchive archive(game/L"0009");
    const auto read=[&](const std::string& route){
        const auto mounted=routes.find(route);
        return mounted==routes.end()?archive.read(route):readAsset(mounted->second);
    };
    const auto prefab=read("character/bin__/prefab/1_pc/02_phw/nude/phw_00_nude_00_0001_damian.prefab");
    if(prefab.compare(0,6,std::string("\xff\xff\x04\0\0\0",6))!=0||
       prefab.find(venusBodyModelRoute)==prefab.npos)return plan;
    plan.hasBaseUnderwear=prefab.find("character/model/1_pc/2_phw/armor/38_underwear/")!=prefab.npos;
    for(const auto& route:appearances){
        if(route.rfind("character/appearance/3_npc/",0)!=0)continue;
        auto appearance=read(route);
        const auto original=appearancePrefab(appearance,L"Nude");
        auto body=original;
        lowerAscii(body);
        if(body=="phw_00_nude_00_0001"){
            if(!replaceOnce(appearance,original,venusBody)||appearancePrefab(appearance,L"Nude")!=venusBody)continue;
        }else if(body!=venusBody)continue;
        plan.appearances.emplace(route,std::move(appearance));
    }
    return plan;
}

SkinToneAssets prepareSkinTone(const fs::path& game,const std::map<std::string,fs::path>& routes,
                              const std::map<std::string,std::string>& addonAppearances){
    SkinToneAssets plan;
    plan.source=activeVenusDamianeSkin(routes);
    if(!plan.source.custom())return plan;
    const GameArchive stock(game/L"0009");
    const auto get=[&](const std::string& route){
        const auto mounted=routes.find(route);
        return mounted==routes.end()?stock.read(route):readAsset(mounted->second,16*1024*1024);
    };
    const auto getDimensions=[&](std::string route){
        lowerAscii(route);
        const auto mounted=routes.find(route);
        return textureSize(mounted==routes.end()?stock.read(route):textureHeader(mounted->second));
    };
    const auto sameLayout=[&](const std::string& current,const std::string& desired){
        const auto a=getDimensions(current);
        const auto candidate=plan.files.find(desired);
        const auto b=candidate==plan.files.end()?getDimensions(desired):textureSize(candidate->second);
        return a[0]&&b[0]&&uint64_t(a[0])*b[1]==uint64_t(b[0])*a[1];
    };
    std::set<std::string> sources;
    const auto addMountedSource=[&](const std::string& route){
        if(routes.count(route))sources.insert(route);
    };
    auto face=plan.source.texture.empty()?std::string{}:plan.source.route;
    auto desiredBody=plan.source.bodyColours;
    std::map<std::string,std::string> derived;
    if(plan.source.customBody){
        const auto torso=desiredBody.find(torsoSlot);
        if(torso!=desiredBody.end()&&routes.count(torso->second)){
            const auto gain=compareSkinColour(stock.read(stockBodyColours.at(torsoSlot)),get(torso->second));
            if(gain){
                const auto tint=[&](const std::string& route){
                    const auto alias="character/texture/hexappeal_skin_"+fs::path(route).filename().string();
                    if(routes.count(alias))return std::string{};
                    auto adjusted=adjustSkinColour(stock.read(route),*gain);
                    if(adjusted.empty())return std::string{};
                    derived.emplace(alias,std::move(adjusted));
                    return alias;
                };
                for(auto& [slot,route]:desiredBody){
                    const auto original=stockBodyColours.find(slot);
                    if(slot==torsoSlot||original==stockBodyColours.end()||original->second!=route||routes.count(route))
                        continue;
                    const auto adjusted=tint(route);
                    if(adjusted.empty())plan.skipped.push_back("Could not derive a private colour for "+route+".");
                    else route=adjusted;
                }
                if(face.empty()&&!plan.source.route.empty()&&!routes.count(plan.source.route)){
                    face=tint(plan.source.route);
                    if(face.empty())plan.skipped.push_back("Could not derive a private face colour.");
                }
                if(!derived.empty())addMountedSource(torso->second);
            }
        }
    }
    plan.files.merge(derived);
    std::string registryBytes;
    std::optional<Registry> registry;
    if(!face.empty()||!desiredBody.empty()){
        registryBytes=get(skinRegistryRoute);
        try{registry.emplace(registryBytes);}
        catch(const std::exception& error){plan.skipped.push_back(std::string("Skin textures: ")+error.what());}
    }
    std::string primary,secondary;
    unsigned aliasCount=0;
    std::map<std::string,std::string> aliases,bodyAliases;
    std::set<std::string> usedColours;
    const auto registerPart=[&](const std::string& original,const std::string& directory,
                                const std::string& alias,const std::string& destination){
        const auto entry=registry->parts.find(original);
        const auto lookup=registry->lookups.find(original);
        if(entry==registry->parts.end()||lookup==registry->lookups.end()||registry->ambiguous.count(original)||
           entry->second.first!=directory||lookup->second!=directory||
           registry->parts.count(alias)||registry->lookups.count(alias))
            throw std::runtime_error("The skin prefab registry does not match the expected layout.");
        primary+=Registry::text(alias)+Registry::text(destination)+std::string(entry->second.second);
        secondary+=Registry::text(alias)+Registry::text(destination);
        ++aliasCount;
    };
    const auto bodyAlias=[&](const std::string& original){
        if(const auto existing=bodyAliases.find(original);existing!=bodyAliases.end())return existing->second;
        const bool isolated=original=="hva_00_nude_00_0001_damian";
        if(!isolated&&original!=venusBody&&original!="phw_00_nude_00_0001_damian_lnw")
            throw std::runtime_error("The witch body uses an unrecognised prefab.");
        const std::string directory=isolated?"ha_p/02_phw/nude":"1_pc/02_phw/nude";
        const auto prefabRoute=std::string("character/bin__/prefab/")+directory+"/"+original+".prefab";
        auto prefab=get(prefabRoute);
        const std::string sharedStem=std::string("1_pc/2_phw/nude/")+venusBody;
        const std::string isolatedStem="ha_p/2_phw/nude/hva_00_nude_00_0001_damian";
        const bool isolatedModel=isolated&&prefab.find("character/model/"+isolatedStem+".pac")!=prefab.npos;
        const auto& stem=isolatedModel?isolatedStem:sharedStem;
        const auto materialRoute="character/modelproperty/"+stem+".pac_xml";
        auto material=get(materialRoute);
        auto bindings=skinColours(material,false);
        if(bindings.empty())throw std::runtime_error("No unambiguous witch body colour slots were found.");
        std::set<std::string> colours;
        for(auto& [slot,route]:bindings){
            const auto wanted=desiredBody.find(slot);
            if(wanted==desiredBody.end()||wanted->second==route)continue;
            if(!sameLayout(route,wanted->second)||
               !replaceOnce(material,"_path=\""+route+"\"","_path=\""+wanted->second+"\"")){
                plan.skipped.push_back("The colour binding for "+slot+" on "+original+" could not be matched.");
                continue;
            }
            route=wanted->second;
            colours.insert(route);
        }
        if(colours.empty()){bodyAliases.emplace(original,std::string{});return std::string{};}
        const std::string prefix=isolatedModel?"hxu":"hxt";
        const auto alias=(isolated?"hxu":"hxt")+original.substr(3);
        const auto privateStem="ha_p/2_phw/nude/"+prefix+std::string(venusBody).substr(3);
        const auto model="character/model/"+stem+".pac";
        const auto privateModel="character/model/"+privateStem+".pac";
        auto geometry=get(model);
        auto physics=get("character/bin__/meshphysics/"+stem+".hkx");
        auto metadata=get("character/prefab/"+directory+"/"+original+".prefabdata_xml");
        if(skinColours(material,false)!=bindings||prefab.compare(0,6,std::string("\xff\xff\x04\0\0\0",6))!=0||
           model.size()!=privateModel.size()||!replaceOnce(prefab,model,privateModel)||
           geometry.compare(0,4,"PAR ")!=0||physics.compare(4,4,"TAG0")!=0||
           !visitXml(metadata,[](IXmlReader&,UINT depth,std::wstring_view tag){return depth||tag==L"NudePrefabData";}))
            throw std::runtime_error("The installed witch body assets could not be isolated safely.");
        registerPart(original,directory,alias,"ha_p/02_phw/nude");
        plan.files.emplace("character/bin__/prefab/ha_p/02_phw/nude/"+alias+".prefab",std::move(prefab));
        plan.files.emplace("character/prefab/ha_p/02_phw/nude/"+alias+".prefabdata_xml",std::move(metadata));
        plan.files.emplace(privateModel,std::move(geometry));
        plan.files.emplace("character/modelproperty/"+privateStem+".pac_xml",std::move(material));
        plan.files.emplace("character/bin__/meshphysics/"+privateStem+".hkx",std::move(physics));
        for(const auto& colour:colours){usedColours.insert(colour);addMountedSource(colour);}
        bodyAliases.emplace(original,alias);
        ++plan.privateBodies;
        return alias;
    };
    std::vector<std::string> appearances(std::begin(witchAppearances),std::end(witchAppearances));
    for(const auto& [appearance,content]:addonAppearances)appearances.push_back(appearance);
    for(const auto& appearanceRoute:appearances){
        const auto addon=addonAppearances.find(appearanceRoute);
        auto appearance=addon==addonAppearances.end()?get(appearanceRoute):addon->second;
        bool patched=false;
        if(plan.source.brightness||plan.source.colour){
            try{
                const bool selectedAddon=addon!=addonAppearances.end();
                const auto sourceName=selectedAddon?addonCustomization(appearance):std::string{};
                const auto route=selectedAddon?(sourceName.empty()?std::string{}:customizationRoot+sourceName+".paccd_xml"):
                    witchCustomization(appearanceRoute,appearance);
                if(route.empty())throw std::runtime_error("Uses a different customization route.");
                auto data=get(route);
                applyTone(data,"skinBrightness",100,plan.source.brightness);
                applyTone(data,"skinColor",255,plan.source.colour);
                auto destination=route;
                if(selectedAddon){
                    const auto alias="cd_npc_south/ha_"+fs::path(appearanceRoute).stem().string()+"_customization";
                    destination=customizationRoot+alias+".paccd_xml";
                    if(routes.count(destination)||plan.files.count(destination))
                        throw std::runtime_error("The private tone route is already used.");
                    auto isolated=appearance;
                    if(!replaceOnce(isolated,sourceName,alias)||addonCustomization(isolated)!=alias)
                        throw std::runtime_error("The customization reference is ambiguous.");
                    appearance=std::move(isolated);
                    patched=true;
                }
                plan.files.emplace(destination,std::move(data));
                plan.tones.push_back(std::move(destination));
                addMountedSource(damianeCustomizationRoute);
            }catch(const std::exception& error){
                plan.skipped.push_back(fs::path(appearanceRoute).stem().string()+" tone — "+error.what());
            }
        }
        if(registry&&!desiredBody.empty()){
            try{
                const auto original=appearancePrefab(appearance,L"Nude");
                auto normalized=original;
                lowerAscii(normalized);
                const auto first=appearance.find(original);
                if(original.empty()||first==appearance.npos||appearance.find(original,first+original.size())!=appearance.npos)
                    throw std::runtime_error("The body reference is ambiguous.");
                const auto alias=normalized==venusBody&&!plan.source.customBody?std::string{}:bodyAlias(normalized);
                if(!alias.empty()){
                    auto withBody=appearance;
                    withBody.replace(first,original.size(),alias);
                    if(appearancePrefab(withBody,L"Nude")!=alias)
                        throw std::runtime_error("The body reference could not be redirected.");
                    appearance=std::move(withBody);
                    patched=true;
                }
            }catch(const std::exception& error){
                plan.skipped.push_back(fs::path(appearanceRoute).stem().string()+" body — "+error.what());
            }
        }
        if(registry&&!face.empty()){
        const auto originalName=headPrefab(appearance);
        auto head=originalName;
        lowerAscii(head);
        try{
            if(head.empty())throw std::runtime_error("No supported head prefab.");
            const auto alias="hxk"+head.substr(3);
            auto withHead=appearance;
            if(!replaceOnce(withHead,originalName,alias)||headPrefab(withHead,true)!=alias)
                throw std::runtime_error("The head reference is ambiguous.");
            if(!aliases.count(head)){
                const auto model="character/model/1_pc/2_phw/head/head/"+head+".pac";
                const auto privateModel="character/model/ha_p/2_phw/head/head/"+alias+".pac";
                auto prefab=get(std::string("character/bin__/prefab/")+stockHeadDirectory+"/"+head+".prefab");
                auto metadata=get(std::string("character/prefab/")+stockHeadDirectory+"/"+head+".prefabdata_xml");
                auto mesh=get(model);
                auto physics=get("character/bin__/meshphysics/1_pc/2_phw/head/head/"+head+".hkx");
                auto material=get(faceMaterials+head+".pac_xml");
                const auto oldFace=faceColour(material);
                if(!sameLayout(oldFace,face))
                    throw std::runtime_error("The source and target face textures have different layouts.");
                if(prefab.compare(0,6,std::string("\xff\xff\x04\0\0\0",6))!=0||
                   model.size()!=privateModel.size()||!replaceOnce(prefab,model,privateModel)||
                   mesh.compare(0,4,"PAR ")!=0||physics.compare(4,4,"TAG0")!=0||
                   !visitXml(metadata,[](IXmlReader&,UINT depth,std::wstring_view tag){return depth||tag==L"HeadPrefabData";})||
                   !replaceOnce(material,"_path=\""+oldFace+"\"","_path=\""+face+"\"")||
                   faceColour(material)!=face)
                    throw std::runtime_error("The installed head assets cannot be isolated safely.");
                registerPart(head,stockHeadDirectory,alias,privateHeadDirectory);
                plan.files.emplace(std::string("character/bin__/prefab/")+privateHeadDirectory+"/"+alias+".prefab",std::move(prefab));
                plan.files.emplace(std::string("character/prefab/")+privateHeadDirectory+"/"+alias+".prefabdata_xml",std::move(metadata));
                plan.files.emplace(privateModel,std::move(mesh));
                plan.files.emplace("character/bin__/meshphysics/ha_p/2_phw/head/head/"+alias+".hkx",std::move(physics));
                plan.files.emplace("character/modelproperty/ha_p/2_phw/head/head/"+alias+".pac_xml",std::move(material));
                aliases.emplace(head,alias);
            }
            appearance=std::move(withHead);
            patched=true;
            usedColours.insert(face);
            addMountedSource(face);
        }catch(const std::exception& error){
            plan.skipped.push_back(fs::path(appearanceRoute).stem().string()+" — "+error.what());
        }
        }
        if(patched){
            plan.files.emplace(appearanceRoute,std::move(appearance));
            plan.appearances.push_back(appearanceRoute);
        }
    }
    if(aliasCount)plan.files.emplace(skinRegistryRoute,registry->append(aliasCount,primary,secondary));
    for(auto it=plan.files.begin();it!=plan.files.end();){
        if(it->first.rfind("character/texture/hexappeal_skin_",0)==0&&!usedColours.count(it->first))
            it=plan.files.erase(it);
        else ++it;
    }
    plan.sources.assign(sources.begin(),sources.end());
    return plan;
}
}
