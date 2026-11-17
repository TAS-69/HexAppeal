#pragma once
#include <cstdint>
namespace hex {
struct WardrobeNudeCycle {
    enum Phase {Dressed,Removing,Nude,Restoring,Failed};
    Phase phase=Dressed;uint64_t deadline=0;
    bool pending()const{return phase==Removing||phase==Restoring;}
    void removing(uint64_t now){phase=Removing;deadline=now+5000;}
    void restoring(uint64_t now){phase=Restoring;deadline=now+5000;}
    bool observe(bool anyPresent,bool allPresent,uint64_t now){
        if(phase==Removing&&!anyPresent){phase=Nude;return true;}
        if(phase==Restoring&&allPresent){phase=Dressed;return true;}
        if(pending()&&now>=deadline)phase=Failed;return false;
    }
};
}
