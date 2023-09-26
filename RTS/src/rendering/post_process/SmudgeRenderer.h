#pragma once

DECL_VG(class GBuffer);
class MaterialShaderDef;
class Camera3D;

#include "resources/asset/AssetHandleBundle.h"

class SmudgeRenderer {
public:
    SmudgeRenderer(const ui32v2& screenResolution);
    ~SmudgeRenderer();

    void beginSmudgePass(vg::GBuffer* activeGBuffer);
    void renderSmudge(vg::GBuffer* activeGBuffer, const Camera3D& camera);
    void renderPaintNoise(vg::GBuffer* activeGBuffer, const Camera3D& camera);
private:
    // Smudge post process
    std::unique_ptr<vg::GBuffer> mGBuffers[2];
    const MaterialShaderDef* mSmudgeShader = nullptr;
    const MaterialShaderDef* mPaintNoiseShader = nullptr;
    AssetHandleBundle mShaderAssets;
};

