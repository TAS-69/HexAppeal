#include "skin_dds.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace hex {
namespace {
template<class T>
T read(const char* address){
    T value;
    std::memcpy(&value,address,sizeof value);
    return value;
}

struct Blocks {
    unsigned width=0,height=0,stride=0;
    size_t header=0;
    std::string_view data;
};

Blocks textureBlocks(std::string_view data){
    if(data.size()<128||data.substr(0,4)!="DDS "||read<uint32_t>(data.data()+4)!=124||
       read<uint32_t>(data.data()+76)!=32||(read<uint32_t>(data.data()+112)&0x0020fe00u))return {};
    size_t header=128;
    unsigned stride=0;
    const auto encoding=data.substr(84,4);
    if(encoding=="DXT1")stride=8;
    if(encoding=="DXT3"||encoding=="DXT5")stride=16;
    if(encoding=="DX10"&&data.size()>=148&&read<uint32_t>(data.data()+132)==3&&
       !read<uint32_t>(data.data()+136)&&read<uint32_t>(data.data()+140)==1){
        header=148;
        const auto format=read<uint32_t>(data.data()+128);
        if(format==71||format==72)stride=8;
        if(format==74||format==75||format==77||format==78)stride=16;
    }
    const auto width=read<uint32_t>(data.data()+16),height=read<uint32_t>(data.data()+12);
    const auto levels=std::max(1u,read<uint32_t>(data.data()+28));
    if(!stride||!width||!height||width>16384||height>16384||levels>15)return {};
    size_t size=header;
    for(auto w=width,h=height,i=0u;i<levels;++i,w=std::max(1u,w/2),h=std::max(1u,h/2)){
        size+=size_t((w+3)/4)*((h+3)/4)*stride;
        if(w==1&&h==1&&i+1<levels)return {};
    }
    if(size!=data.size())return {};
    return {width,height,stride,header,data.substr(header)};
}

using Colour=std::array<unsigned,3>;
Colour decode565(unsigned word){
    return {(word>>11)*255/31,((word>>5)&63)*255/63,(word&31)*255/31};
}

Colour pixel(const Blocks& map,unsigned x,unsigned y){
    const auto* block=map.data.data()+(size_t(y/4)*((map.width+3)/4)+x/4)*map.stride+map.stride-8;
    const auto first=read<uint16_t>(block),second=read<uint16_t>(block+2);
    const auto index=(read<uint32_t>(block+4)>>((y%4*4+x%4)*2))&3;
    const auto a=decode565(first),b=decode565(second);
    if(index<2)return index?b:a;
    if(map.stride==8&&first<=second){
        if(index==3)return {};
        return {(a[0]+b[0])/2,(a[1]+b[1])/2,(a[2]+b[2])/2};
    }
    const auto weight=4-index;
    return {(a[0]*weight+b[0]*(3-weight))/3,(a[1]*weight+b[1]*(3-weight))/3,
            (a[2]*weight+b[2]*(3-weight))/3};
}

uint16_t scale565(uint16_t word,const SkinGain& gain){
    const auto scale=[](unsigned value,float factor,unsigned maximum){
        return std::min(maximum,static_cast<unsigned>(std::lround(value*factor)));
    };
    return static_cast<uint16_t>(scale(word>>11,gain[0],31)<<11|
                                 scale((word>>5)&63,gain[1],63)<<5|scale(word&31,gain[2],31));
}
}

std::optional<SkinGain> compareSkinColour(std::string_view original,std::string_view installed){
    const auto a=textureBlocks(original),b=textureBlocks(installed);
    if(!a.width||!b.width||uint64_t(a.width)*b.height!=uint64_t(b.width)*a.height)return {};
    std::array<std::vector<float>,3> ratios;
    for(unsigned y=0;y<64;++y)for(unsigned x=0;x<64;++x){
        const auto before=pixel(a,(2*x+1)*a.width/128,(2*y+1)*a.height/128);
        const auto after=pixel(b,(2*x+1)*b.width/128,(2*y+1)*b.height/128);
        if(before[0]<64||before[0]<before[1]||before[1]<before[2])continue;
        for(size_t i=0;i<3;++i)if(before[i]>=32&&after[i]>16&&after[i]<245)
            ratios[i].push_back(float(after[i])/before[i]);
    }
    SkinGain result{};
    for(size_t i=0;i<result.size();++i){
        auto& channel=ratios[i];
        if(channel.size()<128)return {};
        const auto middle=channel.begin()+channel.size()/2;
        std::nth_element(channel.begin(),middle,channel.end());
        const auto gain=*middle;
        const auto agreeing=std::count_if(channel.begin(),channel.end(),[&](float value){return std::abs(value-gain)<0.09f;});
        if(gain<0.45f||gain>1.8f||size_t(agreeing)*4<channel.size()*3)return {};
        result[i]=gain;
    }
    if(std::all_of(result.begin(),result.end(),[](float gain){return std::abs(gain-1.0f)<0.045f;}))return {};
    return result;
}

std::string adjustSkinColour(std::string texture,const SkinGain& gain){
    const auto layout=textureBlocks(texture);
    if(!layout.width)return {};
    std::fill(texture.begin()+32,texture.begin()+76,'\0');
    for(size_t offset=layout.header+layout.stride-8;offset<texture.size();offset+=layout.stride){
        auto* block=texture.data()+offset;
        const auto first=read<uint16_t>(block),second=read<uint16_t>(block+2);
        auto a=scale565(first,gain),b=scale565(second,gain);
        auto indices=read<uint32_t>(block+4);
        if(layout.stride!=8||first>second){
            if(a<b){std::swap(a,b);indices^=0x55555555u;}
            if(layout.stride==8&&a==b){
                if((a&31)<31)++a;
                else --b;
            }
        }else if(a>b){
            std::swap(a,b);
            indices^=~(indices>>1)&0x55555555u;
        }
        std::memcpy(block,&a,sizeof a);
        std::memcpy(block+2,&b,sizeof b);
        std::memcpy(block+4,&indices,sizeof indices);
    }
    return texture;
}
}
