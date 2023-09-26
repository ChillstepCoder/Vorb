#include "stdafx.h"
#include "OverlayRenderer.h"

#include "rendering/MaterialRenderer.h"
#include "resources/ResourceManager.h"
#include "rendering/MaterialShaderRepository.h"

#include "options/DebugOptions.h"

#include <Vorb/graphics/BlendState.h>

OverlayRenderer::OverlayRenderer() {
    mColorOverlayShader = MaterialShaderRepository::get().getAssetHandle(StrToken("color_overlay", 0));
    glCreateVertexArrays(1, &mVao);
}

OverlayRenderer::~OverlayRenderer() {
    glDeleteVertexArrays(1, &mVao);
}

void OverlayRenderer::renderUnderwaterOverlay() {

    vg::BlendState::set(vg::BlendStateType::ALPHA);

    if (const MaterialShaderDef* def = mColorOverlayShader->tryGetAsset()) {

        MaterialRenderer::bindMaterialShaderForRender(*def);
        glUniform4fv(def->getUniform("unColor"), 1, &sDebugOptions.mUnderwaterOverlayColor.x);
        glBindVertexArray(mVao);
        // Draw one triangle that will cover the entire screen
        glDrawArrays(GL_TRIANGLES, 0, 3);

    }
    vg::BlendState::restorePrevious();
}
