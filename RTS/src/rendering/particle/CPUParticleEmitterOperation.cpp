#include "stdafx.h"
#include "CPUParticleEmitterOperation.h"

#include "CpuParticleEmitter.h"

#include "Resources/ResourceManager.h"
#include "Resources/ParticleSystemRepository.h"

#include <Vorb/ui/imgui/imgui.h>
#include <Vorb/ui/imgui/backends/imgui_impl_sdl.h>
#include <Vorb/ui/imgui/backends/imgui_impl_opengl3.h>

CPUParticleEmitterVariable::CPUParticleEmitterVariable(const CPUParticleEmitterVariable& other) : mVarData(other.mVarData), mType(other.mType) {
    if (other.mOperation) {
        mOperation = other.mOperation->clone();
    }
}

void CPUParticleEmitterVariable::evaluate(CpuParticleEmitter& emitter, ParticleID id) {
    // Constants do not evaluate
    if (mType > CPUParticleEmitterVariableType::Constant) {
        if (mType == CPUParticleEmitterVariableType::Operation) {
            mOperation->execute(emitter, id, this);
        }
        else {
            emitter.fillVariableFromType(*this, id, mType);
        }
    }
}

bool CPUParticleEmitterVariable::updateAndRenderTweaker(const char*const label) {
    ImGui::PushID((int)this);
    const ImVec2 contentAvail = ImGui::GetContentRegionAvail();
    const float itemWidth = glm::max(contentAvail.x - 150, 10.0f);

#define INPUT_DEF(xxx) \
    ImGui::PushItemWidth(itemWidth); changed |= xxx; ImGui::PopItemWidth(); ImGui::SameLine(); \
    if (ImGui::Button("V")) { \
        ImGui::OpenPopup("OperationPopup"); \
    }

    bool changed = false;
    if (mOperation) {
        ImGui::Text(label);
        ImGui::SameLine();
        if (ImGui::Button("X")) {
            mOperation.reset();
        }
        else {
            constexpr f32 INDENT_WIDTH = 25.0f;
            ImGui::Indent(INDENT_WIDTH);
            mOperation->updateAndRenderControls();
            ImGui::Unindent(INDENT_WIDTH);
        }
    }
    else {
        if (std::holds_alternative<f32v4>(mVarData)) [[unlikely]] {
            INPUT_DEF(ImGui::InputFloat4(label, &std::get<f32v4>(mVarData).x));
        }
        else if (std::holds_alternative<f32v3>(mVarData)) {
            INPUT_DEF(ImGui::InputFloat3(label, &std::get<f32v3>(mVarData).x));
        }
        else if (std::holds_alternative<f32v2>(mVarData)) {
            INPUT_DEF(ImGui::InputFloat2(label, &std::get<f32v2>(mVarData).x));
        }
        else if (std::holds_alternative<f32>(mVarData)) {
            INPUT_DEF(ImGui::InputFloat(label, &std::get<f32>(mVarData)));
        }
        else if (std::holds_alternative<ui32>(mVarData)) {
            int v = std::get<ui32>(mVarData);
            INPUT_DEF(ImGui::InputInt(label, &v));
            if (v < 0) v = 0;
            mVarData = (ui32)v;
        }
        else if (std::holds_alternative<color4>(mVarData)) {
            color4& color = std::get<color4>(mVarData);
            float colorf[4] = { color.r, color.g, color.b, color.a };
            changed |= ImGui::ColorPicker4(label, colorf, ImGuiColorEditFlags_Uint8);
            color = color4((ui8)colorf[0], (ui8)colorf[1], (ui8)colorf[2], (ui8)colorf[3]);
        }
    }

    auto displayOperationsSelectorCombo = [&]() -> const CPUParticleEmitterOperation* {
        int itemCount = 0;
        const char* items[256];
        const CPUParticleEmitterOperation* operationsCopy[256];
        static int itemSelected = -1; // If the selection isn't within 0..count, Combo won't display a preview
        const auto& operations = Services::ResourceManager::ref().getParticleSystemRepository().getEmitterOperations();
        for (auto&& operation : operations) {
            bool matches = false;
            switch (operation->getVariantInput().first) {
                case CPUparticleEmitterVariableVariantType::color4:
                    matches = std::holds_alternative<color4>(mVarData);
                    break;
                case CPUparticleEmitterVariableVariantType::f32v4:
                    matches = std::holds_alternative<f32v4>(mVarData);
                    break;
                case CPUparticleEmitterVariableVariantType::f32v3:
                    matches = std::holds_alternative<f32v3>(mVarData);
                    break;
                case CPUparticleEmitterVariableVariantType::f32v2:
                    matches = std::holds_alternative<f32v2>(mVarData);
                    break;
                case CPUparticleEmitterVariableVariantType::f32:
                    matches = std::holds_alternative<f32>(mVarData);
                    break;
                case CPUparticleEmitterVariableVariantType::ui32:
                    matches = std::holds_alternative<ui32>(mVarData);
                    break;
                default:
                    assert(false);
            }
            static_assert(e_count(CPUparticleEmitterVariableVariantType) == 7);
            if (matches) {
                operationsCopy[itemCount] = operation.get();
                items[itemCount++] = operation->getDisplayName();
            }
        }
        if (ImGui::Combo("Select", &itemSelected, items, itemCount)) {
            return operationsCopy[itemSelected];
        }
        return nullptr;
    };

    if (ImGui::BeginPopupModal("OperationPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (const CPUParticleEmitterOperation* operation = displayOperationsSelectorCombo()) {
            mOperation = operation->clone();
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::PopID();
    return changed;
}

void CPUParticleEmitterOperation::updateAndRenderControls() {
    ImVec2 frameMin = ImGui::GetCursorScreenPos(); // Top left of frame
    ImGui::BeginGroup();
    ImGui::Text(getDisplayName());

    CPUParticleEmitterVariableVariantTypePair inputTypes = getVariantInput();

    if (inputTypes.second != CPUparticleEmitterVariableVariantType::None) {
        mParam0.updateAndRenderTweaker("A");
        mParam1.updateAndRenderTweaker("B");
    }
    else {
        mParam0.updateAndRenderTweaker("Value");
    }
    ImGui::EndGroup();
    ImVec2 frameMax = ImGui::GetItemRectMax(); // Bottom right of frame
    // Draw a border around the group
    ImGui::GetWindowDrawList()->AddRect(frameMin, frameMax, IM_COL32(255, 255, 255, 100));
}
