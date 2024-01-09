#include "stdafx.h"
#include "TileDistributionEditorViewportPanel.h"

#include "resources/TileDistributionRepository.h"

constexpr i32 MAX_TILE_OFFSET = 2048;

TileDistributionEditorViewportPanel::TileDistributionEditorViewportPanel() = default;

TileDistributionEditorViewportPanel::~TileDistributionEditorViewportPanel() {
}

void TileDistributionEditorViewportPanel::updateAndRenderPrimaryControls(f32 ySize) {
    ImGui::BeginChild("Tile Distribution Controls", ImVec2(0.0f, ySize), true, ImGuiWindowFlags_NoCollapse/* | ImGuiWindowFlags_NoScrollbar*/);

    if (mAssetWasChanged) {
        mDirtyView = true;
        mAssetWasChanged = false;
    }

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
                mAssetData->spacing = glm::clamp(mAssetData->spacing, 1, 128);
                mAssetData->minDistance = glm::min(mAssetData->minDistance, mAssetData->spacing);
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
            mDirtyView |= ImGui::SliderInt("Texture Size", &mTextureSize, TileDistributionPreviewTexture::MIN_TEXTURE_SIZE, TileDistributionPreviewTexture::MAX_TEXTURE_SIZE);
            // TODO: Due to alt+click inputs allowing going outside bounds, we need to sanitize outputs
            mTextureSize = glm::clamp(mTextureSize, TileDistributionPreviewTexture::MIN_TEXTURE_SIZE, TileDistributionPreviewTexture::MAX_TEXTURE_SIZE);
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
        mTextureSize = glm::clamp(mTextureSize, TileDistributionPreviewTexture::MIN_TEXTURE_SIZE, TileDistributionPreviewTexture::MAX_TEXTURE_SIZE);

        const ImVec2 rightDrag = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
        ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);

        const f32 MOVE_SPEED = 1.0f;
        mTileOffset += i32v2(-rightDrag.x * MOVE_SPEED, rightDrag.y * MOVE_SPEED);

        if (mPrevTextureSize != mTextureSize || mPrevTileOffset != mTileOffset) {
            mDirtyView = true;
        }
    }
}

void TileDistributionEditorViewportPanel::renderMesh() {

    if (!mAssetData) {
        return;
    }

    if (mDirtyView) {
        mDirtyView = false;
        mSkipPrecalc = false;
        PreciseTimer timer;
        mPreviewTexture.generate(*mAssetData, mTextureSize, mTileOffset, mDensityMult);
        mLastGenerationTimeMs = timer.stop();
    }

    mPreviewTexture.renderFullScreen(mShowSpawns, mShowThreshold);

}
