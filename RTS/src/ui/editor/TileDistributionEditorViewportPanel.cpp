#include "stdafx.h"
#include "TileDistributionEditorViewportPanel.h"

#include "rendering/MaterialShaderRepository.h"
#include "rendering/MaterialRenderer.h"

#include "generation/TileDistributionSampler.h"

#include <vorb/graphics/FullscreenTriangleVAO.h>
#include <vorb/graphics/SamplerState.h>

TileDistributionEditorViewportPanel::TileDistributionEditorViewportPanel() = default;

TileDistributionEditorViewportPanel::~TileDistributionEditorViewportPanel() {
    if (mThresholdTexture) {
        glDeleteTextures(1, &mThresholdTexture);
    }
}

void TileDistributionEditorViewportPanel::updateAndRenderPrimaryControls(f32 ySize) {
    ImGui::BeginChild("Tile Distribution Controls", ImVec2(0.0f, ySize), true, ImGuiWindowFlags_NoCollapse/* | ImGuiWindowFlags_NoScrollbar*/);

    bool changed = false;
    if (mAssetData) {
        ImGui::Text(mAssetData->getName().toString().c_str());
        updateAndRenderSaveButton();
        ImGui::Separator();

        if (ImGui::CollapsingHeader("Properties")) {
            if (updateAndRenderImguiControls(*mAssetData)) {
                // Mark dirty
                changed = true;
            }
        }

        if (ImGui::CollapsingHeader("View Controls")) {
            mDirtyView |= ImGui::SliderInt("Tile X Offset", &mTileOffset.x, 0, 2048);
            mDirtyView |= ImGui::SliderInt("Tile Y Offset", &mTileOffset.y, 0, 2048);
            ImGui::Checkbox("Show Threshold", &mShowThreshold);
            ImGui::Checkbox("Show Spawns", &mShowSpawns);
            mDirtyView |= ImGui::SliderFloat("Density Mult", &mDensityMult, 0.0f, 1.0f);
        }
    }
    mDirtyView |= changed;
    ImGui::EndChild();
}

void TileDistributionEditorViewportPanel::renderMesh() {

    if (!mImageShader) {
        mImageShader = MaterialShaderRepository::get().getAssetHandle(CStrToken("editor_tile_dist"));
    }

    if (!mAssetData) {
        return;
    }

    if (mDirtyView) {
        mDirtyView = false;
        constexpr ui32 SIZE = 128;
        if (!mThresholdTexture) {
            glCreateTextures(GL_TEXTURE_2D, 1, &mThresholdTexture);
            glTextureStorage2D(mThresholdTexture, 1, GL_RGB8, SIZE, SIZE);
            glCreateTextures(GL_TEXTURE_2D, 1, &mSpawnTexture);
            glTextureStorage2D(mSpawnTexture, 1, GL_R8, SIZE, SIZE);
        }
        color3 colorData[SIZE * SIZE];
        ui8 spawnData[SIZE * SIZE];
        i32v2 worldPos;
        for (ui32 y = 0; y < SIZE; y++) {
            worldPos.y = mTileOffset.y + y;
            for (ui32 x = 0; x < SIZE; x++) {
                worldPos.x = mTileOffset.x + x;
                const f32 val = TileDistributionSampler::getThresholdAtPosition(*mAssetData, worldPos);
                const ui8 byteVal = val * 255;
                colorData[y * SIZE + x] = color3(val, val, val);
                spawnData[y * SIZE + x] = TileDistributionSampler::sample(*mAssetData, worldPos, mDensityMult) ? 255u : 0u;
            }
        }
        glTextureSubImage2D(mThresholdTexture, 0, 0, 0, SIZE, SIZE, GL_RGB, GL_UNSIGNED_BYTE, colorData);
        glTextureSubImage2D(mSpawnTexture, 0, 0, 0, SIZE, SIZE, GL_RED, GL_UNSIGNED_BYTE, spawnData);
        vg::sSamplerStates.POINT_WRAP.setForTexture(mThresholdTexture);
        vg::sSamplerStates.POINT_WRAP.setForTexture(mSpawnTexture);
        checkGlError("TileDistributionEditorViewportPanel::renderMesh dirtyView");
    }

    if (const MaterialShaderDef* shaderDef = mImageShader->tryGetLoadedAsset()) {
        ui32 textureIndex = 0;
        MaterialRenderer::bindMaterialShaderForRender(*shaderDef, &textureIndex);
        glUniform1i(shaderDef->getUniform("unThresholdTexture"), textureIndex);
        glUniform1i(shaderDef->getUniform("unSpawnTexture"), textureIndex + 1);
        glBindTextureUnit(textureIndex, mThresholdTexture);
        glBindTextureUnit(textureIndex + 1, mSpawnTexture);
        glUniform1i(shaderDef->getUniform("unShowThreshold"), (int)mShowThreshold);
        glUniform1i(shaderDef->getUniform("unShowSpawns"), (int)mShowSpawns);


        sGlobalFullTriangleVAO.draw();
    }
}
