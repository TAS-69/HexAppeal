#include "hooks/input.h"
#include <imgui.h>
#include <iostream>

extern "C" int ModActivatorInputQueueTest();
extern "C" int HexAppealControllerPolicyTest();
int main() {
    ImGui::CreateContext();
    ml::input::MenuOpened();
    ml::input::MenuClosed();
    ml::input::FeedInput(ImGui::GetIO());
    ImGui::DestroyContext();
    const int result=ModActivatorInputQueueTest();
    if(result){std::cerr<<"FAIL: input queue test "<<result<<'\n';return 1;}
    const auto controller=HexAppealControllerPolicyTest();if(controller){std::cerr<<"FAIL: controller policy "<<controller<<'\n';return 1;}
    std::cout<<"PASS: optional blocking for window events, polled keyboard/mouse and controller; own hotkey bypass, pass-through mode, focused text capture, key releases, stale-frame fail-open, input queue and gesture ownership.\nNo game window or renderer was used.\n";
    return 0;
}
