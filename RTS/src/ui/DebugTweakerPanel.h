#pragma once

struct SDL_Window;

DECL_VG(class GBuffer);

class DebugTweakerPanel
{
public:
    void updateAndRender(const vg::GBuffer* activeGBuffer, float ySize, float aspectRatio);

private:
};

