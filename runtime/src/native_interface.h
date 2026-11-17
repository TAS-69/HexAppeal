#pragma once
#include <Windows.h>
#include <string>

namespace hex {
inline bool executablePluginFunction(HMODULE module,const void* address){
    if(!module||!address)return false;
    MEMORY_BASIC_INFORMATION memory{};
    if(!VirtualQuery(address,&memory,sizeof memory)||memory.State!=MEM_COMMIT||(memory.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
    const auto protection=memory.Protect&0xff;
    if(protection!=PAGE_EXECUTE&&protection!=PAGE_EXECUTE_READ&&protection!=PAGE_EXECUTE_READWRITE&&protection!=PAGE_EXECUTE_WRITECOPY)return false;
    HMODULE owner=nullptr;
    return GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(address),&owner)&&owner==module;
}
inline FARPROC pluginExport(HMODULE module,const char* name,std::string& error){
    const auto address=module?GetProcAddress(module,name):nullptr;
    if(!address){error=std::string("Required plugin interface is missing: ")+name;return nullptr;}
    if(!executablePluginFunction(module,reinterpret_cast<const void*>(address))){error=std::string("Plugin interface is not executable inside its loaded module: ")+name;return nullptr;}
    return address;
}
}
