#pragma once
#include "HexAppealWardrobe.h"
#include <array>
#include <cstring>
#include <string>
#include <string_view>
#ifdef _WIN32
#include <Windows.h>
#endif

namespace hexappeal {
template<class T>T Sized(){T value{};value.size=sizeof(T);return value;}
template<size_t N>bool Text(char(&output)[N],std::string_view text){
    if(text.size()>=N||text.find('\0')!=std::string_view::npos)return false;
    std::memset(output,0,N);if(!text.empty())std::memcpy(output,text.data(),text.size());return true;
}
class WardrobeClient {
    const HA_WardrobeAPI* api_=nullptr;
    const HA_BaseUnderwearAPI* base_=nullptr;
    const HA_NativeWardrobeAPI* native_=nullptr;
    const HA_OutfitVisibilityAPI* visibility_=nullptr;
    HA_RequestWardrobeRefreshV1 refresh_=nullptr;
    std::string localError_;
    int unavailable(const char* reason){localError_=reason;return HA_UNAVAILABLE;}
public:
    bool Attach(HA_GetWardrobeAPI getAPI,HA_GetBaseUnderwearAPI getBase=nullptr,HA_GetNativeWardrobeAPI getNative=nullptr,HA_RequestWardrobeRefreshV1 refresh=nullptr,HA_GetOutfitVisibilityAPI getVisibility=nullptr){
        Reset();api_=getAPI?getAPI(HA_WARDROBE_ABI):nullptr;
        if(!api_||api_->version!=HA_WARDROBE_ABI||api_->size<sizeof(HA_WardrobeAPI)||!api_->RegisterOutfit||!api_->SetAvailable||!api_->UnregisterOutfit||!api_->SelectOutfit||!api_->GetSelection||!api_->SetControl||!api_->GetControl||!api_->GetLastError){Reset();localError_="Hex Appeal Wardrobe API v1 is unavailable or incomplete.";return false;}
        base_=getBase?getBase(HA_BASE_UNDERWEAR_ABI):nullptr;
        if(base_&&(base_->version!=HA_BASE_UNDERWEAR_ABI||base_->size<sizeof(HA_BaseUnderwearAPI)||!base_->GetState||!base_->SetHidden||!base_->GetEffectiveControl))base_=nullptr;
        native_=getNative?getNative(HA_NATIVE_WARDROBE_ABI):nullptr;
        if(native_&&(native_->version!=HA_NATIVE_WARDROBE_ABI||native_->size<sizeof(HA_NativeWardrobeAPI)||!native_->RegisterOutfit))native_=nullptr;
        visibility_=getVisibility?getVisibility(HA_OUTFIT_VISIBILITY_ABI):nullptr;
        if(visibility_&&(visibility_->version!=HA_OUTFIT_VISIBILITY_ABI||visibility_->size<sizeof(HA_OutfitVisibilityAPI)||!visibility_->GetState||!visibility_->SetHidden))visibility_=nullptr;
        refresh_=refresh;return true;
    }
#ifdef _WIN32
    bool Connect(){
        if(api_)return true;const auto module=GetModuleHandleW(L"HexAppeal.asi");
        if(!module){localError_="Hex Appeal is not loaded yet.";return false;}
        return Attach(reinterpret_cast<HA_GetWardrobeAPI>(GetProcAddress(module,"HexAppeal_GetWardrobeAPI")),
            reinterpret_cast<HA_GetBaseUnderwearAPI>(GetProcAddress(module,"HexAppeal_GetBaseUnderwearAPI")),
            reinterpret_cast<HA_GetNativeWardrobeAPI>(GetProcAddress(module,"HexAppeal_GetNativeWardrobeAPI")),
            reinterpret_cast<HA_RequestWardrobeRefreshV1>(GetProcAddress(module,"HexAppeal_RequestWardrobeRefreshV1")),
            reinterpret_cast<HA_GetOutfitVisibilityAPI>(GetProcAddress(module,"HexAppeal_GetOutfitVisibilityAPI")));
    }
#endif
    void Reset(){api_=nullptr;base_=nullptr;native_=nullptr;visibility_=nullptr;refresh_=nullptr;localError_.clear();}
    bool Connected()const{return api_!=nullptr;}
    bool HasBaseUnderwear()const{return base_!=nullptr;}
    bool HasNativeCombinations()const{return native_!=nullptr;}
    bool HasNativeOutfits()const{return native_!=nullptr;}
    bool HasRefresh()const{return refresh_!=nullptr;}
    bool HasOutfitVisibility()const{return visibility_!=nullptr;}
    int OutfitVisibilityState(int witch,HA_OutfitVisibilityState& value){localError_.clear();value=Sized<HA_OutfitVisibilityState>();return visibility_?visibility_->GetState(witch,&value):unavailable("The Hide outfit extension is unavailable.");}
    int HideOutfit(int witch,bool hidden){localError_.clear();return visibility_?visibility_->SetHidden(witch,hidden?1:0):unavailable("The Hide outfit extension is unavailable.");}
    std::string LastError()const{if(!localError_.empty())return localError_;char text[512]{};if(api_)api_->GetLastError(text,sizeof text);return text;}
    int Register(const HA_Outfit& outfit,uint64_t& token){localError_.clear();if(!api_)return unavailable("Connect to Wardrobe before registering an outfit.");if(token)return unavailable("This outfit is already registered by this client.");uint64_t pending=0;const auto result=api_->RegisterOutfit(&outfit,&pending);if(result==HA_OK)token=pending;return result;}
    int RegisterNative(const HA_NativeOutfit& outfit,uint64_t& token){localError_.clear();if(!native_)return unavailable("This outfit requires the native-outfit extension.");if(token)return unavailable("This outfit is already registered by this client.");uint64_t pending=0;const auto result=native_->RegisterOutfit(&outfit,&pending);if(result==HA_OK)token=pending;return result;}
    int Available(uint64_t token,bool ready,const char* reason=""){localError_.clear();return api_?api_->SetAvailable(token,ready?1:0,reason):unavailable("Wardrobe is not connected.");}
    int Unregister(uint64_t& token){localError_.clear();if(!token)return HA_OK;if(!api_)return unavailable("Reconnect before unregistering this outfit.");const auto result=api_->UnregisterOutfit(token);if(result==HA_OK||result==HA_NOT_FOUND)token=0;return result;}
    int Selection(int witch,HA_Selection& value){localError_.clear();value=Sized<HA_Selection>();return api_?api_->GetSelection(witch,&value):unavailable("Wardrobe is not connected.");}
    int Select(int witch,const char* outfit){localError_.clear();return api_?api_->SelectOutfit(witch,outfit):unavailable("Wardrobe is not connected.");}
    int Stored(int witch,const char* outfit,const char* control,HA_ControlValue& value){localError_.clear();value=Sized<HA_ControlValue>();return api_?api_->GetControl(witch,outfit,control,&value):unavailable("Wardrobe is not connected.");}
    int Effective(int witch,const char* outfit,const char* control,HA_EffectiveControlValue& value){localError_.clear();value=Sized<HA_EffectiveControlValue>();return base_?base_->GetEffectiveControl(witch,outfit,control,&value):unavailable("This outfit requires the Base underwear extension.");}
    int Set(int witch,const char* outfit,const char* control,uint32_t value){localError_.clear();return api_?api_->SetControl(witch,outfit,control,value):unavailable("Wardrobe is not connected.");}
    int BaseState(int witch,HA_BaseUnderwearState& value){localError_.clear();value=Sized<HA_BaseUnderwearState>();return base_?base_->GetState(witch,&value):unavailable("The Base underwear extension is unavailable.");}
    int HideBase(int witch,bool hidden){localError_.clear();return base_?base_->SetHidden(witch,hidden?1:0):unavailable("The Base underwear extension is unavailable.");}
    int Refresh(int witch,const char* outfit){localError_.clear();return refresh_?refresh_(witch,outfit):unavailable("This Hex Appeal build has no refresh extension.");}
};
}
