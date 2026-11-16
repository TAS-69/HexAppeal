#pragma once

namespace ml::gui
{

    void InitStyle(float scale);



    bool FontsNeedRebuild();
    void RebuildFonts();


    void SetDisplayHeight(float height);

    void PollToggle();

    bool WantsDraw();



    void FeedInput();

    void Render();
}
