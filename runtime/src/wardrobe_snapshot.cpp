#include "wardrobe_snapshot.h"
#include <Windows.h>
#include <cstring>

namespace hex {
namespace {
struct Appearance {const char* path;int witch;};
constexpr Appearance appearances[]={
    {"character/appearance/3_npc/cd_npc_south/cd_nhw_south/cd_nhw_south_20166.app_xml",0},
    {"character/appearance/3_npc/cd_npc_north/cd_nhw_north/cd_nhw_north_20026.app_xml",1},
    {"character/appearance/3_npc/cd_npc_south/cd_nhw_south/cd_nhw_south_20167.app_xml",1},
    {"character/appearance/3_npc/cd_npc_south/cd_nhw_south/cd_nhw_south_20165.app_xml",2},
    {"character/appearance/3_npc/cd_npc_south/cd_nhw_south/cd_nhw_south_20168.app_xml",3},
    {"character/appearance/3_npc/cd_npc_south/cd_nhw_south/cd_nhw_south_20169.app_xml",4},
    {"character/appearance/2_mon/cd_m0001_00_twofeet/cd_m0001_00_phw_north/cd_m0001_00_phw_north_44001.app_xml",5}
};
uintptr_t pointer(uintptr_t at){return *reinterpret_cast<const uintptr_t*>(at);}
uint32_t number(uintptr_t at){return *reinterpret_cast<const uint32_t*>(at);}
bool text(uintptr_t wrapper,char* output,size_t capacity) {
    if(!wrapper)return false;
    const auto length=number(wrapper+8);const auto* value=reinterpret_cast<const unsigned char*>(pointer(wrapper));
    if(!value||!length||length>=capacity)return false;
    for(uint32_t i=0;i<length;++i){if(value[i]<32||value[i]>126)return false;output[i]=char(value[i]);}
    output[length]=0;return true;
}
bool upperGarment(const char* name){return std::strstr(name,"_ub_")!=nullptr;}
}
int wardrobeWitchForAppearance(const char* path){
    if(!path)return -1;
    for(const auto& row:appearances)if(!_stricmp(path,row.path))return row.witch;
    return -1;
}
WardrobePartKind wardrobePartKind(int witch,const char* prefab){
    if(!prefab||witch<0||witch>5)return WardrobePartKind::Other;
    if(!strcmp(prefab,"phw_00_uw_00_0001")||!strcmp(prefab,"cd_phw_00_uw_00_0001"))return WardrobePartKind::Underwear;
    static const char* defaults[]={"cd_nhw_so_ub_20166","cd_nhw_so_ub_20167","cd_nhw_so_ub_20165","cd_nhw_so_ub_20168","cd_nhw_so_ub_20169_u","cd_phw_00_ub_00_0164"};
    if(!strcmp(prefab,defaults[witch])||(witch==1&&!strcmp(prefab,"cd_phw_00_ub_00_0157")))return WardrobePartKind::DefaultGarment;
    return WardrobePartKind::Other;
}
bool defaultOutfitPart(int witch,const char* prefab){
    if(!prefab||witch<0||witch>5)return false;
    if(wardrobePartKind(witch,prefab)==WardrobePartKind::DefaultGarment)return true;
    static const char* parts[6][5]={
        {"cd_phw_00_foot_0092","cd_phw_00_hel_0185","cd_phw_00_foot_00_0185",nullptr},
        {"cd_phw_00_foot_00_0163","cd_phw_00_hel_00_0187","cd_phw_00_foot_0038",nullptr},
        {"cd_phw_00_foot_00_0161_index01","cd_phw_00_hel_0177",nullptr},
        {"cd_phw_00_foot_00_0160","cd_phw_00_hel_00_0186",nullptr},
        {"cd_phw_00_foot_0038","cd_phw_00_hel_00_0206_dd","cd_nhw_so_cloak_20169_s",nullptr},
        {"cd_phw_00_foot_0137_01_index01","cd_phw_00_hel_00_0164",nullptr}
    };
    for(const auto* part:parts[witch])if(part&&!strcmp(prefab,part))return true;return false;
}
bool captureWardrobeSnapshot(uintptr_t actor,int witch,uintptr_t gameBase,WardrobeSnapshot& output){
    if(!actor||witch<0||witch>5||!gameBase)return false;
    __try {
        WardrobeSnapshot next{};next.actor=actor;next.witch=witch;
        if(pointer(actor)!=gameBase+0x5991108)return false;
        const auto components=pointer(actor+0x68);if(!components)return false;
        next.control=pointer(components+0x40);
        if(!next.control||pointer(next.control)!=gameBase+0x59c1660||pointer(next.control+8)!=actor)return false;
        next.renderer=pointer(next.control+0x130);if(!next.renderer)return false;
        const auto table=pointer(next.renderer);if(!table)return false;
        next.skinned=pointer(table+8);
        if(!next.skinned||pointer(next.skinned)!=gameBase+0x5f850f0||pointer(next.skinned+0x40)!=gameBase+0x7246a40)return false;
        next.node=pointer(next.skinned+0xb0);
        if(!next.node||pointer(next.node)!=gameBase+0x5f7be58||pointer(next.node+0x50)!=table)return false;
        next.pathWrapper=pointer(next.node+0x18);
        if(!text(next.pathWrapper,next.appearance,sizeof next.appearance)||wardrobeWitchForAppearance(next.appearance)!=witch)return false;
        next.claimData=pointer(next.node+0x58);next.claimCount=number(next.node+0x60);
        const auto capacity=number(next.node+0x64);next.readyCount=number(next.node+0x48);
        if(!next.claimData||next.claimCount>capacity||next.claimCount>next.claims.size()||next.readyCount>256)return false;
        for(uint32_t i=0;i<next.claimCount;++i){
            auto& claim=next.claims[i];const auto entry=next.claimData+16*i;
            claim.key=number(entry);claim.record=pointer(entry+8);if(!claim.record)return false;
            claim.wrapper=pointer(claim.record+0x40);claim.scene=pointer(claim.record+0x30);
            if(!text(claim.wrapper,claim.prefab,sizeof claim.prefab))return false;
            claim.kind=wardrobePartKind(witch,claim.prefab);
            if(claim.kind==WardrobePartKind::DefaultGarment)next.defaultGarment=true;
            else if(upperGarment(claim.prefab))next.customGarment=true;
        }

        if(pointer(next.control+0x130)!=next.renderer||pointer(next.renderer)!=table||pointer(table+8)!=next.skinned||
           pointer(next.skinned+0xb0)!=next.node||pointer(next.node+0x50)!=table||pointer(next.node+0x18)!=next.pathWrapper||
           pointer(next.node+0x58)!=next.claimData||number(next.node+0x60)!=next.claimCount)return false;
        output=next;return true;
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
}
