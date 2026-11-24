#pragma once
#include "hooks/dx12/creation_chain.h"
#include "creator_versions.h"
#include <cwchar>
#include <string>
#include <string_view>
#include <utility>

namespace hex::appearance {
inline std::string versionWarning(const char* name,const std::string& digest){
    const std::string mod=name;
    const bool verified=(mod=="Character Creator"&&verifiedCreatorVersion(digest))||
        (mod=="Spouse Overhaul"&&(digest=="653a650945ee0fcfd5e3c70bf584b27a860bda88098e58e44abd4715b8df4513"||
                                 digest=="d0e4ee5165becebe75bcb24d253de4eef5b8b67ab1444e8420d82b8cc07f75da"));
    if(verified)return {};
    return "This version of "+mod+" has not been verified. Functionality is not guaranteed. Outfit hooks are checked separately.";
}
inline HMODULE jumpImage(void* address){
    const auto observed=creation::Observe(address,nullptr);
    if(observed.state!=creation::EntryState::Other||observed.destination==address)return nullptr;
    MEMORY_BASIC_INFORMATION memory{};
    if(!VirtualQuery(observed.destination,&memory,sizeof memory)||memory.State!=MEM_COMMIT||memory.Type!=MEM_IMAGE||
        (memory.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return nullptr;
    const auto protection=memory.Protect&0xff;
    if(protection!=PAGE_EXECUTE&&protection!=PAGE_EXECUTE_READ&&protection!=PAGE_EXECUTE_READWRITE&&protection!=PAGE_EXECUTE_WRITECOPY)return nullptr;
    return static_cast<HMODULE>(memory.AllocationBase);
}
inline std::string hookOwnerName(void* address){
    const auto owner=jumpImage(address);
    if(!owner||owner==GetModuleHandleW(nullptr))return {};
    wchar_t path[32768]{};
    const auto size=GetModuleFileNameW(owner,path,_countof(path));
    if(!size||size>=_countof(path))return {};
    const auto slash=std::wcsrchr(path,L'\\');
    const auto* file=slash?slash+1:path;
    const std::pair<const wchar_t*,const char*> names[]={
        {L"HeroineOfPywel.asi","Heroine of Pywel"},
        {L"CharacterCreator.asi","Character Creator"},
        {L"SpouseOverhaul.asi","Spouse Overhaul"},
        {L"CrimsonDesertLiveTransmog.asi","Live Transmog"},
        {L"PywelWardrobe.asi","Pywel Wardrobe"},
        {L"FAPS.asi","Framework for Animation and Posing Systems"},
        {L"BodyPhysics.asi","Two Reasons Body Physics"},
        {L"InstantSurgery.asi","Instant Surgery"},
        {L"DmmRuntime.asi","Definitive Mod Manager"}
    };
    for(const auto& [asi,title]:names)
        if(_wcsicmp(asi,file)==0)return std::string(title)+" ("+std::string(asi,asi+std::wcslen(asi))+")";
    char utf8[1024]{};
    if(!WideCharToMultiByte(CP_UTF8,0,file,-1,utf8,sizeof utf8,nullptr,nullptr))return {};
    return utf8;
}
inline std::string entryIssue(void* address,std::string_view operation){
    std::string detail(operation);
    const auto observed=creation::Observe(address,nullptr,true);
    switch(observed.state){
        case creation::EntryState::Unreadable: detail+=" has an unreadable or non-executable entry.";break;
        case creation::EntryState::Cycle: detail+=" has a circular hook chain.";break;
        case creation::EntryState::HopLimit: detail+=" has more redirects than the hook check can follow.";break;
        default: detail+=" failed the live instruction check.";break;
    }
    const auto owner=hookOwnerName(address);
    if(!owner.empty())detail+=" The entry currently routes into "+owner+".";
    else if(observed.destination!=address)detail+=" The hook owner could not be identified.";
    return detail;
}
inline bool knownJump(void* address,HMODULE first,HMODULE second){
    if(!first&&!second)return false;
    const auto owner=jumpImage(address);
    return owner&&(owner==first||owner==second);
}
inline bool knownJumpTo(void* address,const wchar_t* module){
    const auto owner=jumpImage(address);
    if(!owner)return false;
    if(owner==GetModuleHandleW(module))return true;
    wchar_t filename[32768]{};
    const auto size=GetModuleFileNameW(owner,filename,_countof(filename));
    if(!size||size>=_countof(filename))return false;
    const auto leaf=std::wcsrchr(filename,L'\\');
    return _wcsicmp(leaf?leaf+1:filename,module)==0;
}
template<size_t N>
bool checkedEntry(void* address,const unsigned char (&expected)[N],HMODULE first=nullptr,HMODULE second=nullptr){
    unsigned char actual[N]{};
    if(creation::Read(address,actual,N)&&std::memcmp(actual,expected,N)==0)return true;
    return (first||second)&&knownJump(address,first,second);
}
enum class GameEntry { Unavailable, Stock, Detoured };

template<size_t N,size_t M>
GameEntry checkedGameEntry(void* address,const unsigned char (&head)[N],size_t tailOffset,
                           const unsigned char (&tail)[M]){
    if(!creation::ExecutableAddress(address))return GameEntry::Unavailable;
    unsigned char actual[M]{};
    const auto* suffix=static_cast<const unsigned char*>(address)+tailOffset;
    if(!creation::Read(suffix,actual,M)||std::memcmp(tail,actual,M)!=0)return GameEntry::Unavailable;
    if(checkedEntry(address,head))return GameEntry::Stock;
    const auto entry=creation::Observe(address,nullptr,true);
    return entry.state==creation::EntryState::Other&&entry.destination!=address?
        GameEntry::Detoured:GameEntry::Unavailable;
}
}
