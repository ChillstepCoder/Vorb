#include "stdafx.h"
#include "AnimMachineEditorViewportPanel.h"

#include "definitions/ModelDef.h"
#include "definitions/RigDef.h"
#include "resources/AnimationRepository.h"
#include "resources/ModelRepository.h"

#include "rendering/model/skeletal/SkeletalAnimator.h"
#include "rendering/MaterialShaderRepository.h"

#include "ui/imgui_controls/ObjectVector.h"

static const char* BottomControlsName = "AnimMachine Controls";

AnimMachineEditorViewportPanel::AnimMachineEditorViewportPanel() {
   
    mGraph = std::make_unique<NodeGraph>();
}

AnimMachineEditorViewportPanel::~AnimMachineEditorViewportPanel() {
}

void AnimMachineEditorViewportPanel::updateAndRenderPrimaryControls(f32 ySize) {

    if (!mAssetData) {
        ImGui::Text("SELECT ASSET");
        return;
    }
    ImGui::Spacing();

    updateAndRenderSaveButton();

    bool changed = false;
    changed = updateAndRenderImguiControls(*mAssetData);
    changed |= ImguiUtil::ObjectVector<AnimStateDef>("States", mAssetData->stateDefs,
        [](AnimStateDef& o, ui32 i) {
        bool changed = updateAndRenderImguiControls(o);
        if (changed) {
            switch (o.stateType) {
                case AnimStateType::AnimSequence:
                    if (o.assetRef.assetType != AssetType::Animation) {
                        o.assetRef.assetType = AssetType::Animation;
                        o.assetRef.invalidate();
                    }
                    break;
                case AnimStateType::Blendspace1D:
                    if (o.assetRef.assetType != AssetType::Blendspace1D) {
                        o.assetRef.assetType = AssetType::Blendspace1D;
                        o.assetRef.invalidate();
                    }
                    break;
                case AnimStateType::Blendspace2D:
                    panic("BlendSpace 2d not implemented yet");
                default:
                    panic("Selected invalid state type");
            }
            static_assert(e_count(AnimStateType) == 3);
        }
        changed |= ImguiUtil::ObjectVector<AnimTransitionDef>("Transitions", o.transitions,
            [](AnimTransitionDef& o, ui32 i) {
                bool changed = updateAndRenderImguiControls(o);
                x; // Rest of controls
                return changed;
            }
        );
        return changed;
    });

    ImGui::Text("Hello world");
}

bool AnimMachineEditorViewportPanel::updateAndRenderSecondaryControls(f32 ySize) {
    ImGui::Text("Hi");
    return true;
}

void AnimMachineEditorViewportPanel::updateAndRenderBottomControls() {
    ImGui::Begin(BottomControlsName, nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNavFocus);
    ImGui::Text("Bottom!");

    if (mDidJustEnter) {
        ImGui::SetWindowFocus(BottomControlsName);
        mDidJustEnter = false;
    }

    mGraph->updateAndRender();

    //ImGui::ShowTestWindow();
    //ImGui::ShowMetricsWindow();
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
