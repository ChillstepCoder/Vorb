#pragma once

struct SDL_Window;
class IEntityComponentSystem;

#include <Vorb/graphics/GBuffer.h> // TODO: Why forward declare no work

class DebugTweakerPanel
{
public:
    DebugTweakerPanel(const f32v2& screenDims);
    void updateAndRender(IEntityComponentSystem& ecs, const vg::GBuffer* activeGBuffer, float aspectRatio);

private:
    const f32v2 mScreenDims;
};

