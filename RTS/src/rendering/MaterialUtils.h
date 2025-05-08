#pragma once
class MaterialShaderDef;
struct LightingOptions;

namespace MaterialUtils {
    void uploadLightingUniforms(const MaterialShaderDef& material);
    void uploadTonemapUniforms(const MaterialShaderDef& material);
    void updateAndRenderLightingControls(ui32& ID, LightingOptions* options, int presetIndex);
};

