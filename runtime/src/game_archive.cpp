#include "game_archive.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace hex {
namespace {
using std::string_view;
constexpr size_t maxAsset=32*1024*1024;
struct Cursor {
    string_view bytes;
    size_t at=0;
    string_view take(size_t size){
        if(size>bytes.size()-at)throw std::runtime_error("Truncated character archive index.");
        const auto result=bytes.substr(at,size);
        at+=size;
        return result;
    }
    uint32_t u32(){const auto data=take(4);uint32_t value;std::memcpy(&value,data.data(),4);return value;}
    uint8_t byte(){return static_cast<uint8_t>(take(1)[0]);}
};
using NameTree=std::map<uint32_t,std::string>;
constexpr uint32_t root=UINT32_MAX;

uint32_t parentDistance(Cursor& input){
    uint32_t result=0;
    for(unsigned shift=0;shift<=28;shift+=7){
        const auto byte=input.byte();
        if(shift==28&&(byte&0xf0))break;
        result|=uint32_t(byte&127)<<shift;
        if(!(byte&128))return result;
    }
    throw std::runtime_error("Invalid character archive parent.");
}
NameTree readNames(string_view bytes,bool compact){
    Cursor input{bytes};
    NameTree result;
    while(input.at<bytes.size()){
        const auto node=static_cast<uint32_t>(input.at);
        uint32_t parent=root;
        if(compact){
            const auto distance=parentDistance(input);
            if(distance>node)throw std::runtime_error("Invalid character archive name reference.");
            parent=distance?node-distance:root;
        }else parent=input.u32();
        const auto name=input.take(input.byte());
        std::string prefix;
        if(parent!=root){
            const auto earlier=result.find(parent);
            if(earlier==result.end())throw std::runtime_error("Invalid character archive name tree.");
            prefix=earlier->second;
        }
        if(prefix.size()+name.size()>512)throw std::runtime_error("Character archive name is too long.");
        prefix.append(name);
        result.emplace(node,std::move(prefix));
    }
    return result;
}
std::string load(const std::filesystem::path& path,uint32_t offset,size_t size){
    if(!size||size>maxAsset)return {};
    std::ifstream file(path,std::ios::binary);
    if(!file.seekg(offset))return {};
    std::string data(size,'\0');
    return file.read(data.data(),data.size())?std::move(data):std::string{};
}
std::string lower(std::string value){
    for(auto& ch:value)if(ch>='A'&&ch<='Z')ch+='a'-'A';
    return value;
}
uint32_t rot(uint32_t value,unsigned bits){return value<<bits|value>>(32-bits);}
uint32_t word(string_view value){
    uint32_t result=0;
    std::memcpy(&result,value.data(),value.size());
    return result;
}
uint32_t fileSeed(string_view name){
    uint32_t a=0xdeadbeef+0xc5ede+static_cast<uint32_t>(name.size()),b=a,c=a;
    while(name.size()>12){
        a+=word(name.substr(0,4));b+=word(name.substr(4,4));c+=word(name.substr(8,4));
        a=(a-c)^rot(c,4);c+=b;b=(b-a)^rot(a,6);a+=c;c=(c-b)^rot(b,8);b+=a;
        a=(a-c)^rot(c,16);c+=b;b=(b-a)^rot(a,19);a+=c;c=(c-b)^rot(b,4);b+=a;
        name.remove_prefix(12);
    }
    if(name.empty())return c;
    a+=word(name.substr(0,4));
    if(name.size()>4)b+=word(name.substr(4,4));
    if(name.size()>8)c+=word(name.substr(8,4));
    c=(c^b)-rot(b,14);a=(a^c)-rot(c,11);b=(b^a)-rot(a,25);c=(c^b)-rot(b,16);
    a=(a^c)-rot(c,4);b=(b^a)-rot(a,14);return (c^b)-rot(b,24);
}
void decrypt(std::string& data,string_view name){
    const auto seed=fileSeed(name);
    const auto k=seed^0x60616263;
    std::array<uint32_t,16> state{{0x61707865,0x3320646e,0x79622d32,0x6b206574,
        k,k^0xa0a0a0a,k^0xc0c0c0c,k^0x6060606,k^0xe0e0e0e,k^0xa0a0a0a,k^0x6060606,k^0x2020202,
        seed,seed,seed,seed}};
    for(size_t offset=0;offset<data.size();offset+=64){
        auto block=state;
        const auto quarter=[&](int a,int b,int c,int d){
            block[a]+=block[b];block[d]=rot(block[d]^block[a],16);
            block[c]+=block[d];block[b]=rot(block[b]^block[c],12);
            block[a]+=block[b];block[d]=rot(block[d]^block[a],8);
            block[c]+=block[d];block[b]=rot(block[b]^block[c],7);
        };
        for(int i=0;i<10;++i){
            quarter(0,4,8,12);quarter(1,5,9,13);quarter(2,6,10,14);quarter(3,7,11,15);
            quarter(0,5,10,15);quarter(1,6,11,12);quarter(2,7,8,13);quarter(3,4,9,14);
        }
        for(size_t i=0;i<block.size();++i)block[i]+=state[i];
        const auto* key=reinterpret_cast<const unsigned char*>(block.data());
        for(size_t i=0;i<std::min<size_t>(64,data.size()-offset);++i)data[offset+i]^=key[i];
        if(!++state[12])++state[13];
    }
}
std::string decodeBlock(string_view data,size_t size){
    Cursor input{data};
    std::string result;
    result.reserve(size);
    const auto length=[&](size_t value){
        if(value!=15)return value;
        uint8_t rest;
        do{rest=input.byte();value+=rest;}while(rest==255&&value<=size);
        return value;
    };
    while(input.at<data.size()){
        const auto token=input.byte();
        const auto literals=length(token>>4);
        if(literals>size-result.size())return {};
        result.append(input.take(literals));
        if(input.at==data.size())break;
        const auto offset=word(input.take(2));
        const auto count=length(token&15)+4;
        if(!offset||offset>result.size()||count>size-result.size())return {};
        for(size_t i=0;i<count;++i)result+=result[result.size()-offset];
    }
    return result.size()==size?std::move(result):std::string{};
}
}

uint32_t gameArchiveNameHash(std::string_view name){return fileSeed(name);}

std::string readLooseGameAsset(const std::filesystem::path& path) try{
    std::error_code error;
    const auto size=std::filesystem::file_size(path,error);
    if(error||size>maxAsset)return {};
    auto data=load(path,0,static_cast<size_t>(size));
    if(path.extension()!=L".paloc"||data.compare(0,5,"paloc")!=0)return data;
    constexpr size_t header=512;
    if(data.size()<header)return {};
    const auto packed=word(string_view(data).substr(9,4)),plain=word(string_view(data).substr(13,4));
    if(packed!=data.size()-header||!plain||plain>maxAsset)return {};
    return decodeBlock(string_view(data).substr(header),plain);
}catch(const std::runtime_error&){return {};}

GameArchive::GameArchive(const std::filesystem::path& pathToGroup,std::string_view prefix):directory(pathToGroup){
    const auto path=directory/L"0.pamt";
    std::error_code ec;
    const auto size=std::filesystem::file_size(path,ec);
    const auto bytes=ec||size>maxAsset?std::string{}:load(path,0,static_cast<size_t>(size));
    Cursor input{bytes};
    input.take(4);
    const auto chunks=input.u32();
    if(!chunks||chunks>4096)throw std::runtime_error("Unsupported character archive layout.");
    input.take(8+chunks*12-4);
    const auto folders=input.take(input.u32()),nodes=input.take(input.u32());
    const bool compact=!(folders.empty()?nodes:folders).empty()&&(folders.empty()?nodes:folders).front()==0;
    const auto folderNames=readNames(folders,compact),fileNames=readNames(nodes,compact);
    const auto folderCount=input.u32();
    if(folderCount>bytes.size()/16)throw std::runtime_error("Invalid character archive folder count.");
    Cursor folderRecords{input.take(size_t(folderCount)*16)};
    const auto count=input.u32();
    if(count>bytes.size()/20)throw std::runtime_error("Invalid character archive record count.");
    Cursor files{input.take(size_t(count)*20)};
    std::vector<string_view> extensions;
    if(compact){
        Cursor table{input.take(input.u32())};
        if(input.at!=bytes.size())throw std::runtime_error("Invalid character archive extension table.");
        while(table.at<table.bytes.size()){
            const auto end=table.bytes.find('\0',table.at);
            if(end==string_view::npos)throw std::runtime_error("Invalid character archive extension.");
            extensions.push_back(table.take(end-table.at));
            table.take(1);
        }
    }
    for(uint32_t i=0;i<folderCount;++i){
        folderRecords.u32();
        const auto& folder=folderNames.at(folderRecords.u32());
        const auto first=folderRecords.u32(),amount=folderRecords.u32();
        if(first>count||amount>count-first)throw std::runtime_error("Invalid character archive folder range.");
        if(!prefix.empty()){
            if(folder!=prefix&&(folder.compare(0,prefix.size(),prefix)!=0||folder[prefix.size()]!='/'))continue;
        }else if(folder.find("character/appearance/")!=0&&folder.find("character/descriptors/customization/")!=0&&
           folder!="character/bin__"&&folder!="character/texture"&&
           !(folder.find("character/")==0&&
             ((folder.size()>=10&&folder.substr(folder.size()-10)=="/head/head")||
              (folder.size()>=5&&folder.substr(folder.size()-5)=="/nude"))))
            continue;
        files.at=size_t(first)*20;
        for(uint32_t j=0;j<amount;++j){
            const auto& name=fileNames.at(files.u32());
            const auto offset=files.u32(),stored=files.u32(),unpacked=files.u32(),flags=files.u32();
            const auto suffix=compact?extensions.at(flags>>24):string_view{};
            const auto chunk=flags&65535;
            if(chunk>=4096)continue;
            entries.try_emplace(lower(folder+"/"+name+std::string(suffix)),Entry{chunk,offset,stored,unpacked,(flags>>16)&15});
        }
    }
}
std::vector<std::string_view> GameArchive::routes() const{
    std::vector<std::string_view> result;
    result.reserve(entries.size());
    for(const auto& entry:entries)result.push_back(entry.first);
    return result;
}
std::string GameArchive::read(const std::string& route) const {
    const auto found=entries.find(route);
    if(found==entries.end())return {};
    const auto& entry=found->second;
    if(!entry.size||entry.size>maxAsset)return {};
    auto data=load(directory/(std::to_wstring(entry.chunk)+L".paz"),entry.offset,entry.stored);
    if(data.empty())return {};
    const string_view name=route;
    const bool localization=name.size()>=6&&name.substr(name.size()-6)==".paloc";
    if(localization||(name.size()>=6&&(name.substr(name.size()-4)=="_xml"||name.substr(name.size()-6)==".pappt")))
        decrypt(data,string_view(route).substr(route.find_last_of('/')+1));
    if(localization&&entry.size==entry.stored&&data.compare(0,5,"paloc")==0){
        constexpr size_t header=512;
        if(data.size()<header)return {};
        const auto packed=word(string_view(data).substr(9,4)),plain=word(string_view(data).substr(13,4));
        if(packed!=data.size()-header||!plain||plain>maxAsset)return {};
        return decodeBlock(string_view(data).substr(header),plain);
    }
    if(entry.size==entry.stored||!entry.compression)return data;
    if(entry.compression==2)return decodeBlock(data,entry.size);
    if(entry.compression==1&&data.size()>=128&&data.compare(0,4,"DDS ")==0){
        if(data.compare(84,4,"DXT1")==0){
            uint32_t packed=0,topMip=0;
            std::memcpy(&packed,data.data()+32,4);
            std::memcpy(&topMip,data.data()+20,4);
            if(packed&&packed<topMip&&packed<=data.size()-128&&topMip<=entry.size&&
               data.size()-packed+topMip==entry.size){
                const auto mip=decodeBlock(std::string_view(data).substr(128,packed),topMip);
                if(!mip.empty()){
                    data.replace(128,packed,mip);
                    return data;
                }
            }
        }
        return data.substr(0,std::min<size_t>(148,data.size()));
    }
    return {};
}
}
