#include "pose_observation.h"
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <locale>
namespace hex {
bool savePoseObservation(const std::filesystem::path& folder,const ActorIdentity& identity,
                         const std::vector<PosePart>& parts,uint32_t frame,std::string& error) {
    if(identity.witch<0||identity.witch>=6||parts.empty()||parts.size()>128||frame>=4)return false;
    for(size_t i=0;i<parts.size();++i) {
        const auto& part=parts[i];const auto count=uint32_t(part.bones.size());
        if(!count||count>2048||part.source.size()!=count||part.animation.size()!=count)return false;
        const auto stem=L"witch-"+std::to_wstring(identity.witch)+L"-frame-"+std::to_wstring(frame)+L"-part-"+std::to_wstring(i);
        std::ofstream out(folder/(stem+L".bin"),std::ios::binary|std::ios::trunc);
        if(!out){error="Could not open the pose observation file.";return false;}
        const char magic[8]={'H','X','P','o','s','e','0','2'};
        const uint32_t fields[]={uint32_t(identity.witch),uint32_t(i),frame,count,64};
        const uint64_t tick=GetTickCount64();
        out.write(magic,8);out.write(reinterpret_cast<const char*>(fields),sizeof(fields));out.write(reinterpret_cast<const char*>(&tick),8);
        out.write(reinterpret_cast<const char*>(part.bones.data()),count*sizeof(ShapeBone));
        out.write(reinterpret_cast<const char*>(part.animation.data()),count*sizeof(PoseMatrix));
        out.write(reinterpret_cast<const char*>(part.source.data()),count*sizeof(PoseMatrix));
        out.close();if(!out){error="Could not finish the pose observation file.";return false;}
        size_t changed=0,nonfinite=0;float maximum=0;
        for(size_t b=0;b<count;++b){bool different=false;for(size_t c=0;c<16;++c) {
            const auto a=part.animation[b][c],g=part.source[b][c];
            nonfinite+=!std::isfinite(a)||!std::isfinite(g);
            if(std::isfinite(a)&&std::isfinite(g)){const auto delta=std::abs(a-g);maximum=std::max(maximum,delta);different|=delta>1.e-5f;}
        }changed+=different;}
        std::ofstream note(folder/(stem+L".txt"));note.imbue(std::locale::classic());
        note<<"record="<<identity.name<<"\nrecord_key="<<identity.recordKey<<"\npart="<<part.part
            <<"\nbones="<<count<<"\nchanged_matrices="<<changed<<"\nmax_component_difference="<<std::setprecision(9)<<maximum
            <<"\nnonfinite_components="<<nonfinite<<"\npose_writes=false\n";
    }
    return true;
}
}
