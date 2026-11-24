#pragma once
#include <Windows.h>
#include <string>
#include "pose_access.h"
namespace hex {
bool initializeWardrobeObservation(std::string& error);
void startWardrobeRuntime();
bool outfitRangeAvailable();
struct WardrobeProbeStatus {bool ready=false,pending=false;std::string detail,versionWarning;};
WardrobeProbeStatus wardrobeProbeStatus();
bool requestWardrobeRefresh(int target,std::string& error);
}
