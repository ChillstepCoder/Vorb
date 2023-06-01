#pragma once

class MaterialShader;

class OverlayRenderer {
public:
    OverlayRenderer();
    ~OverlayRenderer();

    void renderUnderwaterOverlay();
private:
    const MaterialShader* mColorOverlayShader = nullptr;
    VGVertexArray mVao = 0;
};

