#pragma once
#include "mounted_dependencies.h"
namespace hex {
struct DancerResourceStatus {bool mounted=false,external=false,ready=false,paletteReady=false,verified=true;int colour=-1;std::filesystem::path sourceRoot,texture;std::string detail,paletteDetail,warning;};
DancerResourceStatus inspectDancerResources(const DependencyStatus& dependencies,bool runtimeLoaded);
}
