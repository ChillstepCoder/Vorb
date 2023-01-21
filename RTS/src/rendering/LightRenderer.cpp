#include "stdafx.h"
#include "LightRenderer.h"

#include <glm/mat3x3.hpp>

#include "rendering/MaterialRenderer.h"
#include "rendering/MaterialShaderManager.h"
#include "resources/ResourceManager.h"
#include "rendering/MaterialUtils.h"

#include "options/DebugOptions.h"

#include <Vorb/graphics/GBuffer.h>
#include <Vorb/graphics/FullQuadVBO.h>
#include <Vorb/graphics/BlendState.h>

static_assert((int)LightShape::Count == 1, "Update this file to handle new light shape");
static_assert((int)LightAttenuationType::Count == 1, "Update this file to handle new attenuation type");

LightRenderer::LightRenderer() {
    mSunlightMaterial = Services::ResourceManager::ref().getMaterialShaderManager().getMaterialShader("sunlight");
    assert(mSunlightMaterial);

}

LightRenderer::~LightRenderer() {
   
}

void LightRenderer::renderSunlight(vg::GBuffer& inputGBuffer, VGTexture shadowTexture) const {
    vg::sBlendStates.REPLACE.set();
    ui32 textureUnit = 0;
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

    sGlobalFullQuadVBO.draw();
}
