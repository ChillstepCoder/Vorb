#pragma once

DECL_VG(class GBuffer);
class MaterialShader;
class Camera3D;

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
    const MaterialShader* mSmudgeShader = nullptr;
    const MaterialShader* mPaintNoiseShader = nullptr;
};

