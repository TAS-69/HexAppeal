#pragma once
#include <Windows.h>

struct ImGuiIO;

namespace ml::input
{








    void Init(HWND hwnd);

    void MenuOpened();
    void MenuClosed();
    void InputModeChanged();
    SHORT AsyncKeyRaw(int key);

    void FeedInput(ImGuiIO& io);




    void SetOverlaySize(float w, float h);
}
