#include "model.h"
#include <Windows.h>
#include <cmath>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <locale>
#include <limits>
#include <algorithm>

namespace hex {
bool Profile::valid() const {
    return uint32_t(top)<3 && uint32_t(bottom)<3 && std::isfinite(topBlend) && std::isfinite(bottomBlend) && topBlend>=0 && topBlend<=1 && bottomBlend>=0 && bottomBlend<=1 && std::isfinite(height) && std::isfinite(weight) && std::fabs(height)<=1 && std::fabs(weight)<=1;
}
HexAppealProfileV1 Profile::wire() const { return {sizeof(HexAppealProfileV1),uint32_t(top),uint32_t(bottom),topBlend,bottomBlend,uint32_t(autoFit),height,weight}; }
bool Profile::operator==(const Profile& b) const {return top==b.top && bottom==b.bottom && topBlend==b.topBlend && bottomBlend==b.bottomBlend && autoFit==b.autoFit && height==b.height && weight==b.weight;}
float regionCoordinate(Preset preset,float blend){return preset==Preset::Venus?0.f:preset==Preset::Hex?-blend:blend;}
void setRegionCoordinate(float value,Preset& preset,float& blend){
    if(!std::isfinite(value))return;value=std::clamp(value,-1.f,1.f);
    if(std::fabs(value)<.00001f){preset=Preset::Venus;blend=1.f;}
    else {preset=value<0?Preset::Hex:Preset::Flat;blend=std::fabs(value);}
}
void moveShapePad(Profile& profile,float bottom,float top){setRegionCoordinate(bottom,profile.bottom,profile.bottomBlend);setRegionCoordinate(top,profile.top,profile.topBlend);}
float linkedStature(const Profile& profile){return profile.height;}
void snapShapePad(float& x,float& y,float side,int& detent) {
    if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(side)||side<=0){detent=-1;return;}
    const auto distance=[&](int index){const float dx=(x-float(index%3-1))*side*.5f,dy=(y-float(index/3-1))*side*.5f;return dx*dx+dy*dy;};
    if(detent<0||detent>8||distance(detent)>9.f)detent=-1;
    if(detent<0)for(int i=0;i<9;++i)if(distance(i)<=4.f){detent=i;break;}
    if(detent>=0){x=float(detent%3-1);y=float(detent/3-1);}
}
void Model::selectTarget(int value) {
    if(value < -1 || value >= int(witches.size())) return;
    if(!draftsReady){drafts[0]=saved[0];for(size_t i=0;i<saved.size();++i)drafts[i+1]=saved[i];draftsReady=true;}
    drafts[target+1]=draft;
    target=value;
    draft=drafts[value+1];
    applied=draftIsApplied();
    message=applied?"This combination is active.":draftIsSaved()?"Saved selection loaded. Apply to update the character.":"Unsaved edits restored for this witch.";
}
bool Model::draftIsSaved() const {
    for(size_t i=0;i<saved.size();++i)if((target<0||target==int(i))&&!(draft==saved[i]))return false;
    return true;
}
bool Model::draftIsApplied(uint32_t mask) const {
    bool targeted=false;
    for(size_t i=0;i<saved.size();++i)if((mask&(1u<<i))&&(target<0||target==int(i))){
        targeted=true;
        if(!hasApplied[i]||!(draft==lastApplied[i]))return false;
    }
    return targeted;
}
void Model::restoreSaved(){draft=saved[target<0?0:target];applied=draftIsApplied();message="Saved selection restored to the controls.";}
bool Model::selectCombination(int top,int bottom) {
    if(top<0||top>2||bottom<0||bottom>2) return false;
    draft.top=Preset(top);draft.bottom=Preset(bottom);applied=false;return true;
}
void Model::saveDraft(uint32_t mask) {
    if(!draft.valid()||target < -1||target >= int(witches.size())) return;
    if(target<0)drafts[0]=draft;
    for(size_t i=0;i<saved.size();++i)if((mask&(1u<<i))&&(target<0||target==int(i))){
        saved[i]=draft;
        drafts[i+1]=draft;
    }
    applied=false;
}
bool Model::save(const std::filesystem::path& path,std::string& error) const {
    if(target < -1||target >= int(witches.size())) {error="The selected witch is invalid.";return false;}
    if(!draft.valid()) {error="The selected blend values are invalid.";return false;}
    for(const auto& p:saved) if(!p.valid()) {error="A saved profile is invalid.";return false;}
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(),ec);
    if(ec) {error="Could not create the profile folder.";return false;}
    const auto temp=path.wstring()+L".tmp";
    std::ofstream out(std::filesystem::path(temp),std::ios::binary|std::ios::trunc);
    out.imbue(std::locale::classic());
    if(!out) {error="Could not open the profile file.";return false;}
    out<<"[HexAppeal]\nVersion=2\nTarget="<<target<<"\n";
    for(size_t i=0;i<saved.size();++i) {
        const auto& p=saved[i];
        out<<"\n["<<witches[i].name<<"]\nTop="<<uint32_t(p.top)<<"\nBottom="<<uint32_t(p.bottom)<<"\nTopBlend="<<std::setprecision(std::numeric_limits<float>::max_digits10)<<p.topBlend<<"\nBottomBlend="<<p.bottomBlend<<"\nAutoFit="<<p.autoFit<<"\nHeight="<<p.height<<"\nWeight="<<p.weight<<"\n";
    }
    out.flush();
    if(!out) {error="The profile could not be written.";return false;}
    out.close();
    if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {error="The profile could not be saved atomically.";return false;}
    return true;
}
static bool number(const std::string& input,float& output) {
    std::istringstream stream(input);stream.imbue(std::locale::classic());char tail;
    return bool(stream>>output)&&!(stream>>tail)&&std::isfinite(output);
}
bool Model::load(const std::filesystem::path& path,std::string& error) {
    if(!std::filesystem::exists(path)) return true;
    std::ifstream in(path,std::ios::binary);
    if(!in) {error="Could not read the saved selections.";return false;}
    Model next;
    std::string section,line;
    bool version=false;
    size_t lines=0;
    while(std::getline(in,line)) {
        if(++lines>256||line.size()>512) {error="The profile file is invalid.";return false;}
        if(!line.empty()&&line.back()=='\r')line.pop_back();
        if(line.empty()||line[0]==';')continue;
        if(line.front()=='['&&line.back()==']'){section=line.substr(1,line.size()-2);continue;}
        const auto eq=line.find('=');if(eq==std::string::npos)continue;
        const auto key=line.substr(0,eq),value=line.substr(eq+1);
        float n;
        if(section=="HexAppeal") {
            if(key=="Version") {if(value!="1"&&value!="2"){error="Unsupported profile version.";return false;}version=true;}
            if(key=="Target") {if(!number(value,n)||n!=std::floor(n)||n< -1||n>5){error="Invalid target.";return false;}next.target=int(n);}
            continue;
        }
        for(size_t i=0;i<witches.size();++i)if(section==witches[i].name){
            if(!number(value,n)){error="Invalid profile value.";return false;}
            auto& p=next.saved[i];
            if(key=="Top"||key=="Bottom") {
                if(n<0||n>2||n!=std::floor(n)){error="Invalid preset selection.";return false;}
                (key=="Top"?p.top:p.bottom)=Preset(uint32_t(n));
            } else if(key=="TopBlend")p.topBlend=n;
            else if(key=="BottomBlend")p.bottomBlend=n;
            else if(key=="AutoFit") {if(n!=0&&n!=1){error="Invalid outfit-fit option.";return false;}p.autoFit=n!=0;}
            else if(key=="Height")p.height=n;
            else if(key=="Weight")p.weight=n;
        }
    }
    if(!version) {error="Profile version is missing.";return false;}
    for(const auto& p:next.saved)if(!p.valid()){error="Profile blend is outside 0 to 1.";return false;}
    next.draft=next.saved[next.target<0?0:next.target];
    *this=next;return true;
}
uint64_t requiredCapabilities(const Profile& p) {
    uint64_t c=HEX_CAP_NPC_RECORD_SCOPE|HEX_CAP_INDEPENDENT_REGIONS|HEX_CAP_VENUS_BASE|HEX_CAP_ORIGINAL_ANIMATION_POSE;
    if(p.top==Preset::Hex||p.bottom==Preset::Hex)c|=HEX_CAP_HEX_12_RESOLVED;
    if(p.top==Preset::Flat||p.bottom==Preset::Flat)c|=HEX_CAP_FLAT_BOUNDED;
    if(p.autoFit)c|=HEX_CAP_COORDINATED_BODY_OUTFIT;
    if(p.height!=0||p.weight!=0)c|=HEX_CAP_PROPORTIONS;
    return c;
}
bool validBackend(const HexAppealBackendV1* b) {
    return b&&b->size==sizeof(HexAppealBackendV1)&&b->abi==HEX_APPEAL_BACKEND_ABI&&b->name&&b->ready&&b->begin&&b->prepare&&b->commit&&b->abort&&b->poll;
}
bool Model::apply(const HexAppealBackendV1* backend,std::string& error) {
    const auto before=saved;
    if(target>=-1&&target<int(witches.size())){if(target<0)saved.fill(draft);else saved[target]=draft;}
    const auto mask=target<0?63u:target<6?1u<<target:0u;
    const auto result=applySaved(backend,mask,error);saved=before;return result;
}
bool Model::applySaved(const HexAppealBackendV1* backend,uint32_t mask,std::string& error) {
    if(pendingTransaction){error="Wait for the pending selection to finish.";return false;}
    applied=false;
    if(target < -1 || target >= int(witches.size())){error="Invalid witch selection.";return false;}
    if(!mask||(mask&~63u)){error="Invalid witch mask.";return false;}
    uint64_t required=0;uint32_t count=0;
    for(size_t i=0;i<saved.size();++i)if(mask&(1u<<i)){
        if(!saved[i].valid()||!saved[i].autoFit){error="Invalid shape or outfit fitting disabled.";return false;}
        required|=requiredCapabilities(saved[i]);++count;
    }
    if(!validBackend(backend)){error="NPC shaping is unavailable. No supported fitting adapter is loaded.";return false;}
    if((backend->capabilities&required)!=required){error="The fitting adapter cannot apply this combination to both regions safely.";return false;}
    char reason[256]{};
    if(!backend->ready(reason,sizeof(reason))){reason[255]=0;error=reason[0]?reason:"The fitting adapter is not ready.";return false;}
    uint64_t transaction=0;
    if(!backend->begin(count,&transaction)||!transaction){error="The fitting transaction could not start.";return false;}
    for(size_t i=0;i<witches.size();++i)if(mask&(1u<<i)) {
        const auto p=saved[i].wire();
        if(!backend->prepare(transaction,&witches[i].target,&p)){backend->abort(transaction);error="Body/outfit preparation failed. No selection was applied.";return false;}
    }
    if(!backend->commit(transaction)){
        reason[0]=0;
        const auto rejected=backend->poll(transaction,reason,sizeof(reason))<0;
        reason[255]=0;
        backend->abort(transaction);
        error=rejected&&reason[0]?reason:"The body and outfit update could not start. The previous shape is still active.";
        return false;
    }
    pendingTransaction=transaction;pendingProfile=draft;pendingTarget=target;pendingMask=mask;pendingProfiles=saved;lastApplyFailed=false;message="Updating";poll(backend);return true;
}
void Model::poll(const HexAppealBackendV1* backend) {
    if(!pendingTransaction||!validBackend(backend))return;
    char reason[256]{};const int result=backend->poll(pendingTransaction,reason,sizeof(reason));reason[255]=0;
    if(reason[0])message=reason;
    if(result>0){
        for(size_t i=0;i<saved.size();++i)if(pendingMask&(1u<<i)){lastApplied[i]=pendingProfiles[i];hasApplied[i]=true;}
        applied=draftIsApplied();pendingTransaction=0;
    }
    else if(result<0){applied=false;pendingTransaction=0;lastApplyFailed=true;}
}
}
