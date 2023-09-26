#pragma once

class MaterialShaderDef;

class OverlayRenderer {
public:
    OverlayRenderer();
    ~OverlayRenderer();

    void renderUnderwaterOverlay();
private:
    AssetHandlePtr<MaterialShaderDef> mColorOverlayShader;
    VGVertexArray mVao = 0;
};

