
// MIT License, Copyright (c) 2026 XeTrinityz. See runtime/licenses/UPSTREAM-NOTICES.md.

#pragma once
#include <Windows.h>

namespace ml::hooks
{


    bool InstallDX12Hooks(HMODULE self, bool useReShadeEvents);




    void LogSwapChainCreationPath();
}
