#include "wardrobe_snapshot.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void check(bool value,const char* message){if(!value){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
template<size_t N> using Block=std::array<unsigned char,N>;
static void put(void* block,size_t offset,uintptr_t value){std::memcpy(static_cast<unsigned char*>(block)+offset,&value,sizeof value);}
static uintptr_t ptr(void* value){return reinterpret_cast<uintptr_t>(value);}
static constexpr uintptr_t base=0x140000000;
struct Fixture {
    Block<256> actor{},components{};
    Block<512> control{},skinned{},node{};
    Block<32> renderer{},table{},claims{};
    std::array<Block<128>,2> records{};
    std::array<Block<512>,3> wrappers{};
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
    hex::WardrobeSnapshot out;Fixture f;
    check(hex::captureWardrobeSnapshot(ptr(f.actor.data()),3,base,out),"exact actor-renderer-node chain accepted");
    check(out.defaultGarment&&!out.customGarment&&out.node==ptr(f.node.data())&&out.claimCount==2,"default robe identified without treating body as clothing");
    f.setText(1,"cd_phw_00_ub_00_0170_u");
    check(hex::captureWardrobeSnapshot(ptr(f.actor.data()),3,base,out)&&!out.defaultGarment&&out.customGarment,"barber custom garment distinguished despite original appearance path");
    f.setText(1,"cd_phw_00_ub_00_0191_u");
    check(hex::captureWardrobeSnapshot(ptr(f.actor.data()),3,base,out)&&out.dancer&&!out.defaultGarment,"existing Dancer is identified without claiming installation ownership");
    Fixture other;put(f.node.data(),0x50,ptr(other.table.data()));out.node=123;
    check(!hex::captureWardrobeSnapshot(ptr(f.actor.data()),3,base,out)&&out.node==123,"another actor's node rejected and failed capture leaves output unchanged");
    put(f.node.data(),0x50,ptr(f.table.data()));put(f.control.data(),8,ptr(other.actor.data()));
    check(!hex::captureWardrobeSnapshot(ptr(f.actor.data()),3,base,out),"wrong actor backlink rejected");
    put(f.control.data(),8,ptr(f.actor.data()));
    f.setText(0,"other/character/appearance/3_npc/cd_npc_south/cd_nhw_south/cd_nhw_south_20168.app_xml");
    check(!hex::captureWardrobeSnapshot(ptr(f.actor.data()),3,base,out),"matching suffix alone cannot identify a witch");
    check(hex::wardrobeWitchForAppearance("character/appearance/3_npc/cd_npc_south/cd_nhw_south/cd_nhw_south_20166.app_xml")==0,"Lyselia path");
    check(hex::wardrobePartKind(1,"cd_phw_00_ub_00_0157")==hex::WardrobePartKind::DefaultGarment,"alternate Bari default");
    Fixture bounds;put(bounds.node.data(),0x60,uintptr_t(65)|(uintptr_t(65)<<32));
    check(!hex::captureWardrobeSnapshot(ptr(bounds.actor.data()),3,base,out),"claim cap enforced");
    put(bounds.node.data(),0x60,uintptr_t(2)|(uintptr_t(1)<<32));
    check(!hex::captureWardrobeSnapshot(ptr(bounds.actor.data()),3,base,out),"count beyond capacity rejected");
    check(!hex::captureWardrobeSnapshot(1,3,base,out),"inaccessible actor contained");
    check(!hex::captureWardrobeSnapshot(ptr(bounds.actor.data()),-1,base,out),"all-witches selector cannot be used as actor identity");
    std::puts("PASS: exact wardrobe instance ownership, default/custom/Dancer classification, alternate Bari, stale/cross-actor links and bounded invalid reads.");
}
