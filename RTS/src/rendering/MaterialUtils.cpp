#include "stdafx.h"
#include "MaterialUtils.h"

#include "options/DebugOptions.h"
#include "MaterialShader.h"

#include "options/LightingOptions.h"

#include <Vorb/ui/imgui/imgui.h>
#include <Vorb/ui/imgui/backends/imgui_impl_sdl.h>
#include <Vorb/ui/imgui/backends/imgui_impl_opengl3.h>

void MaterialUtils::uploadLightingUniforms(const MaterialShader& material) {
    ASSERT_RENDER_THREAD();
    LightingOptions& optionsLeft = *sDebugOptions.mLightingOptions;
    LightingOptions& optionsRight = *sDebugOptions.mLightingOptionsSplit;
    glUniform2i(material.getUniform("unLightingModel"), optionsLeft.mLightingModel, optionsRight.mLightingModel);
    glUniform2f(material.getUniform("unHazeExponent"), optionsLeft.mHazeExponent, optionsRight.mHazeExponent);
    glUniform2f(material.getUniform("unHazeDivisor"), optionsLeft.mHazeDivisor, optionsRight.mHazeDivisor);
    glUniform2f(material.getUniform("unAmbient"), optionsLeft.mAmbient, optionsRight.mAmbient);
    glUniform2f(material.getUniform("unSunIntensity"), optionsLeft.mSunIntensity, optionsRight.mSunIntensity);
    uploadTonemapUniforms(material);
    checkGlError("MaterialUtils::uploadLightingUniforms");
}

void MaterialUtils::uploadTonemapUniforms(const MaterialShader& material) {
    LightingOptions& optionsLeft = *sDebugOptions.mLightingOptions;
    LightingOptions& optionsRight = *sDebugOptions.mLightingOptionsSplit;
    if (sDebugOptions.mLightPresetSplitView) {
        glUniform1f(material.getUniform("unLightingSplit"), sDebugOptions.mLightPresetSplitAmount);
    }
    else {
        glUniform1f(material.getUniform("unLightingSplit"), 1.0f);
    }
    glUniform2f(material.getUniform("unGamma"), optionsLeft.mGamma, optionsRight.mGamma);
    glUniform2f(material.getUniform("unExposure"), optionsLeft.mExposure, optionsRight.mExposure);
    glUniform2i(material.getUniform("unTonemapOperator"), optionsLeft.mToneMapOperator, optionsRight.mToneMapOperator);
    // Uchimira properties
    glUniform1f(material.getUniform("unUchMaxDisplayBrightness"), sDebugOptions.unUchMaxDisplayBrightness);
    glUniform1f(material.getUniform("unUchContrast"), sDebugOptions.unUchContrast);
    glUniform1f(material.getUniform("unUchLinearSectionStart"), sDebugOptions.unUchLinearSectionStart);
    glUniform1f(material.getUniform("unUchLinearSectionLength"), sDebugOptions.unUchLinearSectionLength);
    glUniform1f(material.getUniform("unUchBlack"), sDebugOptions.unUchBlack);
    glUniform1f(material.getUniform("unUchPedestal"), sDebugOptions.unUchPedestal);
}

void MaterialUtils::updateAndRenderLightingControls(ui32& ID, LightingOptions* options, int presetIndex) {
    ASSERT_RENDER_THREAD(); ImGui::PushID(++ID);
    ImGui::SliderFloat("Gamma", &options->mGamma, 0.0f, 4.0f);
    ImGui::SliderFloat("Exposure", &options->mExposure, 0.0f, 4.0f);
    ImGui::SliderFloat("Haze Exponent", &options->mHazeExponent, 0.0f, 2.0f);
    ImGui::SliderFloat("Haze Divisor", &options->mHazeDivisor, 10.0f, 15000.0f);
    ImGui::SliderFloat("Ambient Light", &options->mAmbient, 0.0f, 2.0f);
    ImGui::SliderFloat("Sun Intensity", &options->mSunIntensity, 0.0f, 20.0f);
    switch (options->mToneMapOperator) {
        case 0:
            ImGui::Text("TONEMAP: NONE");
            break;
        case 1:
            ImGui::Text("TONEMAP: REINARD");
            break;
        case 2:
            ImGui::Text("TONEMAP: LOTTES");
            break;
        case 3:
            ImGui::Text("TONEMAP: UCHIMURA");
            break;
        case 4:
            ImGui::Text("TONEMAP: UNREAL");
            break;
        case 5:
            ImGui::Text("TONEMAP: FILMIC");
            break;
        case 6:
            ImGui::Text("TONEMAP: UNCHARTED 2");
            break;
    }
    ImGui::SliderInt("Tonemap Operator", &options->mToneMapOperator, 0, 6);

    switch (options->mLightingModel) {
        case 0:
            ImGui::Text("LIGHTMODEL: PHONG");
            break;
        case 1:
            ImGui::Text("LIGHTMODEL: BLINN_PHONG");
            break;
    }
    ImGui::SliderInt("Lighting model", &options->mLightingModel, 0, e_cast(LIGHTING_MODEL::COUNT) - 1);
    if (ImGui::Button("Reset to Default")) {
        *options = sLightingPresetDefaults[sDebugOptions.mUsingPBR][presetIndex];
    }
    ImGui::PopID();
}