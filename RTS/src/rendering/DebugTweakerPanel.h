#pragma once

struct SDL_Window;

class DebugTweakerPanel
{
public:
    DebugTweakerPanel(const f32v2& screenDims);
    void updateAndRender();

private:
    const f32v2 mScreenDims;
};

