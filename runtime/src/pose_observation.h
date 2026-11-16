#pragma once
#include "pose_access.h"
#include <filesystem>
namespace hex {
bool savePoseObservation(const std::filesystem::path& folder,const ActorIdentity& identity,
                         const std::vector<PosePart>& parts,uint32_t frame,std::string& error);
}
