#include "stdafx.h"
#include "AnimMachineEditorViewportPanel.h"

#include "definitions/ModelDef.h"
#include "definitions/RigDef.h"
#include "resources/AnimationRepository.h"
#include "resources/AnimMachineRepository.h"
#include "resources/ModelRepository.h"

#include "rendering/model/skeletal/SkeletalAnimator.h"
#include "rendering/MaterialShaderRepository.h"

#include "ui/imgui_controls/ObjectVector.h"
#include "ui/ImguiUtil.hpp"

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
        [this](AnimStateDef& o, ui32 stateIndex) {
        bool changed = false;
        changed |= ImguiUtil::StrTokenInput("Name", o.name);
        changed |= updateAndRenderImguiControls(o);
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
            [this, stateIndex](AnimTransitionDef& o, ui32 index) {
                bool changed = false;
                if (!o.toState.isValid()) {
                    ImguiUtil::ScopedColor color(ImGuiCol_Text, ImguiColors::Theme::error);
                    ImGui::Text("INVALID TO STATE");
                }
                if (ImGui::BeginCombo("To State", o.toState.toString().c_str())) {
                    for (size_t i = 0; i < mAssetData->stateDefs.size(); ++i) {
                        AnimStateDef& def = mAssetData->stateDefs[i];
                        if (i == stateIndex) {
                            // No self selection
                            continue;
                        }
                        bool isSelected = def.name == o.toState;
                        ImGui::Selectable(def.name.toString().c_str(), &isSelected);
                        if (isSelected) {
                            ImGui::SetItemDefaultFocus();
                            if (def.name != o.toState) {
                                o.toState = def.name;
                                changed = true;
                            }
                        }
                    }
                    ImGui::EndCombo();
                }
                changed |= updateAndRenderImguiControls(o);
                ImGui::Text("Condition:");
                ImGui::Indent();
                if (ImguiUtil::EnumCombo("Type", o.condition.defType)) {
                    changed = true;
                    if (o.condition.isValid()) {
                        const AnimTransitionConditionDef& def = getAnimTransitionConditionDef(o.condition.defType);
                        if (std::holds_alternative<f32>(def.defaultParam)) {
                            if (!std::holds_alternative<f32>(o.condition.param)) {
                                o.condition.param = f32(0.0f);
                            }
                            ImGui::InputFloat("param", &std::get<f32>(o.condition.param));
                        }
                        else if (std::holds_alternative<f32v2>(def.defaultParam)) {
                            if (!std::holds_alternative<f32v2>(o.condition.param)) {
                                o.condition.param = f32v2(0.0f);
                            }
                            ImGui::InputFloat2("param", &std::get<f32v2>(o.condition.param).x);
                        }
                        else {
                            o.condition.param = std::monostate();
                        }
                        static_assert(std::variant_size_v<AnimParamVar> == 3, "Update edit");
                    }
                }
                ImGui::Unindent();
                changed |= ImGui::InputFloat("Transition duration", &o.transitionDuration);
                return changed;
            }
        );
        return changed;
    });

    if (changed) {
        onChanged();
    }

    // For laptop and stuff going off screen
    for (int i = 0; i < 5; ++i) ImGui::Spacing();
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

void AnimMachineEditorViewportPanel::onChanged() {
    AnimMachineRepository::get().onAssetChangedByEditor(mAssetData->getID());
}
