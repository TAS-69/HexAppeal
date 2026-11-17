#include "shaping.h"
#include "runtime.h"
#include "hex_presets.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <locale>
#include <sstream>
#include <Psapi.h>

namespace hex {
bool applyStature(const Profile& profile,const std::vector<PoseMatrix>& input,std::vector<PoseMatrix>& output,std::string& error) {
    if(!profile.valid()||input.empty()){error="Invalid stature input";return false;}
    const float amount=linkedStature(profile);
    const float axes[]={1.f+.10f*amount,1.f+.10f*amount,1.f+.10f*amount};
    auto next=input;
    for(auto& matrix:next) {
        for(float value:matrix)if(!std::isfinite(value)){error="Non-finite stature pose";return false;}
        if(std::fabs(matrix[3])>.0001f||std::fabs(matrix[7])>.0001f||std::fabs(matrix[11])>.0001f||std::fabs(matrix[15]-1.f)>.0001f){error="Unsupported stature matrix";return false;}
        if(amount!=0.f)for(size_t row=0;row<4;++row)for(size_t axis=0;axis<3;++axis)matrix[row*4+axis]*=axes[axis];
    }
    output=std::move(next);return true;
}
uint64_t statureStamp(uint64_t value,const Profile& profile) {
    const auto amount=linkedStature(profile);if(amount==0.f)return value;
    uint32_t bits=0;memcpy(&bits,&amount,sizeof(bits));
    value^=0x4845585354415455ull;
    for(unsigned i=0;i<4;++i){value^=(bits>>(i*8))&255u;value*=1099511628211ull;}
    return value?value:1;
}
namespace {
struct Lock {
    PSRWLOCK value;
    explicit Lock(unsigned char* base):value(reinterpret_cast<PSRWLOCK>(base+0x12A730)){AcquireSRWLockExclusive(value);}
    ~Lock(){ReleaseSRWLockExclusive(value);}
};
struct NativeVector {const unsigned char* first;const unsigned char* end;const unsigned char* capacity;};
std::string nativeString(const unsigned char* p) {
    const auto size=*reinterpret_cast<const size_t*>(p+16),capacity=*reinterpret_cast<const size_t*>(p+24);
    if(size>128||capacity<size)return {};
    return std::string(capacity>15?*reinterpret_cast<const char*const*>(p):reinterpret_cast<const char*>(p),size);
}
bool number(const std::string& text,float& result){std::istringstream in(text);in.imbue(std::locale::classic());char tail;return bool(in>>result)&&!(in>>tail)&&std::isfinite(result);}
bool bytesMatch(const unsigned char* input,const unsigned char* expected,size_t count){__try{return !std::memcmp(input,expected,count);}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
SurgeryProfile identity(){SurgeryProfile p{};p.scale.fill(1.f);p.symmetry=1;return p;}
bool resolveGuard(unsigned char* base,const SurgeryProfile* p,float* out) {
    __try {
        const auto fn=reinterpret_cast<float(*)(const SurgeryProfile*,int,int)>(base+0x12A80);
        for(int i=0;i<64;++i)out[i]=fn(p,i,0);
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool deformGuard(unsigned char* base,const ShapeBone* bones,const PoseMatrix* src,PoseMatrix* dst,
                 int count,const SurgeryProfile* p,void* scratch,bool* changed) {
    __try {
        *changed=reinterpret_cast<bool(*)(const ShapeBone*,const PoseMatrix*,PoseMatrix*,int,const SurgeryProfile*,void*)>(base+0xE960)(bones,src,dst,count,p,scratch);
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
}
bool NativeShapeEngine::initialiseMetadata() {
    for(size_t i=0;i<names.size();++i) {
        const auto name=*reinterpret_cast<const char*const*>(base+0xC4900+i*72);
        if(!name||strnlen(name,96)>80)return false;
        names[i]=name;
    }
    const auto macros=reinterpret_cast<const NativeVector*>(base+0x129B70);
    const auto bytes=macros->end-macros->first;
    if(bytes<7*88||bytes>16*88||bytes%88)return false;
    macroCount=size_t(bytes/88);
    for(size_t i=0;i<macroCount;++i)macroNames[i]=nativeString(macros->first+i*88);
    return names[8]=="BreastSize"&&names[18]=="ButtockFullness"&&macroNames[0]=="BreastShape"&&macroNames[6]=="LowerCurveRaisedTip";
}
bool NativeShapeEngine::initialiseMetadataGuard(){__try{return initialiseMetadata();}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
bool NativeShapeEngine::bind(HMODULE module,std::string& error) {
    base=nullptr;hasHex=false;
    wchar_t path[32768]{};
    if(!module||!GetModuleFileNameW(module,path,32768)) {
        error="The installed Instant Surgery module could not be found.";return false;
    }
    base=reinterpret_cast<unsigned char*>(module);
    MODULEINFO info{};if(!GetModuleInformation(GetCurrentProcess(),module,&info,sizeof info)||info.SizeOfImage<0x12A738){base=nullptr;error="Instant Surgery's file does not contain the required adapter data.";return false;}
    if(sha256(path)!="92d7cab4e244004ea433d1079c42288a985627fce9cb585844030282e591f009"){
        struct Function {size_t offset;unsigned char bytes[16];};
        const Function functions[]={
            {0xe960,{0x40,0x55,0x53,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8d,0xac}},
            {0x11e90,{0x48,0x89,0x4c,0x24,0x08,0x53,0x48,0x81,0xec,0x90,0x00,0x00,0x00,0x48,0x8b,0xd9}},
            {0x12a80,{0x48,0x89,0x5c,0x24,0x18,0x55,0x56,0x41,0x57,0x48,0x83,0xec,0x70,0x80,0xb9,0x40}},
            {0x13140,{0x48,0x83,0xec,0x28,0xe8,0xd7,0x59,0x00,0x00,0xeb,0x02,0x33,0xc0,0x48,0x83,0xc4}},
            {0x17d30,{0x48,0x83,0xec,0x58,0x0f,0x29,0x7c,0x24,0x40,0x0f,0x28,0xf9,0x44,0x0f,0x29,0x44}},
            {0x17fb0,{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c,0x24,0x18,0x48,0x89,0x74,0x24,0x20,0x57}},
            {0x18d70,{0x48,0x89,0x5c,0x24,0x20,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57}}};
        for(const auto& function:functions)if(!bytesMatch(base+function.offset,function.bytes,sizeof function.bytes)){base=nullptr;error="Instant Surgery is present, but its native interface could not be located by this adapter.";return false;}
    }
    if(!initialiseMetadataGuard()||!std::isfinite(minimumScale())||minimumScale()<.1f||minimumScale()>=1.f) {
        base=nullptr;error="Instant Surgery shaping metadata is not initialized or has changed.";return false;
    }
    return true;
}
float NativeShapeEngine::minimumScale() const{__try{return base?*reinterpret_cast<const float*>(base+0x129A80):0.f;}__except(EXCEPTION_EXECUTE_HANDLER){return 0.f;}}
bool NativeShapeEngine::readHex(const std::filesystem::path& path,std::string& error) {
    if(!base){error="The native shaping engine is unavailable.";return false;}
    hasHex=false;std::error_code ec;const auto size=std::filesystem::file_size(path,ec);
    if(ec||size>1024*1024){error="The external Hex preset is unreadable or exceeds the supported 1 MB limit.";return false;}
    SurgeryProfile next=identity();std::ifstream in(path);if(!in){error="The external Hex preset could not be opened.";return false;}std::string line;bool marker=false,first=true;
    const auto trim=[](const std::string& value){const auto begin=value.find_first_not_of(" \t\r");return begin==std::string::npos?std::string{}:value.substr(begin,value.find_last_not_of(" \t\r")-begin+1);};
    while(std::getline(in,line)) {
        if(first&&line.rfind("\xef\xbb\xbf",0)==0)line.erase(0,3);first=false;line=trim(line);
        if(line.empty()||line[0]=='#'||line[0]==';')continue;
        const auto eq=line.find('=');if(eq==std::string::npos){error="Malformed Hex profile.";return false;}
        const auto key=trim(line.substr(0,eq)),text=trim(line.substr(eq+1));float value;
        if(key=="InstantSurgeryOutfit"){marker=!text.empty();continue;}
        if(!number(text,value)){error="Invalid Hex profile number: "+key;return false;}
        if(key=="Symmetry"){if(value!=1){error="This Hex dependency must use its original symmetry setting.";return false;}continue;}
        if(key=="BreastTeardropCurve"){next.teardrop=value;continue;}
        bool found=false;
        for(size_t i=0;i<names.size();++i) {
            if(key=="Shape."+names[i]){next.scale[i]=value;found=true;break;}
            if(key=="Left."+names[i]){next.left[i]=value;found=true;break;}
            if(key=="Right."+names[i]){next.right[i]=value;found=true;break;}
        }
        for(size_t i=0;i<macroCount;++i)if(key=="Macro."+macroNames[i]){next.macros[i]=value;found=true;break;}
        const char* contourNames[]={"RoundedBreastProfile","LowerSeatFullness","HipThighTransition","SoftAbdominalContour"};
        for(size_t i=0;i<4;++i)if(key==std::string("Contour.")+contourNames[i]){next.contours[i]=value;found=true;break;}
        if(!found){error="Unrecognized Hex profile setting: "+key;return false;}
    }
    if(in.bad()){error="The external Hex preset could not be read completely.";return false;}
    if(!marker){error="The original Hex outfit-profile marker is missing.";return false;}
    originalHex=next;hasHex=true;return true;
}
bool NativeShapeEngine::bottomParameter(size_t i) {
    return (i>=16&&i<=27)||i==42||i==43||i==46||i==47||i==57||i==58;
}
bool NativeShapeEngine::resolve(const SurgeryProfile& shape,std::array<float,64>& percent,std::string& error) {
    if(!base){error="The native shaping engine is unavailable.";return false;}
    Lock lock(base);
    if(!resolveGuard(base,&shape,percent.data())||!std::all_of(percent.begin(),percent.end(),[](float n){return std::isfinite(n)&&n>=-150.001f&&n<=400.001f;})) {
        error="Instant Surgery could not resolve the shape.";return false;
    }
    return true;
}
bool NativeShapeEngine::compose(const Profile& choice,ResolvedShape& output,std::string& error) {
    if(!base||!choice.valid()){error="The shape selection is invalid or the engine is unavailable.";return false;}
    if(((choice.top==Preset::Hex&&choice.topBlend>0)||(choice.bottom==Preset::Hex&&choice.bottomBlend>0))&&!hasHex){error="Hex's required preset has not been read.";return false;}
    std::array<float,64> hex{};
    if(hasHex&&!resolve(originalHex,hex,error))return false;
    ResolvedShape next;next.native=identity();
    const auto convert=reinterpret_cast<float(*)(int,float)>(base+0x17D30);
    Lock lock(base);
    const auto weight=choice.weight;
    const std::array<std::pair<size_t,float>,17> fullness={{{1,8},{2,10},{22,18},{23,6},{24,8},{25,8},{26,10},{27,5},{28,10},{29,4},{30,4},{31,8},{35,2},{40,4},{41,4},{59,4},{60,4}}};
    for(size_t i=0;i<64;++i) {
        const bool bottom=bottomParameter(i);const auto selected=bottom?choice.bottom:choice.top;
        float percent=selected==Preset::Hex?hex[i]:0.f;
        if(selected==Preset::Flat&&(i==8||i==18))percent=-150.f;
        percent*=bottom?choice.bottomBlend:choice.topBlend;
        for(const auto& item:fullness)if(item.first==i)percent=std::clamp(percent+item.second*weight,-150.f,400.f);
        next.percent[i]=percent;next.native.scale[i]=convert(int(i),percent);
        if(!std::isfinite(next.native.scale[i])||next.native.scale[i]<minimumScale()-.00001f){error="Resolved scale is invalid.";return false;}
    }
    output=next;return true;
}
bool NativeShapeEngine::deform(const std::vector<ShapeBone>& bones,const std::vector<PoseMatrix>& input,
        const SurgeryProfile& shape,std::vector<PoseMatrix>& output,std::string& error,bool callerOwnsSurgeryLock) {
    if(!base||bones.empty()||bones.size()!=input.size()||bones.size()>2048){error="Invalid pose input.";return false;}
    for(size_t i=0;i<bones.size();++i) {
        if(bones[i].parent< -1||bones[i].parent>=int(bones.size())||bones[i].parent==int(i)){error="Invalid bone parent.";return false;}
        for(float x:input[i])if(!std::isfinite(x)){error="Non-finite input pose.";return false;}
    }
    auto next=input;std::vector<unsigned char> scratch(0x70000,0);bool changed=false;
    bool success;
    if(callerOwnsSurgeryLock)success=deformGuard(base,bones.data(),input.data(),next.data(),int(bones.size()),&shape,scratch.data(),&changed);
    else {Lock lock(base);success=deformGuard(base,bones.data(),input.data(),next.data(),int(bones.size()),&shape,scratch.data(),&changed);}
    if(!success) {error="The native pose calculation faulted.";return false;}
    if(!changed){error="The native pose calculation rejected this rig or profile.";return false;}
    for(const auto& matrix:next)for(float x:matrix)if(!std::isfinite(x)){error="The native pose calculation returned invalid values.";return false;}
    output=std::move(next);return true;
}
}
