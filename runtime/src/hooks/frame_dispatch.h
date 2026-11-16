#pragma once
#include <cstdint>
#include <string>
namespace hex {
using FrameJob=void(*)();
bool initializeFrameDispatch(FrameJob callback,std::string& error);
uint32_t frameThreadId();
}
