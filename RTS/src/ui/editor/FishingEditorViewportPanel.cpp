#include "stdafx.h"
#include "FishingEditorViewportPanel.h"

#include "ui/minigame/FishingMinigame.h"

#include "rendering/RenderContext.h"
#include "rendering/mesh/MeshDrawer.h"
#include "resources/ResourceManager.h"
#include "resources/ModelRepository.h"
#include "rendering/MaterialShaderManager.h"

#include <Vorb/ui/imgui/imgui.h>
#include <Vorb/ui/imgui/backends/imgui_impl_sdl.h>
#include <Vorb/ui/imgui/backends/imgui_impl_opengl3.h>

#include <Vorb/ui/GameWindow.h>

bool FishingEditorViewportPanel::updateAndRender()
{
 
    bool isOpen = true;
    ImGui::Begin("Fishing Editor", &isOpen, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar);

    ImVec2 mouseDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
    ImGui::ResetMouseDragDelta(ImGuiMouseButton_Right);
    mViewportDims = f32v2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y);
    updateCamera(mViewportDims.x / mViewportDims.y);

    mClearColor = f32v4(0.0f);

    //updateFramebufferAndLazyInit(imageDims);

    //clearFramebuffers();

    // Lazy init so we don't use GPU memory when not in editor
    if (sGBuffers[0] == nullptr) {
        initGBuffers(mViewportDims);
    }

    renderCenterPanel(nullptr);

    ImGui::End();

    return isOpen;
}

void FishingEditorViewportPanel::updateAndRenderControls(f32 ySize) {
    ImGui::BeginChild("Fishing Editor Controls", ImVec2(0.0f, ySize), true, ImGuiWindowFlags_NoCollapse/* | ImGuiWindowFlags_NoScrollbar*/);
    ImGui::Text("Fishing Editor Controls");
    if (mCurrentFishingMinigame) {
        if (ImGui::Button("Stop Minigame")) {
            mCurrentFishingMinigame.reset();
        }
    } else {
        if (ImGui::Button("Start Minigame")) {
            mCurrentFishingMinigame = std::make_unique<FishingMinigame>(*mFishDef);
        }
    }
    FishingMinigameFishData& minigameData = mFishDef->mMinigameData;
    ImGui::Text("Test Minigame Data");
    ImGui::Separator();
    ImGui::SliderFloat2("Acceleration", &minigameData.mAcceleration.x, 0.0f, 2500.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Fish Drag", &minigameData.mFishDrag, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Fish Damage Rate", &minigameData.mFishDamageRate, 0.0f, 1.0f);
    ImGui::SliderFloat("Center Magnitism", &minigameData.mCenterMagnitism, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Gravity", &minigameData.mGravity, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
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
    ImGui::Separator();
    ImGui::Text("Player");
    // Player Data
    // TODO: Shared struct for physics and motion info
    ImGui::SliderFloat("Player Max Speed", &minigameData.mPlayerMaxSpeed, 0.0f, 600.0f);
    ImGui::SliderFloat("Player Acceleration", &minigameData.mPlayerAcceleration, 0.0f, 2500.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Player Drag", &minigameData.mPlayerDrag, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Player Wall Bouncyness", &minigameData.mPlayerWallBouncyness, 0.0f, 1.0f);
    ImGui::SliderFloat2("Player Stickyness", &minigameData.mPlayerStickyness.x, 0.0f, 1.0f);
    ImGui::SliderFloat("Player Strength", &minigameData.mPlayerStrength, 0.0f, 2.0f);
    ImGui::Separator();
    updateAndRenderSharedControls();
    ImGui::Separator();

    ImGui::EndChild();
}

const MaterialShader* FishingEditorViewportPanel::getShader() {
    if (mCurrentFishingMinigame) return nullptr;

    ResourceManager& resourceManager = Services::ResourceManager::ref();
    return resourceManager.getMaterialShaderManager().getMaterialShader("editor_model_pbr");
}

void FishingEditorViewportPanel::renderMesh() {
    if (mCurrentFishingMinigame) {
        FishingMinigameResult result = mCurrentFishingMinigame->updateAndRender(mViewportDims, RenderContext::getInstance().getCurrentFrameElapsedSec());
        if (result.result == MinigameResultType::Fail) {
            LOG_INFO("Fishing failed!");
            mCurrentFishingMinigame = std::make_unique<FishingMinigame>(*mFishDef);
        } else if (result.result == MinigameResultType::Success) {
            LOG_INFO("Fishing success!");
            mCurrentFishingMinigame = std::make_unique<FishingMinigame>(*mFishDef);
        }
    }
    else {
        renderFishModel();
    }
}

void FishingEditorViewportPanel::renderFishModel() {
    ModelRepository& modelRepo = Services::ResourceManager::ref().getModelRepository();
    if (mFishDef) {
        const ModelDef& model = modelRepo.getModelDef(mFishDef->mModel);
        for (int i = 0; i < model.getNumMeshes(); ++i) {
            MeshDrawer::draw(model.getMesh(i).mMainMesh, MeshLODLevel::Highest);
        }
    }
}
