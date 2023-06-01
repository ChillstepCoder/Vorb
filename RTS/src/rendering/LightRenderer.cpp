#include "stdafx.h"
#include "LightRenderer.h"

#include <glm/mat3x3.hpp>

#include "rendering/MaterialRenderer.h"
#include "rendering/MaterialShaderManager.h"
#include "resources/ResourceManager.h"
#include "rendering/MaterialUtils.h"

#include "rendering/texture/Cubemap.h"
#include "rendering/material/BrdfLUT.h"
#include "rendering/StencilBufferIDs.h"

#include "options/LightingOptions.h"
#include "options/DebugOptions.h"

#include <Vorb/graphics/GBuffer.h>
#include <Vorb/graphics/FullscreenTriangleVAO.h>
#include <Vorb/graphics/BlendState.h>

static_assert((int)LightShape::Count == 1, "Update this file to handle new light shape");
static_assert((int)LightAttenuationType::Count == 1, "Update this file to handle new attenuation type");

LightRenderer::LightRenderer() {
    mSunlightMaterial = Services::ResourceManager::ref().getMaterialShaderManager().getMaterialShader("sunlight");
    mSunlightMaterialPbr = Services::ResourceManager::ref().getMaterialShaderManager().getMaterialShader("sunlight_pbr");
    assert(mSunlightMaterial);
    assert(mSunlightMaterialPbr);

}

LightRenderer::~LightRenderer() {
   
}

void LightRenderer::renderSunlight(vg::GBuffer& inputGBuffer, VGTexture shadowTexture, const Cubemap& skyCubeMap) const {
    vg::sBlendStates.REPLACE.set();
    ui32 textureUnit = 0;

    if (sDebugOptions.mUsingPBR) {
        // Do not shade the sky
        glEnable(GL_STENCIL_TEST);
        glStencilFunc(GL_NOTEQUAL, 0/*e_cast(StencilBufferIDs::SKY)*/, 0xFF);
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

        MaterialRenderer::bindMaterialForRender(*mSunlightMaterialPbr, &textureUnit);
        assert(textureUnit <= 1);

        glUniform1i(mSunlightMaterialPbr->getUniform("unIrradianceMap"), textureUnit);
        glBindTextureUnit(textureUnit++, skyCubeMap.getIrradianceTexture());
        glUniform1i(mSunlightMaterialPbr->getUniform("unPrefilterMap"), textureUnit);
        glBindTextureUnit(textureUnit++, skyCubeMap.getPrefilterMap());
        glUniform1i(mSunlightMaterialPbr->getUniform("unBrdfLUT"), textureUnit);
        glBindTextureUnit(textureUnit++, BrdfLUT::getTexture());
        // Texture inputs
        glUniform1i(mSunlightMaterialPbr->getUniform("unTextureAlbedo"), textureUnit);
        glBindTextureUnit(textureUnit++, inputGBuffer.getAlbedoTexture());
        glUniform1i(mSunlightMaterialPbr->getUniform("unTextureNormals"), textureUnit);
        glBindTextureUnit(textureUnit++, inputGBuffer.getNormalTexture());
        glUniform1i(mSunlightMaterialPbr->getUniform("unTextureRoughness"), textureUnit);
        glBindTextureUnit(textureUnit++, inputGBuffer.getTertiaryTexture());
        glUniform1i(mSunlightMaterialPbr->getUniform("unTextureDepth"), textureUnit);
        glBindTextureUnit(textureUnit++, inputGBuffer.getDepthTexture());
        glUniform1i(mSunlightMaterialPbr->getUniform("unTextureShadow"), textureUnit);
        glBindTextureUnit(textureUnit++, shadowTexture);

        // Light uniforms
        LightingOptions& optionsLeft = *sDebugOptions.mLightingOptions;
        LightingOptions& optionsRight = *sDebugOptions.mLightingOptionsSplit;
        glUniform2f(mSunlightMaterialPbr->getUniform("unHazeExponent"), optionsLeft.mHazeExponent, optionsRight.mHazeExponent);
        if (sDebugOptions.mIsCameraUnderwater) {
            glUniform2f(mSunlightMaterialPbr->getUniform("unHazeDivisor"), sDebugOptions.mUnderwaterHazeDivisor, sDebugOptions.mUnderwaterHazeDivisor);
        }
        else {
            glUniform2f(mSunlightMaterialPbr->getUniform("unHazeDivisor"), optionsLeft.mHazeDivisor, optionsRight.mHazeDivisor);
        }
        glUniform2f(mSunlightMaterialPbr->getUniform("unAmbient"), optionsLeft.mAmbient, optionsRight.mAmbient);
        glUniform2f(mSunlightMaterialPbr->getUniform("unExposure"), optionsLeft.mExposure, optionsRight.mExposure);
        glUniform2f(mSunlightMaterialPbr->getUniform("unSunIntensity"), optionsLeft.mSunIntensity, optionsRight.mSunIntensity);
        glUniform2f(mSunlightMaterialPbr->getUniform("unGamma"), optionsLeft.mGamma, optionsRight.mGamma);
        if (sDebugOptions.mLightPresetSplitView) {
            glUniform1f(mSunlightMaterialPbr->getUniform("unLightingSplit"), sDebugOptions.mLightPresetSplitAmount);
        }
        else {
            glUniform1f(mSunlightMaterialPbr->getUniform("unLightingSplit"), 1.0f);
        }
    }
    else {
        MaterialRenderer::bindMaterialForRender(*mSunlightMaterial, &textureUnit);
        // Texture inputs
        glUniform1i(mSunlightMaterial->getUniform("unTextureAlbedo"), textureUnit);
        glBindTextureUnit(textureUnit, inputGBuffer.getAlbedoTexture());
        glUniform1i(mSunlightMaterial->getUniform("unTextureNormals"), textureUnit + 1);
        glBindTextureUnit(textureUnit + 1, inputGBuffer.getNormalTexture());
        glUniform1i(mSunlightMaterial->getUniform("unTextureDepth"), textureUnit + 2);
        glBindTextureUnit(textureUnit + 2, inputGBuffer.getDepthTexture());
        glUniform1i(mSunlightMaterial->getUniform("unTextureShadow"), textureUnit + 3);
        glBindTextureUnit(textureUnit + 3, shadowTexture);

        // Light uniforms
        LightingOptions& optionsLeft = *sDebugOptions.mLightingOptions;
        LightingOptions& optionsRight = *sDebugOptions.mLightingOptionsSplit;
        glUniform2i(mSunlightMaterial->getUniform("unLightingModel"), optionsLeft.mLightingModel, optionsRight.mLightingModel);
        glUniform2f(mSunlightMaterial->getUniform("unHazeExponent"), optionsLeft.mHazeExponent, optionsRight.mHazeExponent);
        glUniform2f(mSunlightMaterial->getUniform("unHazeDivisor"), optionsLeft.mHazeDivisor, optionsRight.mHazeDivisor);
        glUniform2f(mSunlightMaterial->getUniform("unAmbient"), optionsLeft.mAmbient, optionsRight.mAmbient);
        glUniform2f(mSunlightMaterial->getUniform("unSunIntensity"), optionsLeft.mSunIntensity, optionsRight.mSunIntensity);
        if (sDebugOptions.mLightPresetSplitView) {
            glUniform1f(mSunlightMaterial->getUniform("unLightingSplit"), sDebugOptions.mLightPresetSplitAmount);
        }
        else {
            glUniform1f(mSunlightMaterial->getUniform("unLightingSplit"), 1.0f);
        }
    }

    sGlobalFullTriangleVAO.draw();
    checkGlError("LightRenderer::renderSunlight");

    glDisable(GL_STENCIL_TEST);
}
