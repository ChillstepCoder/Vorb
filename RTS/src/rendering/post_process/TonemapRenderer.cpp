#include "stdafx.h"
#include "TonemapRenderer.h"

#include "options/DebugOptions.h"
#include "resources/ResourceManager.h"
#include "rendering/MaterialShaderManager.h"
#include "rendering/MaterialRenderer.h"
#include "rendering/MaterialUtils.h"

#include <Vorb/graphics/GBuffer.h>
#include <Vorb/graphics/FullscreenTriangleVAO.h>

TonemapRenderer::TonemapRenderer() {
    mShader = Services::ResourceManager::ref().getMaterialShaderManager().getMaterialShader("tonemap");
}

TonemapRenderer::~TonemapRenderer() {

}

void TonemapRenderer::render(VGTexture lightTextureInput) {
    ui32 textureUnit = 0;
    MaterialRenderer::bindMaterialForRender(*mShader, &textureUnit);
    glUniform1i(mShader->getUniform("unLightTexture"), textureUnit);
    glBindTextureUnit(textureUnit, lightTextureInput);
    MaterialUtils::uploadTonemapUniforms(*mShader);
    sGlobalFullTriangleVAO.draw();
}
