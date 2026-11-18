/* Hex Appeal Wardrobe API v1. Original interface copyright TAS-69, 0BSD. */
#ifndef HEX_APPEAL_WARDROBE_H
#define HEX_APPEAL_WARDROBE_H
#include <stdint.h>
#ifdef _WIN32
#define HA_CALL __cdecl
#else
#define HA_CALL
#endif
#ifdef __cplusplus
extern "C" {
#endif
#define HA_WARDROBE_ABI 1u
#define HA_BASE_UNDERWEAR_ABI 1u
#define HA_NATIVE_WARDROBE_ABI 1u
#define HA_OUTFIT_VISIBILITY_ABI 1u
#define HA_OUTFIT_POLICY_ABI 1u
#define HA_WARDROBE_MAX_PREFAB_STATES 256u
#define HA_WARDROBE_MAX_VISIBILITY_PARTS 16u
#define HA_WARDROBE_MAX_CONTROLS 8u
#define HA_WARDROBE_MAX_OPTIONS 16u
enum HA_Result {HA_OK=0,HA_INVALID=1,HA_NOT_READY=2,HA_NOT_FOUND=3,HA_DUPLICATE=4,HA_UNAVAILABLE=5,HA_IO_ERROR=6,HA_LIMIT=7};
enum HA_ControlType {HA_CHOICE=0,HA_TOGGLE=1};
enum HA_ControlScope {HA_PER_WITCH=0,HA_GLOBAL=1};
enum HA_ControlFlags {HA_PREFAB_VARIANT=1,HA_FOLLOW_BASE_UNDERWEAR=2,HA_FOLLOW_OUTFIT_HIDE=4};
enum HA_OutfitPolicyFlags {HA_REPLACE_BASE_UNDERWEAR=1};
#pragma pack(push,8)
typedef struct HA_Option {char id[32];char label[64];char prefab[128];} HA_Option;
typedef struct HA_Control {
    uint32_t size,type,scope,flags,defaultValue,optionCount;
    char id[32],label[64],help[256];
    const HA_Option* options;
} HA_Control;
typedef struct HA_Outfit {
    uint32_t size,controlCount;
    char id[96],label[64],prefab[128],credit[128],requirements[256];
    const HA_Control* controls;
} HA_Outfit;
typedef struct HA_Selection {
    uint32_t size,witch,available;
    uint64_t revision;
    char outfit[96],prefab[128];
} HA_Selection;
typedef struct HA_ControlValue {uint32_t size,value,scope;uint64_t revision;} HA_ControlValue;
typedef struct HA_WardrobeAPI {
    uint32_t size,version;
    int (HA_CALL *RegisterOutfit)(const HA_Outfit*,uint64_t* token);
    int (HA_CALL *SetAvailable)(uint64_t token,uint32_t available,const char* reason);
    int (HA_CALL *UnregisterOutfit)(uint64_t token);
    int (HA_CALL *SelectOutfit)(int32_t witch,const char* outfit);
    int (HA_CALL *GetSelection)(int32_t witch,HA_Selection* selection);
    int (HA_CALL *SetControl)(int32_t witch,const char* outfit,const char* control,uint32_t value);
    int (HA_CALL *GetControl)(int32_t witch,const char* outfit,const char* control,HA_ControlValue* value);
    int (HA_CALL *GetLastError)(char* buffer,uint32_t capacity);
} HA_WardrobeAPI;
typedef struct HA_BaseUnderwearState {
    uint32_t size,hidden,mixed,hiddenMask,nativeReady,nativePending;
    uint64_t revision;
} HA_BaseUnderwearState;
typedef struct HA_EffectiveControlValue {
    uint32_t size,value,storedValue,scope,flags,baseUnderwearHidden;
    uint64_t revision,baseUnderwearRevision;
} HA_EffectiveControlValue;
typedef struct HA_BaseUnderwearAPI {
    uint32_t size,version;
    int (HA_CALL *GetState)(int32_t witch,HA_BaseUnderwearState* state);
    int (HA_CALL *SetHidden)(int32_t witch,uint32_t hidden);
    int (HA_CALL *GetEffectiveControl)(int32_t witch,const char* outfit,const char* control,HA_EffectiveControlValue* value);
} HA_BaseUnderwearAPI;
typedef HA_BaseUnderwearState HA_OutfitVisibilityState;
typedef struct HA_OutfitVisibilityAPI {
    uint32_t size,version;
    int (HA_CALL *GetState)(int32_t witch,HA_OutfitVisibilityState* state);
    int (HA_CALL *SetHidden)(int32_t witch,uint32_t hidden);
} HA_OutfitVisibilityAPI;

typedef struct HA_OutfitPolicyAPI {
    uint32_t size,version;
    int (HA_CALL *SetPolicy)(uint64_t token,uint32_t flags);
} HA_OutfitPolicyAPI;
typedef struct HA_PrefabState {
    uint32_t size,values[HA_WARDROBE_MAX_CONTROLS];
    char prefab[128];
} HA_PrefabState;
typedef struct HA_VisibilityPart {
    uint32_t size;
    char control[32],part[64];
} HA_VisibilityPart;
typedef struct HA_NativeOutfit {
    uint32_t size,controlMask,stateCount,partCount;
    const HA_Outfit* outfit;
    const HA_PrefabState* states;
    const HA_VisibilityPart* parts;
} HA_NativeOutfit;
typedef struct HA_NativeWardrobeAPI {
    uint32_t size,version;
    int (HA_CALL *RegisterOutfit)(const HA_NativeOutfit*,uint64_t* token);
} HA_NativeWardrobeAPI;
#pragma pack(pop)
typedef const HA_WardrobeAPI* (HA_CALL *HA_GetWardrobeAPI)(uint32_t version);
typedef const HA_BaseUnderwearAPI* (HA_CALL *HA_GetBaseUnderwearAPI)(uint32_t version);
typedef const HA_OutfitVisibilityAPI* (HA_CALL *HA_GetOutfitVisibilityAPI)(uint32_t version);
typedef const HA_OutfitPolicyAPI* (HA_CALL *HA_GetOutfitPolicyAPI)(uint32_t version);
typedef const HA_NativeWardrobeAPI* (HA_CALL *HA_GetNativeWardrobeAPI)(uint32_t version);














typedef int (HA_CALL *HA_RequestWardrobeRefreshV1)(int32_t witch,const char* outfit);












#ifdef __cplusplus
}
#endif
#endif
