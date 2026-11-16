#pragma once
#include <imgui.h>
#include <map>
#include <string>
namespace hex::test {
struct Rect{ImVec2 min,max;};
inline std::map<std::string,Rect> controls;
}
