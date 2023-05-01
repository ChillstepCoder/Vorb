#pragma once

struct SDL_Window;
class IWorld;

DECL_VG(class GBuffer);

class DebugTweakerPanel
{
public:
    void updateAndRender(IWorld& world, const vg::GBuffer* activeGBuffer, float ySize, float aspectRatio);

private:
};

