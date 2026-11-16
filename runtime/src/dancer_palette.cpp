#include "dancer_palette.h"
#include "runtime.h"
#include <Windows.h>
#include <atomic>
#include <fstream>
#include <cstring>

namespace hex {
namespace {
std::filesystem::path extended(const std::filesystem::path& path){
    auto value=std::filesystem::absolute(path).lexically_normal().wstring();
    if(value.rfind(L"\\\\?\\",0)==0)return value;
    return value.rfind(L"\\\\",0)==0?L"\\\\?\\UNC\\"+value.substr(2):L"\\\\?\\"+value;
}
bool ordinaryParents(std::filesystem::path path){
    while(!path.empty()){
        const auto flags=GetFileAttributesW(extended(path).c_str());
        if(flags==INVALID_FILE_ATTRIBUTES||(flags&FILE_ATTRIBUTE_REPARSE_POINT))return false;
        const auto parent=path.parent_path();if(parent==path)break;path=parent;
    }return true;
}
}
int dancerColourForHash(const std::string& digest){for(size_t i=0;i<dancerColours.size();++i)if(digest==dancerColours[i].sha256)return int(i);return -1;}
bool managedDancerBinding(const std::filesystem::path& texture,std::filesystem::path& root){
    if(texture.empty()||!texture.is_absolute())return false;
    auto candidate=texture.lexically_normal();for(int i=0;i<5;++i)candidate=candidate.parent_path();
    if(candidate.empty()||_wcsicmp((candidate/L"files/0009"/dancerTexture).lexically_normal().c_str(),texture.lexically_normal().c_str()))return false;
    const auto markerPath=extended(candidate/L"HEX-APPEAL-DANCER-BINDINGS.txt");std::error_code ec;
    if(std::filesystem::file_size(markerPath,ec)!=strlen(dancerBindingMarker)||ec)return false;
    std::ifstream input(markerPath,std::ios::binary);
    std::string marker((std::istreambuf_iterator<char>(input)),{});
    if(marker!=dancerBindingMarker||!ordinaryParents(texture.parent_path()))return false;
    root=candidate;return true;
}
bool verifyDancerPalette(const std::filesystem::path& sourceRoot,std::string& error){
    for(const auto& colour:dancerColours)if(!readableDependency(sourceRoot/colour.folder/dancerTexture,"DDS ")){error=std::string("Locate the complete five-colour Sexy Witches download: ")+colour.label;return false;}
    return true;
}
bool changeDancerBinding(const std::filesystem::path& texture,const std::filesystem::path& sourceRoot,int colour,std::string& error){
    if(colour<0||size_t(colour)>=dancerColours.size()){error="Choose a supplied Dancer colour.";return false;}
    std::filesystem::path root;if(!managedDancerBinding(texture,root)){error="Global colours need a managed external Dancer binding.";return false;}
    const auto before=sha256(texture);if(!readableDependency(texture,"DDS ")){error="The active texture path is missing, unreadable or not a DDS file.";return false;}
    if(!verifyDancerPalette(sourceRoot,error))return false;
    const auto source=sourceRoot/dancerColours[colour].folder/dancerTexture;
    const auto chosenHash=sha256(source);
    if(!ordinaryParents(source)){error="The source uses a redirected path; choose an ordinary extracted dependency folder.";return false;}
    if(before==chosenHash)return true;
    static std::atomic<unsigned> serial{0};
    const auto temporary=texture.parent_path()/(L".HexAppeal-colour-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(++serial)+L".dds");
    const auto tempPath=extended(temporary),sourcePath=extended(source),targetPath=extended(texture);
    if(!CreateHardLinkW(tempPath.c_str(),sourcePath.c_str(),nullptr)){error="The colour binding could not be prepared (same drive and write access required).";return false;}


    if(sha256(temporary)!=chosenHash||sha256(source)!=chosenHash||sha256(texture)!=before){DeleteFileW(tempPath.c_str());error="A colour resource changed during preparation; no binding was replaced.";return false;}
    if(!MoveFileExW(tempPath.c_str(),targetPath.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){DeleteFileW(tempPath.c_str());error="The colour is in use or the binding is not writable. Close the game and choose it through the preparation tool.";return false;}
    if(sha256(texture)!=chosenHash){error="The new colour binding could not be verified.";return false;}
    return true;
}
}
