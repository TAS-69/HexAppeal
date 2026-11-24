#include "spouse_wardrobe.h"
#include <Windows.h>
#include <roapi.h>
#include "winrt_runtime.h"
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <fstream>

namespace hex {
namespace {
using namespace winrt::Windows::Data::Json;

bool meshChoices(std::wstring_view mesh,std::array<uint8_t,16>& choices){
    if(mesh.size()!=2*choices.size())return false;
    const auto digit=[](wchar_t c) {
        if(c>=L'0'&&c<=L'9')return int(c-L'0');
        if(c>=L'a'&&c<=L'f')return int(c-L'a'+10);
        if(c>=L'A'&&c<=L'F')return int(c-L'A'+10);
        return -1;
    };
    for(size_t i=0;i<choices.size();++i){
        const auto high=digit(mesh[2*i]),low=digit(mesh[2*i+1]);
        if(high<0||low<0)return false;
        choices[i]=uint8_t((high<<4)|low);
    }
    return true;
}

bool readParts(const JsonObject& row,const BarberCatalog& catalog,OutfitSlots& result){
    const auto outfit=row.GetNamedObject(L"outfit",nullptr);
    if(!outfit||!outfit.Size()||outfit.Size()>16)return false;
    for(const auto& item:outfit){
        const auto key=item.Key();
        if(key.empty()||key.size()>2)return false;
        size_t index=0;
        for(const auto c:key){
            if(c<L'0'||c>L'9')return false;
            index=10*index+size_t(c-L'0');
        }
        if(!catalog.isArmorSlot(index)||item.Value().ValueType()!=JsonValueType::String)return false;
        const auto part=winrt::to_string(item.Value().GetString());
        if(!validOutfitPart(part)||protectedOutfitPart(part)||!catalog.has(part))return false;
        const auto slot=std::string("Barber")+(index<10?"0":"")+std::to_string(index);
        if(!result.emplace(slot,OutfitParts{part}).second)return false;
    }
    return true;
}
unsigned logNpc(const std::string& row,size_t start){
    const auto end=row.find_first_of(" \r",start);
    const auto stop=end==row.npos?row.size():end;
    if(stop<=start||stop-start>5)return 0;
    unsigned result=0;
    for(size_t i=start;i<stop;++i){
        if(row[i]<'0'||row[i]>'9')return 0;
        result=result*10+unsigned(row[i]-'0');
    }
    return result<=UINT16_MAX?result:0;
}
std::optional<std::array<uint8_t,16>> savedChoices(const std::string& row,size_t start){
    const auto field=row.find(" outfit=",start);
    if(field==row.npos)return {};
    std::string_view value(row.data()+field+8,row.size()-field-8);
    while(!value.empty()&&(value.back()=='\r'||value.back()==' '))value.remove_suffix(1);
    if(value.size()!=31||value.substr(23)!=" / FF FF")return {};
    std::wstring mesh(16,L'F');
    for(size_t i=0;i<8;++i){
        if(i&&value[3*i-1]!=' ')return {};
        mesh+=wchar_t(value[3*i]);
        mesh+=wchar_t(value[3*i+1]);
    }
    std::array<uint8_t,16> choices{};
    if(!meshChoices(mesh,choices))return {};
    return choices;
}
}

bool SpouseBarberSource::refresh(const std::filesystem::path& path,const BarberCatalog& catalog){
    std::error_code ec;
    const auto time=std::filesystem::last_write_time(path,ec);
    if(ec)return false;
    const auto size=std::filesystem::file_size(path,ec);
    if(ec||!size||size>1024*1024)return false;
    if(path==lastPath&&time==lastWrite&&size==lastSize)return true;
    std::ifstream file(path,std::ios::binary);
    std::string contents(size,'\0');
    if(!file.read(contents.data(),contents.size())||file.peek()!=EOF)return false;
    if(std::filesystem::last_write_time(path,ec)!=time||ec)return false;
    if(contents.compare(0,3,"\xef\xbb\xbf")==0)contents.erase(0,3);

    const WinrtApartment apartment;
    if(!apartment)return false;

    try {
        JsonObject root(nullptr);
        if(!JsonObject::TryParse(winrt::to_hstring(contents),root)||root.GetNamedNumber(L"version",-1)<1)return false;
        const auto slots=root.GetNamedObject(L"slots",nullptr);
        if(!slots||slots.Size()>1024)return false;
        std::vector<Look> next;
        for(const auto& save:slots)try{
            const auto sections=save.Value().GetObject().GetNamedObject(L"sections",nullptr);
            if(!sections)continue;
            const auto saved=sections.GetNamedObject(L"spouse_looks",nullptr);
            if(!saved||saved.GetNamedNumber(L"v",-1)!=1)continue;
            const auto rows=saved.GetNamedArray(L"rows",nullptr);
            if(!rows||rows.Size()>256)continue;
            for(const auto& found:rows)try{
                const auto row=found.GetObject();
                Look look;
                const auto character=row.GetNamedNumber(L"character",-1),npc=row.GetNamedNumber(L"npc",-1);
                if(character<0||character>15||npc<=0||npc>65535||int(character)!=character||int(npc)!=npc||
                   row.GetNamedNumber(L"version",-1)!=1||
                   !meshChoices(row.GetNamedString(L"mesh",L""),look.identity)||!readParts(row,catalog,look.slots))continue;
                look.owner=std::to_string(int(character))+"/"+std::to_string(int(npc));
                look.npc=unsigned(npc);
                look.save=winrt::to_string(save.Key());
                if(next.size()>=4096)return false;
                next.push_back(std::move(look));
            }catch(const winrt::hresult_error&){}
        }catch(const winrt::hresult_error&){}
        looks=std::move(next);
        lastPath=path;
        lastWrite=time;
        lastSize=size;
        return true;
    }catch(const winrt::hresult_error&){return false;}
}

unsigned SpouseBarberSource::find(const std::string& save,const std::array<uint8_t,16>& identity,const BarberCatalog& catalog,OutfitSlots& outfit) const{
    if(save.empty())return 0;
    const Look* choice=nullptr;
    for(const auto& look:looks){
        if(look.save!=save||!catalog.matchesIdentity(look.identity,identity))continue;
        if(choice&&(choice->owner!=look.owner||choice->slots!=look.slots))return 0;
        choice=&look;
    }
    if(!choice)return 0;
    outfit=choice->slots;
    return choice->npc;
}

void SpouseGearHandoff::observe(uint64_t now,unsigned nextRevision){
    if(revision==nextRevision)return;
    active=nextRevision!=0;
    began=now;
    revision=nextRevision;
}

bool SpouseEditorHandoff::reconcile(SpouseWardrobeState& state,bool nativeWardrobeOpened){
    if(nativeWardrobeOpened&&state.editorOpen())superseded=state.editorRevision;
    if(!superseded||state.editorRevision!=superseded||!state.editorOpen())return false;
    state.barberOpen=false;
    state.combatEditorOpen=false;
    state.combatEditorNpc=0;
    return true;
}

SpouseWardrobeState activeSpouseWardrobe(const std::filesystem::path& log){
    WIN32_FILE_ATTRIBUTE_DATA stat{};
    FILETIME started{},exit{},kernel{},user{};
    if(!GetFileAttributesExW(log.c_str(),GetFileExInfoStandard,&stat)||
       !GetProcessTimes(GetCurrentProcess(),&started,&exit,&kernel,&user)||
       CompareFileTime(&stat.ftLastWriteTime,&started)<0||
       stat.nFileSizeHigh||stat.nFileSizeLow>2*1024*1024)return {};
    std::ifstream input(log,std::ios::binary);
    if(!input)return {};
    std::string row;
    SpouseWardrobeState result;
    uint64_t editorRevision=0;
    constexpr std::string_view loadedSlot="data: slot loaded slot=";
    constexpr std::string_view restoredSlot="looks: saved looks put back after load slot=";
    const auto saveBoundary=[&result](std::string slot){
        const auto epoch=result.saveEpoch+1;
        result={};
        result.save=std::move(slot);
        result.saveEpoch=epoch;
    };
    bool startedSession=false;
    while(std::getline(input,row)){
        if(row.find("core: ini loaded ")!=row.npos){startedSession=true;result={};}
        if(!startedSession)continue;
        if(row.find("data: records not applied (")!=row.npos){saveBoundary({});continue;}
        if(row.find("barber: opened for spouse")!=row.npos){
            result.barberOpen=true;
            result.combatEditorOpen=false;
            result.combatEditorNpc=0;
            result.editorRevision=++editorRevision;
        }
        else if(row.find("barber: closed")!=row.npos)result.barberOpen=false;
        if(row.find("gear: screen opened for non-spouse")!=row.npos){
            result.combatEditorOpen=false;
            result.combatEditorNpc=0;
        }else if(row.find("gear: screen opened")!=row.npos){
            const auto npc=row.find(" npc=");
            result.combatEditorNpc=npc==row.npos?0:logNpc(row,npc+5);
            result.combatEditorOpen=result.combatEditorNpc!=0;
            if(result.combatEditorOpen){
                result.barberOpen=false;
                result.editorRevision=++editorRevision;
            }
        }else if(row.find("gear: screen closed")!=row.npos&&row.find("gear: screen closed for non-spouse")==row.npos){
            if(result.combatEditorNpc)++result.gearRevision[result.combatEditorNpc];
            result.combatEditorOpen=false;
            result.combatEditorNpc=0;
        }
        if(row.find("data: slot loaded, no mod records slot=")!=row.npos){saveBoundary({});continue;}
        constexpr std::string_view barberEdit="barber: look saved npc=";
        const auto edit=row.find(barberEdit);
        if(edit!=row.npos){
            const auto start=edit+barberEdit.size();
            if(const auto npc=logNpc(row,start)){
                auto& saved=result.barberEdits[npc];
                ++saved.revision;
                saved.choices=savedChoices(row,start);
            }
        }
        for(const auto signal:{loadedSlot,restoredSlot}){
            const auto at=row.find(signal);
            if(at==row.npos)continue;
            const auto first=at+signal.size();
            const auto last=row.find(' ',first);
            const auto id=row.substr(first,last==row.npos?row.npos:last-first);
            if(!id.empty()&&id.size()<=5&&id.find_first_not_of("0123456789")==id.npos){
                if(signal==loadedSlot||result.save!=id)saveBoundary(id);
            }else saveBoundary({});
        }
        if(!result.save.empty()&&(row.find("gear: Set Combat Gear pressed")!=row.npos||
            row.find("gear: always wear on")!=row.npos||row.find("gear: always wear off")!=row.npos)){
            const auto key=row.find(" npc=");
            if(key!=row.npos)if(const auto npc=logNpc(row,key+5))++result.gearRevision[npc];
        }
        constexpr std::string_view gear="gear: look set npc=";
        const auto found=row.find(gear);
        if(result.save.empty()||found==row.npos||row.find(" err=0")==row.npos)continue;
        const auto id=logNpc(row,found+gear.size());
        if(!id)continue;
        if(row.find(" state=always ")!=row.npos||row.find(" state=fight ")!=row.npos||
           row.find(" state=combat ")!=row.npos){
            if(result.combat.insert(id).second)++result.gearRevision[id];
        }else if(row.find(" state=no-fight ")!=row.npos){
            if(result.combat.erase(id))++result.gearRevision[id];
        }
    }
    return input.bad()?SpouseWardrobeState{}:result;
}
}
