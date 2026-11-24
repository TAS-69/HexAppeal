#pragma once
#include <cstdint>
namespace hex {
struct WardrobeNudeObservation {
    bool acknowledged=false;
    bool matchesRequest=false;
    bool firstDelay=false;
};
struct WardrobeNudeCycle {
    enum Phase {Dressed,Removing,Nude,Restoring};
    Phase phase=Dressed;
    uint64_t deadline=0;
    bool delayReported=false;
    bool pending()const{return phase==Removing||phase==Restoring;}
    bool retainsRemovedOutfit()const{return phase==Nude;}
    void removing(uint64_t now){phase=Removing;deadline=now+5000;delayReported=false;}
    void restoring(uint64_t now){phase=Restoring;deadline=now+5000;delayReported=false;}
    WardrobeNudeObservation observe(bool anyPresent,bool allPresent,bool hideRequested,uint64_t now){
        const bool matchesRequest=hideRequested?!anyPresent:allPresent;
        if(phase==Removing&&!anyPresent){
            phase=Nude;
            return {true,matchesRequest,false};
        }
        if(phase==Restoring&&allPresent){
            phase=Dressed;
            return {true,matchesRequest,false};
        }
        const bool firstDelay=pending()&&now>=deadline&&!delayReported;
        delayReported|=firstDelay;
        return {false,matchesRequest,firstDelay};
    }
};
}
