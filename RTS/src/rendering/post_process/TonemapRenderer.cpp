#include "stdafx.h"
#include "TonemapRenderer.h"

#include "options/DebugOptions.h"
#include "rendering/MaterialShaderRepository.h"
#include "rendering/MaterialRenderer.h"
#include "rendering/MaterialUtils.h"

#include <Vorb/graphics/GBuffer.h>
#include <Vorb/graphics/FullscreenTriangleVAO.h>

TonemapRenderer::TonemapRenderer() {
    mShaderDef = MaterialShaderRepository::get().getAssetHandle(CStrToken("tonemap"));
}

TonemapRenderer::~TonemapRenderer() {

}

void TonemapRenderer::render(VGTexture lightTextureInput) {
    const MaterialShaderDef* def = mShaderDef->tryGetLoadedAsset();
    if (def) {
        ui32 textureUnit = 0;
        MaterialRenderer::bindMaterialShaderForRender(*def, &textureUnit);
        glUniform1i(def->getUniform("unLightTexture"), textureUnit);
        glBindTextureUnit(textureUnit, lightTextureInput);
        MaterialUtils::uploadTonemapUniforms(*def);
        sGlobalFullTriangleVAO.draw();
    }
}
