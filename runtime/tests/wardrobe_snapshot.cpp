#include "wardrobe_snapshot.h"
#include "game_layout.h"
#include <Windows.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static void check(bool value,const char* message){if(!value){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
template<size_t N> using Block=std::array<unsigned char,N>;
static void put(void* block,size_t offset,uintptr_t value){std::memcpy(static_cast<unsigned char*>(block)+offset,&value,sizeof value);}
static uintptr_t ptr(void* value){return reinterpret_cast<uintptr_t>(value);}
static constexpr uintptr_t base=0x140000000;
struct Fixture {
    Block<256> actor{},components{};
    Block<512> control{},skinned{},node{};
    Block<256> holder{},controller{};
    Block<32> renderer{},table{},claims{};
    std::array<Block<128>,2> records{};
    std::array<Block<512>,4> wrappers{};
    std::array<uint8_t,16> mesh{};
    void setText(int index,const char* name){auto& w=wrappers[index];w.fill(0);const uint32_t length=uint32_t(std::strlen(name));put(w.data(),0,ptr(w.data()+24));std::memcpy(w.data()+8,&length,4);std::memcpy(w.data()+24,name,length+1);}
    Fixture(){
        put(actor.data(),0,base+0x5991108);put(actor.data(),0x68,ptr(components.data()));
        put(components.data(),0x40,ptr(control.data()));put(control.data(),0,base+0x59c1660);put(control.data(),8,ptr(actor.data()));put(control.data(),0x130,ptr(renderer.data()));
        put(renderer.data(),0,ptr(table.data()));put(table.data(),8,ptr(skinned.data()));
        put(skinned.data(),0,base+0x5f850f0);put(skinned.data(),0x40,base+0x7246a40);put(skinned.data(),0xb0,ptr(node.data()));
        put(node.data(),0,base+0x5f7be58);put(node.data(),0x50,ptr(table.data()));put(node.data(),0x18,ptr(wrappers[0].data()));
        setText(0,"character/appearance/3_npc/cd_npc_south/cd_nhw_south/cd_nhw_south_20168.app_xml");
        setText(1,"cd_nhw_so_ub_20168");setText(2,"phw_00_nude_00_0001_damian");
        put(node.data(),0x58,ptr(claims.data()));put(node.data(),0x60,uintptr_t(2)|(uintptr_t(2)<<32));put(node.data(),0x48,2);
        for(int i=0;i<2;++i){put(claims.data(),i*16,0xd900+i);put(claims.data(),i*16+8,ptr(records[i].data()));put(records[i].data(),0x40,ptr(wrappers[i+1].data()));}
    }
};
int main(){
    hex::testGameLayout()=&hex::layouts[0];  // the fixtures use this build's addresses
    check(!hex::wardrobeActorRecentlySeen(4500,0)&&hex::wardrobeActorRecentlySeen(4500,2501)&&
          !hex::wardrobeActorRecentlySeen(4500,2500)&&!hex::wardrobeActorRecentlySeen(4500,4501),
          "only a recent actual render observation grants an actor ownership, including during overlapping reloads");
    hex::WardrobeSnapshot out;Fixture f;
    check(hex::captureWardrobeSnapshot(ptr(f.actor.data()),3,base,out),"exact actor-renderer-node chain accepted");
    check(hex::wardrobeWitchForActorAppearance(ptr(f.actor.data()),base)==3,
          "a supported witch is discovered through its validated live appearance without the identity plugin");
    Block<64> identity{},characterInfo{},characterTable{},characterRows{},characterRecord{};
    put(f.components.data(),0x20,ptr(identity.data()));
    put(identity.data(),8,ptr(f.actor.data()));
    put(identity.data(),0x30,1);
    put(characterInfo.data(),0,ptr(characterTable.data()));
    put(characterTable.data(),0x20,ptr(characterRows.data()));
    put(characterTable.data(),0x28,2);
    put(characterRows.data(),0x18+8,ptr(characterRecord.data()));
    f.setText(3,"NHW_Unique_Goodness_Witch_1");
    put(characterRecord.data(),0,ptr(f.wrappers[3].data()));
    char record[96]{};
    check(hex::readWardrobeCharacterRecord(ptr(f.actor.data()),base,ptr(characterInfo.data()),record)&&
          std::strcmp(record,"NHW_Unique_Goodness_Witch_1")==0,
          "native character record identity resolves with no helper module");
    put(identity.data(),0x30,2);
    check(!hex::readWardrobeCharacterRecord(ptr(f.actor.data()),base,ptr(characterInfo.data()),record),
          "out of bounds native character index is rejected");
    put(identity.data(),0x30,1);
    put(identity.data(),8,0);
    check(!hex::readWardrobeCharacterRecord(ptr(f.actor.data()),base,ptr(characterInfo.data()),record),
          "a reused identity component cannot resolve a different actor");
    check(out.defaultGarment&&!out.customGarment&&out.node==ptr(f.node.data())&&out.claimCount==2,"default robe identified without treating body as clothing");
    f.setText(1,"cd_phw_00_ub_00_0170_u");
    check(hex::captureWardrobeSnapshot(ptr(f.actor.data()),3,base,out)&&!out.defaultGarment&&out.customGarment,"barber custom garment distinguished despite original appearance path");
    f.setText(1,"example_ub_0001");
    check(hex::captureWardrobeSnapshot(ptr(f.actor.data()),3,base,out)&&out.customGarment&&!out.defaultGarment,"unregistered replacement remains custom without claiming installation ownership");
    Fixture other;put(f.node.data(),0x50,ptr(other.table.data()));out.node=123;
    check(!hex::captureWardrobeSnapshot(ptr(f.actor.data()),3,base,out)&&out.node==123,"another actor's node rejected and failed capture leaves output unchanged");
    check(hex::wardrobeWitchForActorAppearance(ptr(f.actor.data()),base)<0,"fallback must not claim a foreign renderer");
    put(f.node.data(),0x50,ptr(f.table.data()));put(f.control.data(),8,ptr(other.actor.data()));
    check(!hex::captureWardrobeSnapshot(ptr(f.actor.data()),3,base,out),"wrong actor backlink rejected");
    put(f.control.data(),8,ptr(f.actor.data()));
    f.setText(0,"other/character/appearance/3_npc/cd_npc_south/cd_nhw_south/cd_nhw_south_20168.app_xml");
    check(!hex::captureWardrobeSnapshot(ptr(f.actor.data()),3,base,out),"matching suffix alone cannot identify a witch");
    check(hex::wardrobeWitchForActorAppearance(ptr(f.actor.data()),base)<0,"unrelated paths do not match by suffix");
    constexpr auto addon="character/appearance/3_npc/cd_npc_south/cd_nhw_south/cd_nhw_south_20113.app_xml";
    f.setText(0,addon);
    check(hex::wardrobeWitchForActorAppearance(ptr(f.actor.data()),base)<0,"the built-in fallback does not claim add-on appearances");
    check(!hex::captureWardrobeSnapshot(ptr(f.actor.data()),6,base,out)&&
          !hex::captureWardrobeSnapshot(ptr(f.actor.data()),3,base,out,addon)&&
          hex::captureWardrobeSnapshot(ptr(f.actor.data()),6,base,out,addon)&&out.witch==6,
          "add-on snapshots require an exact configured appearance and a separate target");
    check(!hex::captureWardrobeSnapshot(ptr(f.actor.data()),6,base,out,
          "character/appearance/3_npc/cd_npc_south/cd_nhw_south/cd_nhw_south_20114.app_xml"),
          "one add-on cannot claim another character's renderer");
    check(hex::wardrobeWitchForAppearance("character/appearance/3_npc/cd_npc_south/cd_nhw_south/cd_nhw_south_20166.app_xml")==0,"Lyselia path");
    f.setText(0,"character/appearance/3_npc/cd_npc_south/cd_nhw_south/cd_nhw_south_20166.app_xml");
    put(f.actor.data(),0,base+0x59961d0);
    check(hex::wardrobeWitchForActorAppearance(ptr(f.actor.data()),base)<0,"a player actor cannot be a fallback witch");
    auto companionLayout=hex::layouts[0];
    Block<128> manager{},player{};
    const uintptr_t managerGlobal=ptr(manager.data());
    companionLayout.playerActor=0x59961d0;
    companionLayout.actorManager=0x59c7a08;
    companionLayout.actorManagerGlobal=reinterpret_cast<uintptr_t>(&managerGlobal)-base;
    hex::testGameLayout()=&companionLayout;
    put(manager.data(),0,base+companionLayout.actorManager);
    put(manager.data(),0x50,ptr(player.data()));
    put(player.data(),0,base+companionLayout.playerActor);
    put(identity.data(),8,ptr(f.actor.data()));
    f.setText(0,addon);
    check(hex::readWardrobeCharacterRecord(ptr(f.actor.data()),base,ptr(characterInfo.data()),record)&&
          hex::captureWardrobeSnapshot(ptr(f.actor.data()),6,base,out,addon),
          "a non-player actor of the companion class can provide a checked record and renderer");
    put(manager.data(),0x50,ptr(f.actor.data()));
    check(!hex::readWardrobeCharacterRecord(ptr(f.actor.data()),base,ptr(characterInfo.data()),record)&&
          !hex::captureWardrobeSnapshot(ptr(f.actor.data()),6,base,out,addon),
          "the currently controlled player cannot enter Wardrobe even with a registered appearance");
    hex::testGameLayout()=&hex::layouts[0];
    check(hex::wardrobePartKind(1,"cd_phw_00_ub_00_0157")==hex::WardrobePartKind::DefaultGarment,"alternate Bari default");
    check(hex::wardrobePartKind(1,"phw_00_uw_00_0002")==hex::WardrobePartKind::Underwear&&!hex::defaultOutfitPart(1,"phw_00_uw_00_0002"),"original alternate Bari underwear stays separate from clothing");
    check(hex::defaultOutfitPart(3,"cd_nhw_so_ub_20168")&&hex::defaultOutfitPart(3,"cd_phw_00_hel_00_0186")&&hex::defaultOutfitPart(3,"cd_phw_00_foot_00_0160"),"Hide outfit owns Elowen's exact default robe hat and footwear");
    check(hex::defaultOutfitPart(3,"ha_phw_00_foot_00_0160")&&hex::defaultOutfitPart(2,"ha_phw_00_foot_00_0161_index01")&&hex::defaultOutfitPart(1,"ha_phw_00_foot_00_0163"),"isolated witch footwear retains exact Hide outfit ownership");
    check(!hex::defaultOutfitPart(0,"ha_phw_00_foot_00_0160")&&!hex::defaultOutfitPart(3,"ha_phw_00_foot_00_0163"),"isolated footwear does not grant cross-witch ownership");
    check(!hex::defaultOutfitPart(3,"phw_00_nude_00_0001_damian")&&!hex::defaultOutfitPart(3,"phw_00_head_00_0141")&&!hex::defaultOutfitPart(3,"phw_00_hair_00_0006_04")&&!hex::defaultOutfitPart(3,"example_ub_custom")&&!hex::defaultOutfitPart(2,"cd_phw_00_hel_00_0186"),"Hide outfit excludes body hair head foreign outfits and another witch's accessories");
    Fixture rebuilt;
    put(rebuilt.records[0].data(),0x30,ptr(rebuilt.holder.data()));
    check(hex::captureWardrobeSnapshot(ptr(rebuilt.actor.data()),3,base,out),"capture clothing before an outfit rebuild");
    const auto mask=hex::wardrobeVisibilityClaim(out,out.claims[0]);
    check(hex::hasWardrobeVisibilityClaim(out,mask),"the exact original renderer claim can be restored");
    put(rebuilt.records[0].data(),0x30,ptr(rebuilt.controller.data()));
    check(hex::captureWardrobeSnapshot(ptr(rebuilt.actor.data()),3,base,out),"capture the replacement scene in the same record slot");
    const auto newScene=hex::wardrobeVisibilityClaim(out,out.claims[0]);
    check(!hex::hasWardrobeVisibilityClaim(out,mask)&&!(newScene==mask),
        "reused key and record cannot restore visibility onto a different scene");
    auto carriedMask=mask;
    check(hex::rebindAddonVisibilityClaim(out,carriedMask)&&carriedMask==newScene,
          "add-on renderer retains the sole previously masked prefab key across its scene rebuild");
    auto sharedKey=out;
    sharedKey.claims[sharedKey.claimCount++]=out.claims[0];
    check(!hex::rebindAddonVisibilityClaim(sharedKey,carriedMask),
          "do not restore visibility through an ambiguous renderer key");
    rebuilt.setText(3,"cd_nhw_so_ub_20168");
    put(rebuilt.records[0].data(),0x40,ptr(rebuilt.wrappers[3].data()));
    check(hex::captureWardrobeSnapshot(ptr(rebuilt.actor.data()),3,base,out)&&!hex::hasWardrobeVisibilityClaim(out,newScene),
        "a different outfit wrapper invalidates the old claim even if the prefab name is unchanged");
    check(!hex::rebindAddonVisibilityClaim(out,carriedMask),"add-on visibility cannot cross into a different prefab wrapper");
    const auto newWrapper=hex::wardrobeVisibilityClaim(out,out.claims[0]);
    put(rebuilt.control.data(),0x130,ptr(other.renderer.data()));
    check(hex::captureWardrobeSnapshot(ptr(rebuilt.actor.data()),3,base,out)&&!hex::hasWardrobeVisibilityClaim(out,newWrapper),
        "a new renderer never inherits the old renderer's visibility restoration");
    carriedMask=newWrapper;
    check(!hex::rebindAddonVisibilityClaim(out,carriedMask),"add-on visibility cannot cross into a different renderer");
    check(hex::hasWardrobeVisibilityClaim(out,hex::wardrobeVisibilityClaim(out,out.claims[0])),
        "the rebuilt outfit can take its own visibility mask");
    Fixture spouse;
    check(hex::captureWardrobeSnapshot(ptr(spouse.actor.data()),3,base,out),"spouse probe starts with a validated live actor snapshot");
    put(spouse.control.data(),0xb8,ptr(spouse.holder.data()));
    put(spouse.holder.data(),0x20,ptr(spouse.controller.data()));
    put(spouse.controller.data(),0,base+0x59a9548);
    put(spouse.controller.data(),0x70,ptr(spouse.wrappers[3].data()));
    put(spouse.controller.data(),0xa0,ptr(spouse.mesh.data()));
    put(spouse.controller.data(),0xa8,spouse.mesh.size());
    spouse.mesh.fill(255);
    spouse.mesh[10]=105;
    spouse.mesh[15]=47;
    std::array<uint8_t,16> choices{};
    spouse.setText(3,"character/descriptors/customizationmeta/meshparam_phw.xml");
    check(!hex::captureBarberMeshChoices(out,base,choices)&&choices[10]==0,"an ordinary actor's controller is not read as spouse clothing");
    spouse.setText(3,"character/descriptors/customizationmeta/meshparam_spouse_phw.xml");
    check(hex::captureBarberMeshChoices(out,base,choices)&&choices==spouse.mesh,"verified spouse controller exposes the exact mesh choices");
    put(spouse.controller.data(),0xa8,15);
    check(!hex::captureBarberMeshChoices(out,base,choices)&&choices==spouse.mesh,"a changed mesh layout leaves captured choices untouched");
    put(spouse.controller.data(),0xa8,spouse.mesh.size());
    put(spouse.control.data(),0x130,ptr(other.renderer.data()));
    check(!hex::captureBarberMeshChoices(out,base,choices),"a replaced renderer invalidates the barber observation");
    Fixture bounds;put(bounds.node.data(),0x60,uintptr_t(65)|(uintptr_t(65)<<32));
    check(!hex::captureWardrobeSnapshot(ptr(bounds.actor.data()),3,base,out),"claim cap enforced");
    put(bounds.node.data(),0x60,uintptr_t(2)|(uintptr_t(1)<<32));
    check(!hex::captureWardrobeSnapshot(ptr(bounds.actor.data()),3,base,out),"count beyond capacity rejected");
    check(!hex::captureWardrobeSnapshot(1,3,base,out),"inaccessible actor contained");
    check(!hex::captureWardrobeSnapshot(ptr(bounds.actor.data()),-1,base,out),"all-witches selector cannot be used as actor identity");
    Fixture live;
    check(hex::captureWardrobeSnapshot(ptr(live.actor.data()),3,base,out),"a live cache owner starts with a verified renderer");
    const hex::WardrobeInstance original{out.actor,out.control,out.renderer,out.node,out.witch};
    check(!hex::canDiscardWardrobeInstance(original,base,1,false),"keep a recently seen entry");
    check(hex::canDiscardWardrobeInstance(original,base,30001,false),"an expired entry with no outfit state can be dropped");
    check(!hex::canDiscardWardrobeInstance(original,base,30001,true),"keep a live masked instance even when it is off camera");
    put(live.control.data(),0x130,0);
    check(!hex::canDiscardWardrobeInstance(original,base,30001,true),"a temporarily unavailable renderer does not prove the old instance is gone");
    put(live.control.data(),0x130,ptr(other.renderer.data()));
    check(hex::canDiscardWardrobeInstance(original,base,30001,true),"a validated replacement renderer retires the old mask");
    put(live.control.data(),0x130,ptr(live.renderer.data()));
    std::vector<Fixture> reloads(72);
    for(auto& earlier:reloads){
        check(hex::captureWardrobeSnapshot(ptr(earlier.actor.data()),3,base,out),"capture a separate reload instance");
        const hex::WardrobeInstance saved{out.actor,out.control,out.renderer,out.node,out.witch};
        put(earlier.actor.data(),0,base+0x59c1660);
        check(!hex::canDiscardWardrobeInstance(saved,base,30000,true),"wait for the ownership cache inactivity window");
        check(hex::canDiscardWardrobeInstance(saved,base,30001,true),"a recycled actor does not retain old outfit state at capacity");
    }
    auto* released=VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    check(released!=nullptr,"allocate a test actor page");
    const hex::WardrobeInstance freed{ptr(released),original.control,original.renderer,original.node,3};
    DWORD oldProtection=0;
    check(VirtualProtect(released,4096,PAGE_READWRITE|PAGE_GUARD,&oldProtection)!=0,"guard the test actor page");
    check(!hex::canDiscardWardrobeInstance(freed,base,30001,true),"guarded memory is inconclusive");
    MEMORY_BASIC_INFORMATION page{};
    check(VirtualQuery(released,&page,sizeof page)!=0&&(page.Protect&PAGE_GUARD),"ownership check leaves the guard page intact");
    check(VirtualFree(released,4096,MEM_DECOMMIT)!=0,"decommit the old actor page");
    check(hex::canDiscardWardrobeInstance(freed,base,30001,true),"a decommitted actor has no live renderer mask to restore");
    check(VirtualFree(released,0,MEM_RELEASE)!=0,"release the actor test allocation");
    std::puts("PASS: exact wardrobe instance and visibility ownership, scene/wrapper replacement, and 72 retired outfit owners with live-owner preservation.");
}
