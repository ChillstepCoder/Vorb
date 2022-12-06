#include "stdafx.h"
#include "MaterialUtils.h"

#include "options/DebugOptions.h"
#include "MaterialShader.h"

void MaterialUtils::uploadLightingUniforms(const MaterialShader& material) {
    LightingOptions& optionsLeft = *sDebugOptions.mLightingOptions;
    LightingOptions& optionsRight = *sDebugOptions.mLightingOptionsSplit;
    glUniform2f(material.getUniform("unGamma"), optionsLeft.mGamma, optionsRight.mGamma);
    glUniform2f(material.getUniform("unExposure"), optionsLeft.mExposure, optionsRight.mExposure);
    glUniform2i(material.getUniform("unTonemapOperator"), optionsLeft.mToneMapOperator, optionsRight.mToneMapOperator);
    glUniform2i(material.getUniform("unLightingModel"), optionsLeft.mLightingModel, optionsRight.mLightingModel);
    glUniform2f(material.getUniform("unHazeExponent"), optionsLeft.mHazeExponent, optionsRight.mHazeExponent);
    glUniform2f(material.getUniform("unHazeDivisor"), optionsLeft.mHazeDivisor, optionsRight.mHazeDivisor);
    glUniform2f(material.getUniform("unAmbient"), optionsLeft.mAmbient, optionsRight.mAmbient);
    glUniform2f(material.getUniform("unSunIntensity"), optionsLeft.mSunIntensity, optionsRight.mSunIntensity);
    if (sDebugOptions.mLightPresetSplitView) {
        glUniform1f(material.getUniform("unLightingSplit"), sDebugOptions.mLightPresetSplitAmount);
    }
    else {
        glUniform1f(material.getUniform("unLightingSplit"), 1.0f);
    }
    glUniform1f(material.getUniform("unUchMaxDisplayBrightness"), sDebugOptions.unUchMaxDisplayBrightness);
    glUniform1f(material.getUniform("unUchContrast"), sDebugOptions.unUchContrast);
    glUniform1f(material.getUniform("unUchLinearSectionStart"), sDebugOptions.unUchLinearSectionStart);
    glUniform1f(material.getUniform("unUchLinearSectionLength"), sDebugOptions.unUchLinearSectionLength);
    glUniform1f(material.getUniform("unUchBlack"), sDebugOptions.unUchBlack);
    glUniform1f(material.getUniform("unUchPedestal"), sDebugOptions.unUchPedestal);
    checkGlError("MaterialUtils::uploadLightingUniforms");
}
