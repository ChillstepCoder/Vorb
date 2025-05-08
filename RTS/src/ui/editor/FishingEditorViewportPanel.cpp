#include "stdafx.h"
#include "FishingEditorViewportPanel.h"

#include "ui/minigame/FishingMinigame.h"

#include "rendering/RenderContext.h"
#include "resources/ModelRepository.h"
#include "rendering/MaterialShaderRepository.h"

#include <imgui.h>

FishingEditorViewportPanel::FishingEditorViewportPanel() : AssetEditorViewportPanel<FishDef>() {
    mShader = MaterialShaderRepository::get().getAssetHandle(CStrToken("editor_model_pbr"));
}

void FishingEditorViewportPanel::updateAndRenderInternal(f32 elapsedSec) {
    if (mAssetWasChanged) {
        mCurrentFishingMinigame.reset();
        mAssetWasChanged = false;
    }
}

void FishingEditorViewportPanel::updateAndRenderPrimaryControls(f32 ySize) {
    if (!mAssetData) {
        return;
    }

    bool changed = false;
    ImGui::Separator();
    updateAndRenderSaveButton();
    ImGui::BeginChild("Fishing Editor Controls", ImVec2(0.0f, ySize), true, ImGuiWindowFlags_NoCollapse/* | ImGuiWindowFlags_NoScrollbar*/);

    ImGui::Text("Fishing Editor Controls");
    if (mCurrentFishingMinigame) {
        if (ImGui::Button("Stop Minigame")) {
            mCurrentFishingMinigame.reset();
        }
    } else {
        if (ImGui::Button("Start Minigame")) {
            mCurrentFishingMinigame = std::make_unique<FishingMinigame>(*mAssetData, nullptr, nullptr, getMinigameFlags());
        }
    }
    ImGui::Checkbox("Disable chests", &mDisableChests);
    ImGui::Checkbox("Disable debris", &mDisableDebris);
    ImGui::Separator();
    FishingMinigameFishData& minigameData = mAssetData->mMinigameData;
    if (updateAndRenderImguiControls(*mAssetData)) {
        // Mark dirty
        changed = true;
    }
    ImGui::Separator();
    ImGui::Text("Minigame");
    constexpr f32 INDENT = 16.0f;
    ImGui::Indent(INDENT);
    if (updateAndRenderImguiControls(mAssetData->mMinigameData)) {
        // Mark dirty
        changed = true;
    }
    ImGui::Unindent(INDENT);

    ImGui::Separator();
    ImGui::Text("Player");
    ImGui::Indent(INDENT);
    // Player Data
    // TODO: Shared struct for physics and motion info
    ImGui::SliderFloat("Player Max Speed", &minigameData.mPlayerMaxSpeed, 0.0f, 600.0f);
    ImGui::SliderFloat("Player Acceleration", &minigameData.mPlayerAcceleration, 0.0f, 4000.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Player Drag", &minigameData.mPlayerDrag, 0.0f, 1.0f);
    ImGui::SliderFloat("Player Wall Bouncyness", &minigameData.mPlayerWallBouncyness, 0.0f, 1.0f);
    ImGui::SliderFloat2("Player Stickyness", &minigameData.mPlayerStickyness.x, 0.0f, 1.0f);
    ImGui::SliderFloat("Player Strength", &minigameData.mPlayerStrength, 0.0f, 2000.0f);

    ImGui::Unindent(INDENT);
    ImGui::Separator();
    updateAndRenderSharedControls();
    ImGui::Separator();

    ImGui::EndChild();

    if (changed) {
        LOG_CRITICAL("TODO MARK DIRTY");
    }
}

const MaterialShaderDef* FishingEditorViewportPanel::getShader() {
    if (mCurrentFishingMinigame) return nullptr;

    return mShader->tryGetLoadedAsset();
}

void FishingEditorViewportPanel::renderMesh() {
    if (mCurrentFishingMinigame) {

        MinigameResult result = mCurrentFishingMinigame->updateAndRender(mViewportDims, RenderContext::getInstance().getCurrentFrameElapsedSec());
        if (result.mType == MinigameResultType::Fail) {
            LOG_INFO("Fishing failed!");
            mCurrentFishingMinigame = std::make_unique<FishingMinigame>(*mAssetData, nullptr, nullptr, getMinigameFlags());
        } else if (result.mType == MinigameResultType::Success) {
            LOG_INFO("Fishing success!");
            mCurrentFishingMinigame = std::make_unique<FishingMinigame>(*mAssetData, nullptr, nullptr, getMinigameFlags());
        }
    }
    else {
        renderFishModel();
    }
}

void FishingEditorViewportPanel::renderFishModel() {
    ModelRepository& modelRepo = ModelRepository::get();
    if (mAssetData) {
        const ModelDef& model = modelRepo.getLoadedAsset(mAssetData->mModelId);
        renderMeshStatic(&model, 0, 0, false, 0);
    }
}

BitFlags<FishingMinigameFlags> FishingEditorViewportPanel::getMinigameFlags() {
    BitFlags<FishingMinigameFlags> flags;
    if (mDisableChests) {
        flags.setBit(FishingMinigameFlags::DISABLE_CHESTS);
    }
    if (mDisableDebris) {
        flags.setBit(FishingMinigameFlags::DISABLE_DEBRIS);
    }
    return flags;
}
