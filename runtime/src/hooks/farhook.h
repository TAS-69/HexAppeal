#pragma once
#include <cstdint>




namespace ml::farhook
{





    bool InstallOverAbsJump(const char* name, uintptr_t target, void* detour, void** original, char* why, unsigned whyLen);



    uintptr_t AbsJumpTarget(uintptr_t target);
}
