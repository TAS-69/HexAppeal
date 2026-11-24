#include "pose_access.h"
#include "game_layout.h"
#include "model.h"
#include "wardrobe_snapshot.h"
#include "character_addons.h"
#include <Windows.h>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <limits>

namespace hex {
namespace {
struct NativeIdentity {int32_t hero;uint32_t instance;char name[96];uint8_t valid;uint8_t extra[3];};
struct IdentityRow {const char* name;uint32_t key;int witch;};
const IdentityRow identities[]={
    {"NHW_Unique_Modesty_Witch_Store",1002941,0},
    {"NHW_Unique_Goodness_Witch_Store",1002940,1},
    {"NHW_Unique_Goodness_Witch_1",1001374,1},
    {"NHW_Unique_WitchBari_102045",1001091,1},
    {"NHW_Unique_power_Witch_Store",1002938,2},
    {"NHW_Unique_Wisdom_Witch_Store",1002939,3},
    {"NHW_Unique_power_Witch_1",1001369,4},
    {"NHW_WhiteCrow_Witch_6622",6622,5}
};
bool nativeIdentityGuard(HMODULE surgery,void* actor,NativeIdentity* out) {
    __try {
        reinterpret_cast<NativeIdentity*(*)(NativeIdentity*,void*)>(reinterpret_cast<unsigned char*>(surgery)+0x11E90)(out,actor);
        return true;
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
void* contextActorGuard(HMODULE surgery,void* context) {
    __try {return reinterpret_cast<void*(*)(void*)>(reinterpret_cast<unsigned char*>(surgery)+0x13140)(context);}
    __except(EXCEPTION_EXECUTE_HANDLER){return nullptr;}
}
uintptr_t ptr(uintptr_t p,size_t off){return p?*reinterpret_cast<const uintptr_t*>(p+off):0;}
uint32_t word(uintptr_t p,size_t off){return p?*reinterpret_cast<const uint32_t*>(p+off):0;}
bool resolveCharacter(const char* name,uintptr_t actor,ActorIdentity& output,int (*addonLookup)(std::string_view)){
    for(const auto& row:identities)if(_stricmp(name,row.name)==0){
        output={};output.witch=row.witch;output.recordKey=row.key;output.actor=actor;
        strcpy_s(output.name,name);
        return true;
    }
    if(addonLookup){
        const auto target=addonLookup(name);
        if(const auto* addon=characterAddon(target)){
            output={};output.witch=target;output.recordKey=addon->recordKey;output.actor=actor;
            strcpy_s(output.name,name);
            return true;
        }
    }
    return false;
}
bool gameIdentity(void* actor,ActorIdentity& output,int (*addonLookup)(std::string_view)){
    const auto* layout=loadedGameLayout();
    if(!actor||!layout)return false;
    const auto address=reinterpret_cast<uintptr_t>(actor);
    const auto game=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    char name[96]{};
    if(layout->characterInfo&&readWardrobeCharacterRecord(address,game,game+layout->characterInfo,name)&&
       resolveCharacter(name,address,output,addonLookup))return true;
    char appearance[256]{};
    if(!readWardrobeActorAppearance(address,game,appearance))return false;
    const auto witch=wardrobeWitchForAppearance(appearance);
    if(witch>=0&&size_t(witch)<witches.size()){
        output={};output.witch=witch;output.actor=address;
        strcpy_s(output.name,witches[witch].name);
        return true;
    }
    if(addonLookup){
        const auto target=enabledAddonTargetForAppearance(appearance);
        const auto* addon=characterAddon(target);
        if(addon){
            output={};output.witch=target;output.recordKey=addon->recordKey;output.actor=address;
            strncpy_s(output.name,addon->label.c_str(),_TRUNCATE);
            return true;
        }
    }
    return false;
}
bool partCountGuard(void* model,uint32_t* count,uintptr_t* list) {
    __try {*count=word(reinterpret_cast<uintptr_t>(model),0x78);*list=ptr(reinterpret_cast<uintptr_t>(model),0x70);return *list&&*count>0&&*count<=128;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
struct RawPart {uintptr_t part;uintptr_t destination;uintptr_t animation;uintptr_t boneRows;uintptr_t hashRows;uint32_t count;uint32_t hashCount;uintptr_t skeleton;};
int rawPartGuard(void* upload,void* model,uintptr_t list,uint32_t index,RawPart* out) {
    __try {
        const auto part=ptr(list,index*8);if(!part)return 0;
        const auto skeletonRef=ptr(part,0xA8);if(!skeletonRef)return 0;
        const auto flag=*reinterpret_cast<const uint8_t*>(skeletonRef+0x28);if(flag>1)return -1;
        const auto skeleton=ptr(ptr(skeletonRef,0x18+flag*8),0x68);if(!skeleton)return -1;
        const auto count=word(skeleton,0x50),hashCount=word(skeleton,0xB0);
        const auto rows=ptr(skeleton,0x58),hashRows=ptr(skeleton,0xA8);
        if(!count||count>2048||!hashCount||hashCount>4096||!rows||!hashRows)return -1;
        auto record=ptr(ptr(ptr(reinterpret_cast<uintptr_t>(model),0x40),0x30),0);
        const auto meshDescriptorArray=ptr(ptr(record,0x20),0x30);
        const auto meshDescriptorIndex=word(record,0x30);
        if(!meshDescriptorArray||meshDescriptorIndex>0x1000000)return -1;
        const auto meshDescriptor=meshDescriptorArray+size_t(meshDescriptorIndex)*0x80;
        record=ptr(ptr(part,0xB8),0xC0);record=ptr(record,0);
        const auto partDescriptorArray=ptr(ptr(record,0x20),0x30);
        const auto partDescriptorIndex=word(record,0x30);
        if(!partDescriptorArray||partDescriptorIndex>0x1000000)return -1;
        const auto partDescriptor=partDescriptorArray+size_t(partDescriptorIndex)*0x7C;
        const auto stream=ptr(reinterpret_cast<uintptr_t>(upload),0x3E8);
        if(!stream||word(stream,0x10)!=64)return -1;
        const auto bufferIndex=(word(meshDescriptor,0x10)>>20)&15;
        const auto streamArray=ptr(stream,0x28);
        const auto buffer=ptr(ptr(streamArray,bufferIndex*8),0x38);
        const auto a=word(meshDescriptor,0x38),b=word(partDescriptor,0x20);
        if(!buffer||a>0x1000000||b>0x1000000||uint64_t(a)+b>0x1000000)return -1;
        const auto animation=ptr(ptr(skeletonRef,0x90),0);
        if(!animation)return -1;
        *out={part,buffer+(size_t(a)+b)*64,animation,rows,hashRows,count,hashCount,skeleton};
        return 1;
    }__except(EXCEPTION_EXECUTE_HANDLER){return -1;}
}
bool copyGuard(void* dst,const void* src,size_t bytes){__try{memcpy(dst,src,bytes);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
bool physicsRigGuard(void* resource,uint32_t count,RawPart* raw) {
    __try {
        const auto skeleton=reinterpret_cast<uintptr_t>(resource);
        if(!skeleton||count<8||count>2048||word(skeleton,0x50)!=count)return false;
        raw->count=count;raw->hashCount=word(skeleton,0xB0);
        raw->boneRows=ptr(skeleton,0x58);raw->hashRows=ptr(skeleton,0xA8);
        return raw->hashCount&&raw->hashCount<=4096&&raw->boneRows&&raw->hashRows;
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool bonesGuard(const RawPart* raw,ShapeBone* bones) {
    __try {
        for(size_t i=0;i<raw->count;++i)bones[i]={0,*reinterpret_cast<const int32_t*>(raw->boneRows+i*0xB0+0x84)};
        for(size_t i=0;i<raw->hashCount;++i) {
            const auto row=reinterpret_cast<const uint32_t*>(raw->hashRows+i*8);
            if(row[0]>=raw->count)return false;
            bones[row[0]].nameHash=row[1];
        }
        return true;
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool writable(const void* p,size_t bytes) {
    const auto first=reinterpret_cast<uintptr_t>(p);
    if(!first||bytes>std::numeric_limits<uintptr_t>::max()-first)return false;
    auto cursor=first;const auto end=first+bytes;
    while(cursor<end) {
        MEMORY_BASIC_INFORMATION info{};
        if(!VirtualQuery(reinterpret_cast<const void*>(cursor),&info,sizeof(info))||info.State!=MEM_COMMIT||(info.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
        if(!(info.Protect&(PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)))return false;
        const auto next=reinterpret_cast<uintptr_t>(info.BaseAddress)+info.RegionSize;if(next<=cursor)return false;cursor=next;
    }
    return true;
}
}
bool readWitchIdentity(HMODULE surgery,void* actor,ActorIdentity& output,int (*addonLookup)(std::string_view)) {
    if(!actor)return false;
    if(gameIdentity(actor,output,addonLookup))return true;
    NativeIdentity value{};
    const auto address=reinterpret_cast<uintptr_t>(actor);
    return surgery&&nativeIdentityGuard(surgery,actor,&value)&&value.valid&&memchr(value.name,0,sizeof value.name)&&
        resolveCharacter(value.name,address,output,addonLookup);
}
bool readWardrobeIdentity(void* actor,ActorIdentity& output){
    return gameIdentity(actor,output,addonTargetForRecord);
}
bool readContextWitchIdentity(HMODULE surgery,void* context,ActorIdentity& output,int (*addonLookup)(std::string_view)) {
    return surgery&&context&&readWitchIdentity(surgery,contextActorGuard(surgery,context),output,addonLookup);
}
bool capturePoseParts(void* upload,void* model,std::vector<PosePart>& output,std::string& error) {
    uint32_t count=0;uintptr_t list=0;
    if(!partCountGuard(model,&count,&list)){error="The actor's render-part list is unavailable.";return false;}
    std::vector<PosePart> next;
    for(uint32_t i=0;i<count;++i) {
        RawPart raw{};const auto result=rawPartGuard(upload,model,list,i,&raw);
        if(result==0)continue;
        if(result<0){error="An outfit or body part has an unsupported pose layout.";return false;}
        PosePart part;part.part=raw.part;part.skeleton=raw.skeleton;part.renderParts.push_back(raw.part);part.destination=reinterpret_cast<PoseMatrix*>(raw.destination);
        part.bones.resize(raw.count);part.source.resize(raw.count);part.animation.resize(raw.count);
        if(!writable(part.destination,part.source.size()*sizeof(PoseMatrix))||!copyGuard(part.source.data(),part.destination,part.source.size()*sizeof(PoseMatrix))||!copyGuard(part.animation.data(),reinterpret_cast<const void*>(raw.animation),part.animation.size()*sizeof(PoseMatrix))||!bonesGuard(&raw,part.bones.data())) {
            error="The actor's pose buffers could not be captured safely.";return false;
        }
        for(auto& earlier:next)if(earlier.destination==part.destination){
            if(earlier.skeleton!=part.skeleton||earlier.source.size()!=part.source.size()||earlier.animation!=part.animation||earlier.bones.size()!=part.bones.size()||memcmp(earlier.bones.data(),part.bones.data(),part.bones.size()*sizeof(ShapeBone))) {error="Shared pose buffers have different rigs or animation sources.";return false;}
            if(std::find(earlier.renderParts.begin(),earlier.renderParts.end(),part.part)==earlier.renderParts.end())earlier.renderParts.push_back(part.part);
            part.destination=nullptr;break;
        }
        if(part.destination)for(const auto& earlier:next) {
            const auto a=reinterpret_cast<uintptr_t>(earlier.destination),b=reinterpret_cast<uintptr_t>(part.destination);
            if(a<b+part.source.size()*sizeof(PoseMatrix)&&b<a+earlier.source.size()*sizeof(PoseMatrix)) {error="Body/outfit pose buffers overlap.";return false;}
        }
        if(part.destination)next.push_back(std::move(part));
    }
    if(next.empty()){error="No skinned body or outfit parts are available.";return false;}
    output=std::move(next);return true;
}
bool poseBytesEqual(const PoseMatrix* address,const std::vector<PoseMatrix>& expected) {
    if(expected.empty())return true;
    if(!address)return false;
    __try {return memcmp(address,expected.data(),expected.size()*sizeof(PoseMatrix))==0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool readPoseBytes(const PoseMatrix* address,size_t count,std::vector<PoseMatrix>& output) {
    if(!count||count>2048)return false;
    std::vector<PoseMatrix> next(count);if(!copyGuard(next.data(),address,count*sizeof(PoseMatrix)))return false;
    output=std::move(next);return true;
}
bool writePoseBytes(PoseMatrix* address,const std::vector<PoseMatrix>& value) {
    return writable(address,value.size()*sizeof(PoseMatrix))&&copyGuard(address,value.data(),value.size()*sizeof(PoseMatrix));
}
bool capturePhysicsSource(void* skeleton,const PoseMatrix* source,uint32_t count,
                          std::vector<ShapeBone>& bones,std::vector<PoseMatrix>& pose) {
    RawPart raw{};if(!source||!physicsRigGuard(skeleton,count,&raw))return false;
    std::vector<ShapeBone> nextBones(count);std::vector<PoseMatrix> nextPose(count);
    if(!bonesGuard(&raw,nextBones.data())||!copyGuard(nextPose.data(),source,count*sizeof(PoseMatrix)))return false;
    for(const auto& matrix:nextPose)for(float value:matrix)if(!std::isfinite(value))return false;
    bones=std::move(nextBones);pose=std::move(nextPose);return true;
}
}
