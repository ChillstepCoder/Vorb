#include "stdafx.h"
#include "AnimMachineEditorViewportPanel.h"

#include "definitions/ModelDef.h"
#include "definitions/RigDef.h"
#include "resources/AnimationRepository.h"
#include "resources/ModelRepository.h"

#include "rendering/model/skeletal/SkeletalAnimator.h"
#include "rendering/MaterialShaderRepository.h"

static const char* BottomControlsName = "AnimMachine Controls";

AnimMachineEditorViewportPanel::AnimMachineEditorViewportPanel()
{

}

AnimMachineEditorViewportPanel::~AnimMachineEditorViewportPanel()
{

}

void AnimMachineEditorViewportPanel::updateAndRenderPrimaryControls(f32 ySize)
{
    ImGui::Spacing();
    ImGui::Text("Hello world");
}

bool AnimMachineEditorViewportPanel::updateAndRenderSecondaryControls(f32 ySize) {
    ImGui::Text("Hi");
    return true;
}

void AnimMachineEditorViewportPanel::updateAndRenderBottomControls() {

    ImGui::Begin(BottomControlsName, nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNavFocus);
    ImGui::Text("BOttom!");
    if (mDidJustEnter) {
        ImGui::SetWindowFocus(BottomControlsName);
        mDidJustEnter = false;
    }

    ImGui::End();
}

const MaterialShaderDef* AnimMachineEditorViewportPanel::getShader() {
    return MaterialShaderRepository::get().getAssetHandle(CStrToken("editor_model_skel"))->tryGetLoadedAsset();
}

void AnimMachineEditorViewportPanel::renderMesh()
{

}

void AnimMachineEditorViewportPanel::onChanged()
{

}
