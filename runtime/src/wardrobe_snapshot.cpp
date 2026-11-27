#include "wardrobe_snapshot.h"
#include "game_layout.h"
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
bool actorWasReleased(uintptr_t actor,uintptr_t gameBase){
    __try {return pointer(actor)!=gameBase+gameLayout().actor;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
}
int wardrobeWitchForAppearance(const char* path){
    if(!path)return -1;
    for(const auto& row:appearances)if(!_stricmp(path,row.path))return row.witch;
    return -1;
}
WardrobePartKind wardrobePartKind(int witch,const char* prefab){
    if(!prefab||witch<0)return WardrobePartKind::Other;
    if(!strcmp(prefab,"phw_00_uw_00_0001")||!strcmp(prefab,"cd_phw_00_uw_00_0001")||!strcmp(prefab,"phw_00_uw_00_0002")||!strcmp(prefab,"cd_phw_00_uw_00_0002"))return WardrobePartKind::Underwear;
    if(witch>5)return WardrobePartKind::Other;
    static const char* defaults[]={"cd_nhw_so_ub_20166","cd_nhw_so_ub_20167","cd_nhw_so_ub_20165","cd_nhw_so_ub_20168","cd_nhw_so_ub_20169_u","cd_phw_00_ub_00_0164"};
    if(!strcmp(prefab,defaults[witch])||(witch==1&&!strcmp(prefab,"cd_phw_00_ub_00_0157")))return WardrobePartKind::DefaultGarment;
    return WardrobePartKind::Other;
}
bool defaultOutfitPart(int witch,const char* prefab){
    if(!prefab||witch<0||witch>5)return false;
    if(wardrobePartKind(witch,prefab)==WardrobePartKind::DefaultGarment)return true;
    static const char* parts[6][6]={
        {"cd_phw_00_foot_0092","cd_phw_00_hel_0185","cd_phw_00_foot_00_0185",nullptr},
        {"cd_phw_00_foot_00_0163","cd_phw_00_hel_00_0187","cd_phw_00_foot_0038","ha_phw_00_foot_00_0163",nullptr},
        {"cd_phw_00_foot_00_0161_index01","cd_phw_00_hel_0177","ha_phw_00_foot_00_0161_index01",nullptr},
        {"cd_phw_00_foot_00_0160","cd_phw_00_hel_00_0186","ha_phw_00_foot_00_0160",nullptr},
        {"cd_phw_00_foot_0038","cd_phw_00_hel_00_0206_dd","cd_nhw_so_cloak_20169_s",nullptr},
        {"cd_phw_00_foot_0137_01_index01","cd_phw_00_hel_00_0164",nullptr}
    };
    for(const auto* part:parts[witch])if(part&&!strcmp(prefab,part))return true;return false;
}
bool operator==(const WardrobeVisibilityClaim& first,const WardrobeVisibilityClaim& second){
    const auto& a=first.instance;
    const auto& b=second.instance;
    return a.actor==b.actor&&a.control==b.control&&a.renderer==b.renderer&&a.node==b.node&&a.witch==b.witch&&
        first.key==second.key&&first.record==second.record&&first.wrapper==second.wrapper&&first.scene==second.scene&&
        first.prefab==second.prefab;
}
WardrobeVisibilityClaim wardrobeVisibilityClaim(const WardrobeSnapshot& snapshot,const WardrobeClaim& claim){
    return {{snapshot.actor,snapshot.control,snapshot.renderer,snapshot.node,snapshot.witch},
        claim.key,claim.record,claim.wrapper,claim.scene,claim.prefab};
}
bool hasWardrobeVisibilityClaim(const WardrobeSnapshot& snapshot,const WardrobeVisibilityClaim& mask){
    const auto& instance=mask.instance;
    if(instance.actor!=snapshot.actor||instance.control!=snapshot.control||instance.renderer!=snapshot.renderer||
       instance.node!=snapshot.node||instance.witch!=snapshot.witch)return false;
    for(uint32_t i=0;i<snapshot.claimCount;++i){
        const auto& claim=snapshot.claims[i];
        if(claim.key==mask.key&&claim.record==mask.record&&claim.wrapper==mask.wrapper&&claim.scene==mask.scene&&
           mask.prefab==claim.prefab)return true;
    }
    return false;
}
bool rebindAddonVisibilityClaim(const WardrobeSnapshot& snapshot,WardrobeVisibilityClaim& mask){
    const auto& instance=mask.instance;
    if(instance.actor!=snapshot.actor||instance.control!=snapshot.control||instance.renderer!=snapshot.renderer||
       instance.node!=snapshot.node||instance.witch!=snapshot.witch)return false;
    const WardrobeClaim* replacement=nullptr;
    for(uint32_t i=0;i<snapshot.claimCount;++i){
        const auto& claim=snapshot.claims[i];
        if(claim.key!=mask.key)continue;
        if(replacement||!claim.scene||claim.wrapper!=mask.wrapper||mask.prefab!=claim.prefab)return false;
        replacement=&claim;
    }
    if(!replacement)return false;
    mask=wardrobeVisibilityClaim(snapshot,*replacement);
    return true;
}
bool isWardrobeNpc(uintptr_t actor,uintptr_t gameBase){
    const auto& layout=gameLayout();
    if(!actor||!gameBase||!layout.actor)return false;
    __try{
        const auto type=pointer(actor);
        if(type==gameBase+layout.actor)return true;
        if(!layout.playerActor||!layout.actorManagerGlobal||!layout.actorManager||
           type!=gameBase+layout.playerActor)return false;
        const auto manager=pointer(gameBase+layout.actorManagerGlobal);
        if(!manager||pointer(manager)!=gameBase+layout.actorManager)return false;
        const auto player=pointer(manager+0x50);
        return player&&player!=actor&&pointer(player)==gameBase+layout.playerActor&&
            pointer(manager+0x50)==player&&pointer(gameBase+layout.actorManagerGlobal)==manager;
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
uintptr_t readWardrobeActorAppearance(uintptr_t actor,uintptr_t gameBase,char (&path)[256]){
    if(!actor||!gameBase)return 0;
    __try {
        if(!isWardrobeNpc(actor,gameBase))return 0;
        const auto components=pointer(actor+0x68);
        const auto control=components?pointer(components+0x40):0;
        if(!control||pointer(control)!=gameBase+gameLayout().control||pointer(control+8)!=actor)return 0;
        const auto renderer=pointer(control+0x130);
        const auto table=renderer?pointer(renderer):0;
        const auto skinned=table?pointer(table+8):0;
        if(!skinned||pointer(skinned)!=gameBase+gameLayout().skinned||pointer(skinned+0x40)!=gameBase+gameLayout().skinGlobal)return 0;
        const auto node=pointer(skinned+0xb0);
        if(!node||pointer(node)!=gameBase+gameLayout().node||pointer(node+0x50)!=table)return 0;
        const auto wrapper=pointer(node+0x18);
        if(!text(wrapper,path,sizeof path))return 0;
        return pointer(control+0x130)==renderer&&pointer(renderer)==table&&pointer(table+8)==skinned&&
            pointer(skinned+0xb0)==node&&pointer(node+0x50)==table&&pointer(node+0x18)==wrapper?renderer:0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return 0;}
}
int wardrobeWitchForActorAppearance(uintptr_t actor,uintptr_t gameBase){
    char path[256]{};
    return readWardrobeActorAppearance(actor,gameBase,path)?wardrobeWitchForAppearance(path):-1;
}
bool readWardrobeCharacterRecord(uintptr_t actor,uintptr_t gameBase,uintptr_t characterInfo,char (&name)[96]){
    if(!actor||!gameBase||!characterInfo)return false;
    __try {
        if(!isWardrobeNpc(actor,gameBase))return false;
        const auto components=pointer(actor+0x68);
        const auto entry=components?pointer(components+0x20):0;
        if(!entry||pointer(entry+8)!=actor)return false;
        const auto index=*reinterpret_cast<const uint16_t*>(entry+0x30);
        const auto table=pointer(characterInfo);
        if(!table)return false;
        const auto records=databaseRecordStorage(table);
        const auto count=databaseRecordCount(table);
        if(!records||count>0x10000||index>=count)return false;
        const auto row=databaseRecordAt(records,index);
        if(gameLayout().expandedRecords){
            const auto indexRow=records+size_t(index)*0x30;
            const auto label=pointer(indexRow+8);
            if(!row||number(row)!=number(indexRow)||number(row+4)!=index||!text(label,name,sizeof name))return false;
            return pointer(actor+0x68)==components&&pointer(components+0x20)==entry&&pointer(entry+8)==actor&&
                *reinterpret_cast<const uint16_t*>(entry+0x30)==index&&pointer(characterInfo)==table&&
                databaseRecordStorage(table)==records&&databaseRecordAt(records,index)==row&&pointer(indexRow+8)==label;
        }
        const auto label=row?pointer(row):0;
        const auto* value=label?reinterpret_cast<const unsigned char*>(pointer(label)):nullptr;
        if(!value)return false;
        size_t length=0;
        while(length<sizeof name){
            const auto ch=value[length];
            if(!ch)break;
            if(ch<32||ch>126)return false;
            name[length++]=static_cast<char>(ch);
        }
        if(!length||length==sizeof name)return false;
        name[length]=0;
        return pointer(actor+0x68)==components&&pointer(components+0x20)==entry&&
            pointer(entry+8)==actor&&*reinterpret_cast<const uint16_t*>(entry+0x30)==index&&
            pointer(characterInfo)==table&&databaseRecordStorage(table)==records&&databaseRecordAt(records,index)==row;
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
uintptr_t checkedWardrobeRenderer(uintptr_t actor,int witch,uintptr_t gameBase,const char* addonAppearance){
    if(!addonAppearance&&(witch<0||witch>5))return 0;
    char path[256]{};
    const auto renderer=readWardrobeActorAppearance(actor,gameBase,path);
    if(!renderer)return 0;
    const bool matches=addonAppearance?_stricmp(path,addonAppearance)==0:wardrobeWitchForAppearance(path)==witch;
    return matches?renderer:0;
}
bool matchesWardrobeAppearance(uintptr_t actor,uintptr_t gameBase,const char* appearance){
    return checkedWardrobeRenderer(actor,-1,gameBase,appearance)!=0;
}
bool captureWardrobeSnapshot(uintptr_t actor,int witch,uintptr_t gameBase,WardrobeSnapshot& output,
    const char* addonAppearance,const char** issue,bool verifiedRecord){
    if(issue)*issue="Waiting for the live character renderer.";
    if(!actor||witch<0||(witch>5)!=bool(addonAppearance)||!gameBase)return false;
    __try {
        WardrobeSnapshot next{};next.actor=actor;next.witch=witch;
        if(!isWardrobeNpc(actor,gameBase))return false;
        const auto components=pointer(actor+0x68);if(!components)return false;
        next.control=pointer(components+0x40);
        if(!next.control||pointer(next.control)!=gameBase+gameLayout().control||pointer(next.control+8)!=actor)return false;
        next.renderer=pointer(next.control+0x130);if(!next.renderer)return false;
        const auto table=pointer(next.renderer);if(!table)return false;
        next.skinned=pointer(table+8);
        if(!next.skinned||pointer(next.skinned)!=gameBase+gameLayout().skinned||pointer(next.skinned+0x40)!=gameBase+gameLayout().skinGlobal)return false;
        next.node=pointer(next.skinned+0xb0);
        if(!next.node||pointer(next.node)!=gameBase+gameLayout().node||pointer(next.node+0x50)!=table)return false;
        next.pathWrapper=pointer(next.node+0x18);
        if(!text(next.pathWrapper,next.appearance,sizeof next.appearance))return false;
        const int appearanceWitch=wardrobeWitchForAppearance(next.appearance);
        if(addonAppearance?_stricmp(next.appearance,addonAppearance)!=0:
           appearanceWitch!=witch&&(!verifiedRecord||appearanceWitch>=0)){
            if(issue)*issue="The live appearance does not match this character's registered appearance.";
            return false;
        }
        if(issue)*issue="Waiting for a complete equipment part list from the live renderer.";
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
        output=next;if(issue)*issue=nullptr;return true;
    }__except(EXCEPTION_EXECUTE_HANDLER){
        if(issue)*issue="The actor's renderer changed during equipment capture. Retrying.";
        return false;
    }
}
bool canDiscardWardrobeInstance(const WardrobeInstance& instance,uintptr_t gameBase,uint64_t inactiveFor,bool hasOutfitState){
    if(inactiveFor<=30000)return false;
    if(!hasOutfitState)return true;
    if(!instance.actor||!gameBase||instance.witch<0||instance.witch>5)return false;
    MEMORY_BASIC_INFORMATION region{};
    if(!VirtualQuery(reinterpret_cast<const void*>(instance.actor),&region,sizeof region))return false;
    if(region.State==MEM_FREE||region.State==MEM_RESERVE)return true;
    if(region.State!=MEM_COMMIT||(region.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return false;
    if(actorWasReleased(instance.actor,gameBase))return true;
    WardrobeSnapshot current;
    return captureWardrobeSnapshot(instance.actor,instance.witch,gameBase,current)&&
        (current.control!=instance.control||current.renderer!=instance.renderer||current.node!=instance.node);
}
bool captureBarberMeshChoices(const WardrobeSnapshot& snapshot,uintptr_t gameBase,std::array<uint8_t,16>& choices){
    if(!gameBase||!snapshot.actor||!snapshot.control||!snapshot.renderer)return false;
    __try {
        if(pointer(snapshot.control)!=gameBase+gameLayout().control||pointer(snapshot.control+8)!=snapshot.actor||pointer(snapshot.control+0x130)!=snapshot.renderer)return false;
        const auto holder=pointer(snapshot.control+0xb8);
        const auto controller=holder?pointer(holder+0x20):0;
        if(!controller||pointer(controller)!=gameBase+gameLayout().controller)return false;
        char file[128]{};
        if(!text(pointer(controller+0x70),file,sizeof file)||_stricmp(file,"character/descriptors/customizationmeta/meshparam_spouse_phw.xml"))return false;
        const auto mesh=pointer(controller+0xa0);
        if(!mesh||number(controller+0xa8)!=choices.size())return false;
        std::array<uint8_t,16> read{};
        std::memcpy(read.data(),reinterpret_cast<const void*>(mesh),read.size());
        if(pointer(holder+0x20)!=controller||pointer(controller+0xa0)!=mesh||number(controller+0xa8)!=choices.size()||pointer(snapshot.control+0x130)!=snapshot.renderer)return false;
        choices=read;
        return true;
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
}
