#pragma once
#include "LightData.h"

class MaterialShader;
class Cubemap;

DECL_VG(class GBuffer);

// TODO: IRendererBase?
// TODO: Use point sprite for lights?
class LightRenderer {
public:
    LightRenderer();
    ~LightRenderer();

    void renderSunlight(vg::GBuffer& inputGBuffer, VGTexture shadowTexture, const Cubemap& skyCubeMap) const;

private:
    const MaterialShader* mSunlightMaterial = nullptr;
    const MaterialShader* mSunlightMaterialPbr = nullptr;
};

