#include "stdafx.h"
#include "FishingEditorViewportPanel.h"

#include "ui/minigame/FishingMinigame.h"

#include "rendering/RenderContext.h"
#include "rendering/mesh/MeshDrawer.h"
#include "resources/ModelRepository.h"
#include "resources/FishRepository.h"
#include "rendering/MaterialShaderRepository.h"

#include <imgui.h>
#include <imgui_internal.h>


#include <Vorb/ui/GameWindow.h>

FishingEditorViewportPanel::FishingEditorViewportPanel() : AssetEditorViewportPanel<FishDef>() {
    mShader = MaterialShaderRepository::get().getAssetHandle(CStrToken("editor_model_pbr"));
}

void FishingEditorViewportPanel::updateAndRenderInternal(f32 elapsedSec) {
    if (mAssetWasChanged) {
        mCurrentFishingMinigame.reset();
        mAssetWasChanged = false;
    }

    renderCenterPanel(nullptr);
}

void FishingEditorViewportPanel::updateAndRenderPrimaryControls(f32 ySize) {
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
    FishingMinigameFishData& minigameData = mAssetData->mMinigameData;
    ImGui::Text("Test Minigame Data");
    ImGui::Separator();
    ImGui::SliderFloat2("Acceleration", &minigameData.mAcceleration.x, 0.0f, 4000.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Fish Drag", &minigameData.mFishDrag, 0.0f, 1.0f);
    ImGui::SliderFloat("Fish Damage Rate", &minigameData.mFishDamageRate, 0.0f, 1.0f);
    ImGui::SliderFloat("Center Magnitism", &minigameData.mCenterMagnitism, 0.0f, 200.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Gravity", &minigameData.mGravity, 0.0f, 1000.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Jerk Chance", &minigameData.mJerkChance, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Jerk Intensity", &minigameData.mJerkIntensity, 0.0f, 5.0f);
    ImGui::SliderFloat2("Jerk Cooldown Range", &minigameData.mJerkCooldownVarianceSec.x, 0.0f, 10.0f);
    ImGui::SliderFloat("Max Speed", &minigameData.mMaxSpeed, 0.0f, 600.0f);
    ImGui::SliderFloat("Out of Stamina Power Mult", &minigameData.mOutOfStaminaPowerMult, 0.0f, 1.0f);
    ImGui::SliderFloat("Player Damage Rate", &minigameData.mPlayerDamageRate, 0.0f, 1.0f);
    ImGui::SliderFloat("Radius", &minigameData.mRadius, 0.1f, 1.0f);
    ImGui::SliderFloat("Stamina Deplete Rate", &minigameData.mStaminaDepleteRate, 0.0f, 1.0f);
    ImGui::SliderFloat("Stamina Recharge Rate", &minigameData.mStaminaRechargeRate, 0.0f, 1.0f);
    ImGui::SliderFloat("Steering Intensity", &minigameData.mSteeringIntensity, 0.0f, 1.0f);
    ImGui::SliderFloat("Wall Bouncyness", &minigameData.mWallBouncyness, 0.0f, 1.0f);
    ImGui::SliderFloat("Success Angle", &minigameData.mSuccessAngle, 1.0f, 90.0f);
    ImGui::SliderFloat("Fail Angle", &minigameData.mFailAngle, 1.0f, 90.0f);
    // TMP CONFIG
    ImGui::SliderInt("Particle Material", &minigameData.mParticleMaterial, 0, 7);
    ImGui::SliderFloat("Particle Scale", &minigameData.mParticleScale, 0.0f, 3.0f);
    ImGui::Separator();
    ImGui::Text("Player");
    // Player Data
    // TODO: Shared struct for physics and motion info
    ImGui::SliderFloat("Player Max Speed", &minigameData.mPlayerMaxSpeed, 0.0f, 600.0f);
    ImGui::SliderFloat("Player Acceleration", &minigameData.mPlayerAcceleration, 0.0f, 4000.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Player Drag", &minigameData.mPlayerDrag, 0.0f, 1.0f);
    ImGui::SliderFloat("Player Wall Bouncyness", &minigameData.mPlayerWallBouncyness, 0.0f, 1.0f);
    ImGui::SliderFloat2("Player Stickyness", &minigameData.mPlayerStickyness.x, 0.0f, 1.0f);
    ImGui::SliderFloat("Player Strength", &minigameData.mPlayerStrength, 0.0f, 2000.0f);
    ImGui::Separator();
    updateAndRenderSharedControls();
    ImGui::Separator();

    ImGui::EndChild();
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
        for (int i = 0; i < model.getNumMeshes(); ++i) {
            MeshDrawer::draw(model.getMesh(i).mMainMesh, MeshLODLevel::Highest);
        }
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
