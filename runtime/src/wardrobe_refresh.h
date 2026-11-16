#pragma once
#include <cstdint>
namespace hex {
struct WardrobeRefreshCycle {
    enum Phase {Idle,Original,Selected};
    uint64_t generation=0,restoreAt=0;uint32_t delayMs=0;Phase phase=Idle;bool requested=false;
    void sync(uint64_t value){if(generation!=value){generation=value;phase=Idle;requested=false;restoreAt=0;}}
    void request(uint64_t value,uint64_t now=0,uint32_t delay=0){sync(value);requested=true;delayMs=delay;if(phase==Selected)restoreAt=now+delayMs;}
    bool begin(bool ownedReplacement){if(phase!=Idle||!requested||!ownedReplacement)return false;phase=Original;requested=false;return true;}
    void applyingSelected(){if(phase==Selected)requested=false;}
    bool acknowledged(bool replacementInstalled,uint64_t now=0){if(phase==Original&&!replacementInstalled){phase=Selected;restoreAt=now+delayMs;return true;}if(phase==Selected&&replacementInstalled){phase=Idle;return true;}return false;}
    bool readyToRestore(uint64_t now)const{return phase!=Selected||now>=restoreAt;}
    bool pending()const{return phase!=Idle||requested;}
};
}
