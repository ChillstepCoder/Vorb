#include "stdafx.h"
#include "WorldTextRenderer.h"

#include "camera/Camera3D.h"
#include "rendering/renderstate/WorldTextRenderState.h"

#include "rendering/mesh/mesher/builder/TextMeshBuilder.h"

#include "resources/ResourceManager.h"
#include "resources/FontRepository.h"
#include "rendering/MaterialShaderRepository.h"
#include "rendering/mesh/Mesh.h"
#include "rendering/MaterialRenderer.h"
#include "rendering/mesh/MeshDrawer.h"

WorldTextRenderer::WorldTextRenderer() {
    mFont = &ResourceManager::get().getFontRepository().getFont("titilium_semibold");
    mTextMesh = std::make_unique<Mesh>();

    AssetHandlePtr<MaterialShaderDef> textShaderHandle = MaterialShaderRepository::get().getAssetHandle(CStrToken("text_billboard"));
    mShader = &textShaderHandle->getLoadedOrUnloadedAsset();
    mShaderHandle = std::move(textShaderHandle);
}

WorldTextRenderer::~WorldTextRenderer() {

}

void WorldTextRenderer::renderWorldText(const std::vector<WorldTextRenderState>& texts, const Camera3D& camera) {

    if (!mShaderHandle->isLoaded()) {
        return;
    }

    // TODO: THIS IS INEFFICIENT
    TextMeshBuilder builder;

    for (const WorldTextRenderState& text : texts) {
        builder.addString(text.text, text.worldPos, *mFont, 0.1f, f32v2(0.0f), TextAlign::CENTER);
    }

    builder.finishMesh(*mTextMesh, MeshDrawMode::STREAM);

    if (mTextMesh->isValid()) {
        glDisable(GL_CULL_FACE); // TODO: Remove
        ui32 textureUnit;
        MaterialRenderer::bindMaterialShaderForRender(*mShader, &textureUnit);
        // Offset relative to camera
        f32v3 offset = -camera.getPosition();
        glUniform3fv(mShader->getUniform("unOffset"), 1, &offset.x);
        glUniform1i(mShader->getUniform("unFontTexture"), textureUnit);
        glBindTextureUnit(textureUnit, mFont->mTexture);
        MeshDrawer::draw(mTextMesh->mGpuData);
    }
}
