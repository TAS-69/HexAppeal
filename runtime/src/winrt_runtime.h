#pragma once
#include <Windows.h>
#include <roapi.h>

namespace hex {
class WinrtApartment {
    HRESULT result=E_FAIL;
public:
    WinrtApartment(){
        static const auto keepActive=[]{
            CO_MTA_USAGE_COOKIE cookie{};
            return CoIncrementMTAUsage(&cookie);
        }();
        if(SUCCEEDED(keepActive))result=RoInitialize(RO_INIT_MULTITHREADED);
    }
    ~WinrtApartment(){if(SUCCEEDED(result))RoUninitialize();}
    explicit operator bool() const{return SUCCEEDED(result)||result==RPC_E_CHANGED_MODE;}
    WinrtApartment(const WinrtApartment&)=delete;
    WinrtApartment& operator=(const WinrtApartment&)=delete;
};
}
