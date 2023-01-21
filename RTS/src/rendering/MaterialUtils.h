#pragma once
class MaterialShader;
struct LightingOptions;

namespace MaterialUtils {
    void uploadLightingUniforms(const MaterialShader& material);
    void uploadTonemapUniforms(const MaterialShader& material);
    void updateAndRenderLightingControls(ui32& ID, LightingOptions* options, int presetIndex);
};

