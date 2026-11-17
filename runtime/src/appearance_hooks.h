#pragma once
#include "hooks/dx12/creation_chain.h"
#include <string>

namespace hex::appearance {
inline std::string versionWarning(const char* name,const std::string& digest,const char* verified){
    return digest==verified?std::string{}:std::string("This version of ")+name+" has not been verified. Functionality is not guaranteed. Outfit hooks are checked separately.";
}
inline bool knownJump(void* address,HMODULE first,HMODULE second){
    const auto observed=creation::Observe(address,nullptr);
    if(observed.state!=creation::EntryState::Other||observed.destination==address)return false;
    MEMORY_BASIC_INFORMATION memory{};
    if(!VirtualQuery(observed.destination,&memory,sizeof memory)||memory.State!=MEM_COMMIT||(memory.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
    const auto protection=memory.Protect&0xff;
    if(protection!=PAGE_EXECUTE&&protection!=PAGE_EXECUTE_READ&&protection!=PAGE_EXECUTE_READWRITE&&protection!=PAGE_EXECUTE_WRITECOPY)return false;
    HMODULE owner=nullptr;
    return GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(observed.destination),&owner)&&owner&&((first&&owner==first)||(second&&owner==second));
}
}
