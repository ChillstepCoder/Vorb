#pragma once

struct SDL_Window;
class World;

DECL_VG(class GBuffer);

class DebugTweakerPanel
{
public:
    void updateAndRender(World& world, const vg::GBuffer* activeGBuffer, float ySize, float aspectRatio, f32v3 cameraPos);

private:
};

