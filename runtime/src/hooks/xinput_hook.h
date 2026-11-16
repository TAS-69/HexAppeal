
// MIT License, Copyright (c) 2026 XeTrinityz. See runtime/licenses/UPSTREAM-NOTICES.md.

#pragma once
#include <Windows.h>
#include <Xinput.h>

namespace ml::hooks
{






    void EnsureXInputHooks();






    bool ReadPadRaw(XINPUT_GAMEPAD* out);
}
