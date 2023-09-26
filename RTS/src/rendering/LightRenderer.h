#pragma once
#include "LightData.h"
#include "resources/asset/AssetHandleBundle.h"

class MaterialShaderDef;
class CubemapDef;

DECL_VG(class GBuffer);

// TODO: IRendererBase?
// TODO: Use point sprite for lights?
class LightRenderer {
public:
    LightRenderer();
    ~LightRenderer();

    void renderSunlight(vg::GBuffer& inputGBuffer, VGTexture shadowTexture, const CubemapDef& skyCubeMap) const;

private:
    AssetHandleBundle mShaderAssets;
    const MaterialShaderDef* mSunlightMaterial = nullptr;
    const MaterialShaderDef* mSunlightMaterialPbr = nullptr;
};

