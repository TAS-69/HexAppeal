#include "shaping.h"
#include "runtime.h"
#include "hex_presets.h"
#include "native_interface.h"
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
    for(const auto& matrix:input) {
        for(float value:matrix)if(!std::isfinite(value)){error="Non-finite stature pose";return false;}
        if(std::fabs(matrix[3])>.0001f||std::fabs(matrix[7])>.0001f||std::fabs(matrix[11])>.0001f||std::fabs(matrix[15]-1.f)>.0001f){error="Unsupported stature matrix";return false;}
    }
    if(&output!=&input)output=input;
    if(amount!=0.f){
        const float scale=1.f+.10f*amount;
        for(auto& matrix:output)
            for(size_t row=0;row<4;++row)
                for(size_t axis=0;axis<3;++axis)matrix[row*4+axis]*=scale;
    }
    return true;
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
template<size_t Length> struct SurgeryEntry {size_t offset;unsigned char bytes[Length];};
constexpr SurgeryEntry<48> observerEntries[]={
    {0x11e90,{0x48,0x89,0x4c,0x24,0x08,0x53,0x48,0x81,0xec,0x90,0x00,0x00,0x00,0x48,0x8b,0xd9,
              0x48,0x8d,0x4c,0x24,0x20,0xe8,0xe6,0x00,0x00,0x00,0x0f,0x10,0x00,0x0f,0x11,0x03,
              0x0f,0x10,0x48,0x10,0x0f,0x11,0x4b,0x10,0x0f,0x10,0x40,0x20,0x0f,0x11,0x43,0x20}},
    {0x18d70,{0x48,0x89,0x5c,0x24,0x20,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,
              0x48,0x8d,0xac,0x24,0x90,0xfb,0xff,0xff,0x48,0x81,0xec,0x70,0x05,0x00,0x00,0x48,
              0x8b,0x05,0xea,0xf8,0x10,0x00,0x48,0x33,0xc4,0x48,0x89,0x85,0x60,0x04,0x00,0x00}}};
constexpr SurgeryEntry<16> shapingEntries[]={
    {0xe960,{0x40,0x55,0x53,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8d,0xac}},
    {0x12a80,{0x48,0x89,0x5c,0x24,0x18,0x55,0x56,0x41,0x57,0x48,0x83,0xec,0x70,0x80,0xb9,0x40}},
    {0x13140,{0x48,0x83,0xec,0x28,0xe8,0xd7,0x59,0x00,0x00,0xeb,0x02,0x33,0xc0,0x48,0x83,0xc4}},
    {0x17d30,{0x48,0x83,0xec,0x58,0x0f,0x29,0x7c,0x24,0x40,0x0f,0x28,0xf9,0x44,0x0f,0x29,0x44}},
    {0x17fb0,{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c,0x24,0x18,0x48,0x89,0x74,0x24,0x20,0x57}}};
template<size_t Length,size_t N>
bool hasSurgeryEntries(HMODULE module,const SurgeryEntry<Length> (&entries)[N],size_t minimumSize){
    MODULEINFO info{};
    if(!module||!GetModuleInformation(GetCurrentProcess(),module,&info,sizeof info)||info.SizeOfImage<minimumSize)return false;
    const auto* image=reinterpret_cast<const unsigned char*>(module);
    for(const auto& entry:entries){
        if(entry.offset+sizeof entry.bytes>info.SizeOfImage)return false;
        const auto* address=image+entry.offset;
        if(!executablePluginFunction(module,address)||!bytesMatch(address,entry.bytes,sizeof entry.bytes))return false;
    }
    return true;
}
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
bool probeSurgeryObserver(HMODULE module,std::string& error){
    if(hasSurgeryEntries(module,observerEntries,0x18d80))return true;
    error="Instant Surgery's actor observer or identity function differs from the checked interface.";
    return false;
}
bool probeSurgeryShaping(HMODULE module,std::string& error){
    if(hasSurgeryEntries(module,shapingEntries,0x12A738))return true;
    error="Instant Surgery's shaping functions differ from the checked interface.";
    return false;
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
    if(!probeSurgeryShaping(module,error))return false;
    base=reinterpret_cast<unsigned char*>(module);
    if(!initialiseMetadataGuard()||!std::isfinite(minimumScale())||minimumScale()<.1f||minimumScale()>=1.f) {
        base=nullptr;error="Instant Surgery shaping metadata is not initialized or has changed.";return false;
    }
    return true;
}
float NativeShapeEngine::minimumScale() const{__try{return base?*reinterpret_cast<const float*>(base+0x129A80):0.f;}__except(EXCEPTION_EXECUTE_HANDLER){return 0.f;}}
bool NativeShapeEngine::readHex(const std::filesystem::path& path,std::string& error,std::string* warning) {
    if(!base){error="The native shaping engine is unavailable.";return false;}
    hasHex=false;std::error_code ec;const auto size=std::filesystem::file_size(path,ec);
    if(ec||size>1024*1024){error="The external preset is unreadable or exceeds the supported 1 MB limit.";return false;}
    SurgeryProfile next=identity();std::ifstream in(path);if(!in){error="The external preset could not be opened.";return false;}
    std::string line,section;
    bool marker=false,first=true,hasShape=false,savedProfile=false,symmetric=true;
    unsigned ignored=0;
    const auto trim=[](const std::string& value){const auto begin=value.find_first_not_of(" \t\r");return begin==std::string::npos?std::string{}:value.substr(begin,value.find_last_not_of(" \t\r")-begin+1);};
    while(std::getline(in,line)) {
        if(first&&line.rfind("\xef\xbb\xbf",0)==0)line.erase(0,3);first=false;line=trim(line);
        if(line.empty()||line[0]=='#'||line[0]==';')continue;
        if(line.front()=='['&&line.back()==']'){
            section=line.substr(1,line.size()-2);
            savedProfile|=section=="General";
            continue;
        }
        const auto eq=line.find('=');if(eq==std::string::npos){error="Malformed Instant Surgery profile.";return false;}
        auto key=trim(line.substr(0,eq));const auto text=trim(line.substr(eq+1));float value;
        if(key=="InstantSurgeryOutfit"){marker=!text.empty();continue;}
        if(section=="General"&&key!="Symmetry")continue;
        if(section=="Shape")key="Shape."+key;
        else if(section=="LeftOffsets")key="Left."+key;
        else if(section=="RightOffsets")key="Right."+key;
        else if(section=="Macros")key="Macro."+key;
        else if(section=="Silhouette")key="Contour."+key;
        float* destination=key=="BreastTeardropCurve"?&next.teardrop:nullptr;
        for(size_t i=0;i<names.size();++i) {
            if(key=="Shape."+names[i]){destination=&next.scale[i];hasShape=true;break;}
            if(key=="Left."+names[i]){destination=&next.left[i];break;}
            if(key=="Right."+names[i]){destination=&next.right[i];break;}
        }
        for(size_t i=0;i<macroCount;++i)if(key=="Macro."+macroNames[i]){destination=&next.macros[i];break;}
        const char* contourNames[]={"RoundedBreastProfile","LowerSeatFullness","HipThighTransition","SoftAbdominalContour"};
        for(size_t i=0;i<4;++i)if(key==std::string("Contour.")+contourNames[i]){destination=&next.contours[i];break;}
        if(!destination&&key!="Symmetry"){++ignored;continue;}
        if(!number(text,value)){error="Invalid preset value: "+key;return false;}
        if(destination)*destination=value;
        else symmetric=value==1;
    }
    if(in.bad()){error="The external preset could not be read completely.";return false;}
    if((!marker&&!savedProfile)||!hasShape){error="An Instant Surgery preset with body shape values is required.";return false;}
    if(!symmetric&&(std::any_of(next.left.begin(),next.left.end(),[](float n){return n!=0.f;})||std::any_of(next.right.begin(),next.right.end(),[](float n){return n!=0.f;}))){
        error="This preset has independent left and right shapes. Choose a symmetric preset for the shape pad.";
        return false;
    }
    std::array<float,64> checked{};
    if(!resolve(next,checked,error))return false;
    if(warning)*warning=ignored?"Preset loaded with "+std::to_string(ignored)+" unrecognised settings skipped.":"";
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
    if(((choice.top==Preset::Hex&&choice.topBlend>0)||(choice.bottom==Preset::Hex&&choice.bottomBlend>0))&&!hasHex){error="The selected preset could not be loaded.";return false;}
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
    auto next=input;
    bool changed=false;
    // The caller or Lock owns the same native lock for this workspace.
    const auto deform=[&]{
        deformationScratch.assign(0x70000,0);
        return deformGuard(base,bones.data(),input.data(),next.data(),int(bones.size()),&shape,deformationScratch.data(),&changed);
    };
    bool success;
    if(callerOwnsSurgeryLock)success=deform();
    else {Lock lock(base);success=deform();}
    if(!success) {error="The native pose calculation faulted.";return false;}
    if(!changed){error="The native pose calculation rejected this rig or profile.";return false;}
    for(const auto& matrix:next)for(float x:matrix)if(!std::isfinite(x)){error="The native pose calculation returned invalid values.";return false;}
    output=std::move(next);return true;
}
}
