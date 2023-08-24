#include "stdafx.h"
#include "CPUParticleEmitterOperation.h"

#include "CpuParticleEmitter.h"

#include "Resources/ResourceManager.h"
#include "Resources/ParticleSystemRepository.h"

#include "serialization/YmlSerializer.h"

#include <Vorb/ui/imgui/imgui.h>
#include <Vorb/ui/imgui/backends/imgui_impl_sdl.h>
#include <Vorb/ui/imgui/backends/imgui_impl_opengl3.h>

CPUParticleEmitterVariable::CPUParticleEmitterVariable(const CPUParticleEmitterVariable& other) : mVarData(other.mVarData) {
    if (other.mOperation) {
        mOperation = other.mOperation->clone();
    }
}

void CPUParticleEmitterVariable::evaluate(CpuParticleEmitter& emitter, ParticleID id) {
    // Constants do not evaluate
    if (mOperation) {
        mOperation->execute(emitter, id, this);
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
            changed = true;
            mOperation.reset();
        }
        else {
            constexpr f32 INDENT_WIDTH = 25.0f;
            ImGui::Indent(INDENT_WIDTH);
            changed |= mOperation->updateAndRenderControls();
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
            float colorf[4] = { color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f };
            changed |= ImGui::ColorPicker4(label, colorf, ImGuiColorEditFlags_Uint8);
            color = color4((ui8)roundf(colorf[0] * 255.0f), (ui8)roundf(colorf[1] * 255.0f), (ui8)roundf(colorf[2] * 255.0f), (ui8)roundf(colorf[3] * 255.0f));
        }
        static_assert(e_count(CPUparticleEmitterVariableVariantType) == 7);
    }

    auto displayOperationsSelectorCombo = [&]() -> const CPUParticleEmitterOperation* {
        const auto& operations = Services::ResourceManager::ref().getParticleSystemRepository().getEmitterOperations();
        for (auto&& operation : operations) {
            bool matches = false;
            switch (operation->getOutputType()) {
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
                const color4 color = operation->getDisplayColor();
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(color.r / 255.f, color.g / 255.f, color.b / 255.f, color.a / 255.f));
                if (ImGui::Button(operation->getDisplayName(), ImVec2(300.f, 0.f))) {
                    ImGui::PopStyleColor(1);
                    return operation.get();
                }
                ImGui::PopStyleColor(1);
            }
        }
        return nullptr;
    };

    if (ImGui::BeginPopupModal("OperationPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (const CPUParticleEmitterOperation* operation = displayOperationsSelectorCombo()) {
            changed = true;
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

void CPUParticleEmitterVariable::saveYmlData(keg::YAMLWriter& writer) const
{
    if (mOperation) {
        mOperation->saveYml(writer);
    }
    else {
        std::visit([&](auto&& arg) {
            YmlSerializable::saveValue(writer, arg);
        }, mVarData);
        static_assert(e_count(CPUparticleEmitterVariableVariantType) == 7);
    }
}

bool CPUParticleEmitterOperation::updateAndRenderControls() {
    ImVec2 frameMin = ImGui::GetCursorScreenPos(); // Top left of frame
    ImGui::BeginGroup();
    ImGui::Text(getDisplayName());

    CPUParticleEmitterVariableVariantTypePair inputTypes = getVariantInput();
    bool changed = false;
    if (inputTypes.second != CPUparticleEmitterVariableVariantType::None) {
        changed |= mParam0.updateAndRenderTweaker("A");
        changed |= mParam1.updateAndRenderTweaker("B");
    }
    else if (inputTypes.first != CPUparticleEmitterVariableVariantType::None) {
        changed |= mParam0.updateAndRenderTweaker("Value");
    }
    ImGui::EndGroup();
    ImVec2 frameMax = ImGui::GetItemRectMax(); // Bottom right of frame
    // Draw a border around the group
    const color4 color = getDisplayColor();
    ImGui::GetWindowDrawList()->AddRect(frameMin, frameMax, IM_COL32(color.r, color.g, color.b, 128));
    return changed;
}

bool CPUParticleEmitterOperation::loadFromYml(keg::ReadContext& context, keg::Node node) const
{
    throw std::logic_error("The method or operation is not implemented.");
}

void CPUParticleEmitterOperation::saveYmlData(keg::YAMLWriter& writer) const
{
    CPUParticleEmitterVariableVariantTypePair input = getVariantInput();
    if (input.second != CPUparticleEmitterVariableVariantType::None) {
        assert(input.first != CPUparticleEmitterVariableVariantType::None);
        beginMap(writer);
        pushKeyValue(writer, "p0");
        mParam0.saveYmlData(writer);
        pushKeyValue(writer, "p1");
        mParam1.saveYmlData(writer);
        endMap(writer);
    }
    else if (input.first != CPUparticleEmitterVariableVariantType::None) {
        mParam0.saveYmlData(writer);
    }
}

void CPUPEO_QueryPosition::execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterVariable* output) {
    output->mVarData = emitter.getParticlePosition(id);
}

void CPUPEO_QueryVelocity::execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterVariable* output) {
    output->mVarData = emitter.getParticleVelocity(id);
}

void CPUPEO_QueryScale::execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterVariable* output) {
    output->mVarData = emitter.getParticleScale(id);
}

void CPUPEO_QueryRotation::execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterVariable* output) {
    output->mVarData = emitter.getParticleRotation(id);
}

void CPUPEO_QueryNormalizedLifetime::execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterVariable* output) {
    f32 l = emitter.getParticleNormalizedLifetime(id);
    output->mVarData = l;
}