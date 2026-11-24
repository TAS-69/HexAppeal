#pragma once
#include "shaping.h"
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
namespace hex {
struct ActorIdentity {int witch=-1;uint32_t recordKey=0;char name[96]{};uintptr_t actor=0;};
struct PosePart {
    uintptr_t part=0;
    uintptr_t skeleton=0;
    std::vector<uintptr_t> renderParts;
    PoseMatrix* destination=nullptr;
    std::vector<ShapeBone> bones;
    std::vector<PoseMatrix> source;
    std::vector<PoseMatrix> animation;
};
bool readWitchIdentity(HMODULE surgery,void* actor,ActorIdentity& identity,int (*addonLookup)(std::string_view)=nullptr);
bool readWardrobeIdentity(void* actor,ActorIdentity& identity);
bool readContextWitchIdentity(HMODULE surgery,void* context,ActorIdentity& identity,int (*addonLookup)(std::string_view)=nullptr);
bool capturePoseParts(void* upload,void* model,std::vector<PosePart>& parts,std::string& error);
bool poseBytesEqual(const PoseMatrix* address,const std::vector<PoseMatrix>& expected);
bool readPoseBytes(const PoseMatrix* address,size_t count,std::vector<PoseMatrix>& output);
bool writePoseBytes(PoseMatrix* address,const std::vector<PoseMatrix>& value);
bool capturePhysicsSource(void* skeleton,const PoseMatrix* source,uint32_t count,
                          std::vector<ShapeBone>& bones,std::vector<PoseMatrix>& pose);
}
