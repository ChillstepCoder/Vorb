#pragma once

struct SDL_Window;
class EntityComponentSystem;

#include <Vorb/graphics/GBuffer.h> // TODO: Why forward declare no work

class DebugTweakerPanel
{
public:
    DebugTweakerPanel(const f32v2& screenDims);
    void updateAndRender(EntityComponentSystem& ecs, const vg::GBuffer* activeGBuffer, float aspectRatio);

private:
    const f32v2 mScreenDims;
};

