#include "stdafx.h"
#include "TileDistributionPreviewTexture.h"

#include "definitions/TileDistributionDef.h"
#include "generation/TileDistributionSampler.h"

#include "rendering/MaterialShaderRepository.h"
#include "rendering/MaterialRenderer.h"

#include <vorb/graphics/FullscreenTriangleVAO.h>
#include <vorb/graphics/SamplerState.h>

TileDistributionPreviewTexture::~TileDistributionPreviewTexture() {
    glDeleteTextures(1, &mSpawnTexture);
    glDeleteTextures(1, &mThresholdTexture);
}

void TileDistributionPreviewTexture::generate(const TileDistributionDef& def, i32 textureSize, i32v2 tilePosCorner, f32 densityMult) {
    
    ASSERT_RENDER_THREAD();
    textureSize = glm::clamp(textureSize, MIN_TEXTURE_SIZE, MAX_TEXTURE_SIZE);
    if (!mSpawnTexture || mTextureSize != textureSize) {
        glDeleteTextures(1, &mThresholdTexture);
        glDeleteTextures(1, &mSpawnTexture);
        mTextureSize = textureSize;
        glCreateTextures(GL_TEXTURE_2D, 1, &mThresholdTexture);
        glCreateTextures(GL_TEXTURE_2D, 1, &mSpawnTexture);
        glTextureStorage2D(mSpawnTexture, 1, GL_R8, mTextureSize, mTextureSize);
        glTextureStorage2D(mThresholdTexture, 1, GL_R8, mTextureSize, mTextureSize);
    }
    PreciseTimer timer;
    static std::vector<ui8> sColorData(MAX_TEXTURE_SIZE * MAX_TEXTURE_SIZE);
    static std::vector<ui8> sSpawnData(MAX_TEXTURE_SIZE * MAX_TEXTURE_SIZE);
    i32v2 worldPos;
    for (ui32 y = 0; y < mTextureSize; y++) {
        worldPos.y = tilePosCorner.y + y;
        for (ui32 x = 0; x < mTextureSize; x++) {
            worldPos.x = tilePosCorner.x + x;
            const f32 val = glm::min(TileDistributionSampler::getThresholdAtPosition(def, worldPos, 1.0f), 1.0f);
            const ui8 byteVal = val * 255;
            sSpawnData[y * mTextureSize + x] = TileDistributionSampler::sample(def, worldPos, densityMult, 1.0f) ? 255u : 0u;
            sColorData[y * mTextureSize + x] = byteVal;
        }
    }
    glTextureSubImage2D(mSpawnTexture, 0, 0, 0, mTextureSize, mTextureSize, GL_RED, GL_UNSIGNED_BYTE, sSpawnData.data());
    glTextureSubImage2D(mThresholdTexture, 0, 0, 0, mTextureSize, mTextureSize, GL_RED, GL_UNSIGNED_BYTE, sColorData.data());
    vg::sSamplerStates.POINT_WRAP.setForTexture(mSpawnTexture);
    vg::sSamplerStates.POINT_WRAP.setForTexture(mThresholdTexture);
    checkGlError("TileDistributionPreviewTexture::generate");
}

void TileDistributionPreviewTexture::renderFullScreen(bool showSpawns, bool showThreshold) {

    if (!mImageShader) {
        mImageShader = MaterialShaderRepository::get().getAssetHandle(CStrToken("editor_tile_dist"));
    }

    if (const MaterialShaderDef* shaderDef = mImageShader->tryGetLoadedAsset()) {
        ui32 textureIndex = 0;
        MaterialRenderer::bindMaterialShaderForRender(*shaderDef, &textureIndex);
        glUniform1i(shaderDef->getUniform("unThresholdTexture"), textureIndex);
        glUniform1i(shaderDef->getUniform("unSpawnTexture"), textureIndex + 1);
        glBindTextureUnit(textureIndex, mThresholdTexture);
        glBindTextureUnit(textureIndex + 1, mSpawnTexture);
        glUniform1i(shaderDef->getUniform("unShowThreshold"), (int)showThreshold);
        glUniform1i(shaderDef->getUniform("unShowSpawns"), (int)showSpawns);


        sGlobalFullTriangleVAO.draw();
    }
}
