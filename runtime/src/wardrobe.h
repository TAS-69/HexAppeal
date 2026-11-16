#pragma once
#include <Windows.h>
#include <string>
#include "pose_access.h"
namespace hex {
bool initializeWardrobeObservation(std::string& error);
void observeWardrobeActor(void* actor,const ActorIdentity& identity);
struct WardrobeProbeStatus {bool ready=false,pending=false;std::string detail;};
WardrobeProbeStatus wardrobeProbeStatus();
bool requestWardrobeRefresh(int target,std::string& error);
}
