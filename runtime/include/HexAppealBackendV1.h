#pragma once
#include <stdint.h>

#define HEX_APPEAL_BACKEND_ABI 3u
#define HEX_CAP_NPC_RECORD_SCOPE (1ull << 0)
#define HEX_CAP_INDEPENDENT_REGIONS (1ull << 1)
#define HEX_CAP_VENUS_BASE (1ull << 2)
#define HEX_CAP_HEX_12_RESOLVED (1ull << 3)
#define HEX_CAP_FLAT_BOUNDED (1ull << 4)
#define HEX_CAP_COORDINATED_BODY_OUTFIT (1ull << 5)
#define HEX_CAP_ORIGINAL_ANIMATION_POSE (1ull << 6)
#define HEX_CAP_PROPORTIONS (1ull << 7)

extern "C" {
struct HexAppealProfileV1 {
    uint32_t size;
    uint32_t top;
    uint32_t bottom;
    float topBlend;
    float bottomBlend;
    uint32_t autoFit;
    float height;
    float weight;
};
struct HexAppealTargetV1 {
    uint32_t size;
    uint32_t keyCount;
    uint32_t characterRecordKeys[3];
};
struct HexAppealBackendV1 {
    uint32_t size;
    uint32_t abi;
    uint64_t capabilities;
    const char* name;
    int (*ready)(char* reason, uint32_t capacity);
    int (*begin)(uint32_t targets, uint64_t* transaction);
    int (*prepare)(uint64_t transaction, const HexAppealTargetV1*, const HexAppealProfileV1*);
    int (*commit)(uint64_t transaction);
    void (*abort)(uint64_t transaction);
    int (*poll)(uint64_t transaction, char* reason, uint32_t capacity);
};
typedef const HexAppealBackendV1* (*HexAppealGetBackendV1Fn)();
}
