#include "wardrobe_items.h"
#include "mounted_dependencies.h"
#include <algorithm>
#include <cstdio>
#include <filesystem>

int wmain(int argc,wchar_t** argv){
    if(argc!=2)return 1;
    const std::filesystem::path game(argv[1]);
    hex::WardrobeItemCatalog catalog;
    if(!catalog.loadStock(game)){
        std::fputs("FAIL: could not read named wardrobe equipment from the game archives.\n",stderr);
        return 1;
    }
    const auto byId=catalog.find("133184","Feet");
    if(byId.size()!=1||byId.front()->name!="Adeion Leather Boots"||
       byId.front()->parts!=hex::OutfitParts{"cd_phw_00_foot_0042"}||
       byId.front()->icon.size()!=hex::wardrobeIconSize*hex::wardrobeIconSize||
       catalog.find("adeion shoes","Feet")!=byId||catalog.find("133184","Headwear").size()!=0){
        std::fprintf(stderr,"FAIL: stock boots did not resolve with the live name, prefab, icon and slot.\n");
        return 1;
    }
    if(catalog.loadStock(game/L"bin64")||catalog.find("133184","Feet")!=byId){
        std::fputs("FAIL: unavailable game data overwrote previously loaded items.\n",stderr);
        return 1;
    }
    for(const auto id:{"391518545","620002","391518546"}){
        const auto accessories=catalog.find(id,"Accessories");
        if(accessories.size()!=1||accessories.front()->icon.size()!=hex::wardrobeIconSize*hex::wardrobeIconSize){
            std::fprintf(stderr,"FAIL: accessory %s did not load its shared game icon.\n",id);
            return 1;
        }
    }
    const auto manifest=game/L"bin64/dmm/manifest.tsv";
    const auto routes=hex::readMountedRoutes(manifest);
    if(!routes.empty()){
        hex::WardrobeItemCatalog mounted;
        if(!mounted.loadStock(game,manifest)){
            std::fputs("FAIL: active DMM tables could not be read with the game archives.\n",stderr);
            return 1;
        }
        const size_t additions=mounted.addMounted(game/L"bin64","venus",manifest);
        const bool refitOn=std::any_of(routes.begin(),routes.end(),[](const auto& route){
            return route.second.wstring().find(L"Damiane Armor Refit Pack for VENUS BODY")!=std::wstring::npos;
        });
        if(refitOn){
            const auto refits=mounted.find("Damiane Armor Refit Pack",{},4096);
            const auto named=std::count_if(refits.begin(),refits.end(),[](const auto* item){return item->id!=0;});
            const auto icons=std::count_if(refits.begin(),refits.end(),[](const auto* item){return !item->icon.empty();});
            if(named<30||icons<30){
                std::fprintf(stderr,"FAIL: active VENUS replacements returned %zu entries, %td with native IDs and %td with icons.\n",refits.size(),named,icons);
                return 1;
            }
            std::printf("PASS: active VENUS refit pack exposes %zu entries, %td with native names and IDs, %td with icons.\n",
                refits.size(),named,icons);
        }
        if(const auto widow=mounted.find("991103","Feet");!widow.empty()){
            if(widow.size()!=1||widow.front()->name!="Black Widow's Stiletto Boots"||widow.front()->icon.empty()){
                std::fputs("FAIL: mounted Black Widow boots lost their actual mod name, icon or slot.\n",stderr);
                return 1;
            }
            std::puts("PASS: mounted Black Widow boots retain their native mod name, icon and slot.");
        }
        std::printf("PASS: indexed %zu mounted Wardrobe records using the active DMM table and assets.\n",additions);
    }
    std::puts("PASS: find stock items by name, ID and tag with game icons, without a generated item index.");
    return 0;
}
