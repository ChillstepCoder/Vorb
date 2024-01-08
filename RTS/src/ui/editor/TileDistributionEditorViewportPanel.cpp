#include "stdafx.h"
#include "TileDistributionEditorViewportPanel.h"

#include "rendering/MaterialShaderRepository.h"
#include "rendering/MaterialRenderer.h"

#include "generation/TileDistributionSampler.h"
#include "resources/TileDistributionRepository.h"

#include <vorb/graphics/FullscreenTriangleVAO.h>
#include <vorb/graphics/SamplerState.h>

constexpr i32 MIN_TEXTURE_SIZE = 64;
constexpr i32 MAX_TEXTURE_SIZE = 512;
constexpr i32 MAX_TILE_OFFSET = 2048;

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
        ImGui::Text("Gen time: %.2f ms", mLastGenerationTimeMs);
        ImGui::SameLine();
        if (ImGui::Button("Regenerate")) {
            mDirtyView = true;
        }
        updateAndRenderSaveButton();
        ImGui::Separator();

        if (ImGui::CollapsingHeader("Properties")) {
            if (updateAndRenderImguiControls(*mAssetData)) {
                if (mAssetData->spacing < 1) mAssetData->spacing = 1;
                mAssetData->probability = glm::min(mAssetData->probability, 1.0f);
                // Mark dirty
                changed = true;
            }
            if (mAssetData->distFunc) {
                if (ImGui::Button("Remove noise")) {
                    changed = true;
                    mAssetData->distFunc.reset();
                }
                else {
                    const f32v2 range = mAssetData->distFunc->getRange();
                    ImGui::Text("Range: [%f, %f]", range.x, range.y);
                    changed |= updateAndRenderImguiControls(*mAssetData->distFunc);
                }
            }
            else if (ImGui::Button("Add Noise")) {
                changed = true;
                mAssetData->distFunc = std::make_unique<NoiseFunction>();
            }
        }

        if (ImGui::CollapsingHeader("View Controls")) {
            mDirtyView |= ImGui::SliderInt("Texture Size", &mTextureSize, MIN_TEXTURE_SIZE, MAX_TEXTURE_SIZE);
            // TODO: Due to alt+click inputs allowing going outside bounds, we need to sanitize outputs
            mTextureSize = glm::clamp(mTextureSize, MIN_TEXTURE_SIZE, MAX_TEXTURE_SIZE);
            mDirtyView |= ImGui::SliderInt("Tile X Offset", &mTileOffset.x, 0, MAX_TILE_OFFSET);
            mDirtyView |= ImGui::SliderInt("Tile Y Offset", &mTileOffset.y, 0, MAX_TILE_OFFSET);
            ImGui::Checkbox("Show Threshold", &mShowThreshold);
            ImGui::Checkbox("Show Spawns", &mShowSpawns);
            changed |= ImGui::Checkbox("Use Precalc", &mTryPrecalc);
            mDirtyView |= ImGui::SliderFloat("Density Mult", &mDensityMult, 0.0f, 1.0f);
        }
        mTileOffset = glm::clamp(mTileOffset, 0, MAX_TILE_OFFSET);
    }
    mDirtyView |= changed;
    if (changed) {
        TileDistributionRepository::get().onAssetChangedByEditor(mAssetData->getID());
    }
    ImGui::EndChild();
}

void TileDistributionEditorViewportPanel::postCenterPanelRender(const i32AABB2&) {
    if (ImGui::IsItemHovered()) {
        const i32 mPrevTextureSize = mTextureSize;
        const i32v2 mPrevTileOffset = mTileOffset;
        const int middleDrag = ImGui::GetMouseDragDelta(ImGuiMouseButton_Middle).y;
        ImGui::ResetMouseDragDelta(ImGuiMouseButton_Middle);
        mTextureSize += middleDrag;
        mTextureSize = glm::clamp(mTextureSize, MIN_TEXTURE_SIZE, MAX_TEXTURE_SIZE);

        const ImVec2 rightDrag = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
        ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);

        const f32 MOVE_SPEED = 1.0f;
        mTileOffset += i32v2(-rightDrag.x * MOVE_SPEED, rightDrag.y * MOVE_SPEED);

        if (mPrevTextureSize != mTextureSize || mPrevTileOffset != mTileOffset) {
            mDirtyView = true;
        }
        LOG_CRITICAL("{} {} {}", middleDrag, rightDrag.x, rightDrag.y);
    }
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
        mSkipPrecalc = false;
        if (!mThresholdTexture || mTextureSize != mLastTextureSize) {
            if (mThresholdTexture && mTextureSize != mLastTextureSize) {
                glDeleteTextures(1, &mThresholdTexture);
                glDeleteTextures(1, &mSpawnTexture);
            };
            mLastTextureSize = mTextureSize;
            glCreateTextures(GL_TEXTURE_2D, 1, &mThresholdTexture);
            glTextureStorage2D(mThresholdTexture, 1, GL_RGB8, mTextureSize, mTextureSize);
            glCreateTextures(GL_TEXTURE_2D, 1, &mSpawnTexture);
            glTextureStorage2D(mSpawnTexture, 1, GL_R8, mTextureSize, mTextureSize);
        }
        PreciseTimer timer;
        static std::vector<color3> sColorData(MAX_TEXTURE_SIZE * MAX_TEXTURE_SIZE);
        static std::vector<ui8> sSpawnData(MAX_TEXTURE_SIZE * MAX_TEXTURE_SIZE);
        i32v2 worldPos;
        for (ui32 y = 0; y < mTextureSize; y++) {
            worldPos.y = mTileOffset.y + y;
            for (ui32 x = 0; x < mTextureSize; x++) {
                worldPos.x = mTileOffset.x + x;
                const f32 val = glm::min(TileDistributionSampler::getThresholdAtPosition(*mAssetData, worldPos), 1.0f);
                const ui8 byteVal = val * 255;
                sColorData[y * mTextureSize + x] = color3(val, val, val);
                if (mTryPrecalc) {
                    sSpawnData[y * mTextureSize + x] = TileDistributionSampler::samplePrecalc(*mAssetData, worldPos, mDensityMult) ? 255u : 0u;
                }
                else {
                    sSpawnData[y * mTextureSize + x] = TileDistributionSampler::sample(*mAssetData, worldPos, mDensityMult) ? 255u : 0u;
                }
            }
        }
        mLastGenerationTimeMs = timer.stop();
        glTextureSubImage2D(mThresholdTexture, 0, 0, 0, mTextureSize, mTextureSize, GL_RGB, GL_UNSIGNED_BYTE, sColorData.data());
        glTextureSubImage2D(mSpawnTexture, 0, 0, 0, mTextureSize, mTextureSize, GL_RED, GL_UNSIGNED_BYTE, sSpawnData.data());
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
