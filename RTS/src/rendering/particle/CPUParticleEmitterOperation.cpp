#include "stdafx.h"
#include "CPUParticleEmitterOperation.h"

#include "CpuParticleEmitter.h"

#include "Resources/ResourceManager.h"
#include "Resources/ParticleSystemRepository.h"

#include "math/Random.h"

#include <Vorb/ui/imgui/imgui.h>
#include <Vorb/ui/imgui/backends/imgui_impl_sdl.h>
#include <Vorb/ui/imgui/backends/imgui_impl_opengl3.h>

#include "ui/editor/EditorCurve.hpp"

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
            ImGui::SameLine();
            if (ImGui::Button("V")) {
                ImGui::OpenPopup("OperationPopup");
            }
            color = color4((ui8)roundf(colorf[0] * 255.0f), (ui8)roundf(colorf[1] * 255.0f), (ui8)roundf(colorf[2] * 255.0f), (ui8)roundf(colorf[3] * 255.0f));
        }
        static_assert(e_count(CPUparticleEmitterVariableVariantType) == 6);
    }

    auto displayOperationsSelectorCombo = [&]() -> const CPUParticleEmitterOperation* {
        const auto& operations = yml::getAllObjects<CPUParticleEmitterOperation>();
        std::vector<const CPUParticleEmitterOperation*> operationsList;
        for (auto&& iter : operations) {
            const std::unique_ptr< CPUParticleEmitterOperation>& operation = iter.second;
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
            static_assert(e_count(CPUparticleEmitterVariableVariantType) == 6);
            if (matches) {
                operationsList.emplace_back(operation.get());
            }
        }
        // Sort based on color and then name
        std::sort(operationsList.begin(), operationsList.end(), [](const CPUParticleEmitterOperation* a, const CPUParticleEmitterOperation* b) -> bool {
            if (a->getDisplayColor() < b->getDisplayColor()) return true;
            if (a->getDisplayColor() != b->getDisplayColor()) return false; // B > A
            return strcmp(a->getDisplayName(), b->getDisplayName()) < 0;
        });

        for (auto&& op : operationsList) {
            const color4 color = op->getDisplayColor();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(color.r / 255.f, color.g / 255.f, color.b / 255.f, color.a / 255.f));
            if (ImGui::Button(op->getDisplayName(), ImVec2(300.f, 0.f))) {
                ImGui::PopStyleColor(1);
                return op;
            }
            ImGui::PopStyleColor(1);
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

bool CPUParticleEmitterVariable::loadFromYml(ryml::ConstNodeRef node, std::string_view name) {
    ryml::ConstNodeRef thisNode = node[c4::to_csubstr(name)];
    if (!thisNode.valid()) {
        return false;
    }

    if (thisNode.is_map()) {
        ryml::ConstNodeRef objNode = thisNode.child(0);
        c4::csubstr str = objNode.key();
        mOperation = yml::cloneYmlObject<CPUParticleEmitterOperation>(str);
        return mOperation->loadFromYml(objNode);
    }
    else {
        thisNode.operator>>(mVarData);
    }
    return true;
}

void CPUParticleEmitterVariable::saveYmlData(ryml::NodeRef node, std::string_view name) const {
    YmlSerializable::saveWithLambda(node, name, [this](ryml::NodeRef node) {
        if (mOperation) {
            node |= ryml::MAP;
            mOperation->saveYml(node);
        }
        else {
            node.operator<<(mVarData);
        }
    });
}

bool CPUParticleEmitterOperation::updateAndRenderControls() {
    ImVec2 frameMin = ImGui::GetCursorScreenPos(); // Top left of frame
    ImGui::BeginGroup();
    ImGui::Text(getDisplayName());

    bool changed = false;
    for (size_t i = 0; i < mParams.size(); ++i) {
        changed |= mParams[i].updateAndRenderTweaker(getParamName(i));
    }
    changed |= updateAndRenderExtraControls();
   
    ImGui::EndGroup();
    ImVec2 frameMax = ImGui::GetItemRectMax(); // Bottom right of frame
    // Draw a border around the group
    const color4 color = getDisplayColor();
    ImGui::GetWindowDrawList()->AddRect(frameMin, frameMax, IM_COL32(color.r, color.g, color.b, 128));
    return changed;
}

bool CPUParticleEmitterOperation::loadFromYml(ryml::ConstNodeRef node) {
    for (size_t i = 0; i < mParams.size(); ++i) {
        if (!mParams[i].loadFromYml(node, getParamName(i))) return false;
    }
 
    return true;
}

void CPUParticleEmitterOperation::saveYmlData(ryml::NodeRef node) const {
    for (size_t i = 0; i < mParams.size(); ++i) {
        mParams[i].saveYmlData(node, getParamName(i));
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

void CPUPEO_RandomFloatInRange::execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterVariable* output) {
    evaluateParams(emitter, id);
    const f32 p0 = std::get<f32>(mParams[0].mVarData);
    const f32 p1 = std::get<f32>(mParams[1].mVarData);
    output->mVarData = (f32)lerp(p0, p1, mSeedByParticleID ? Random::getCachedRandomfSpecific((ui32)id) : Random::getCachedRandomf());
}
bool CPUPEO_RandomFloatInRange::updateAndRenderExtraControls() {
    return ImGui::Checkbox("Seed By Particle ID", &mSeedByParticleID);
}
bool CPUPEO_RandomFloatInRange::loadFromYml(ryml::ConstNodeRef node) {
    CPUParticleEmitterOperation::loadFromYml(node);
    node["seed_by_p"] >> mSeedByParticleID;
    return true;
}
void CPUPEO_RandomFloatInRange::saveYmlData(ryml::NodeRef node) const {
    CPUParticleEmitterOperation::saveYmlData(node);
    node["seed_by_p"] << mSeedByParticleID;
}

void CPUPEO_ColorCurve::execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterVariable* output) {
    evaluateParams(emitter, id);
    f32 normalizedValue = glm::clamp(std::get<f32>(mParams[0].mVarData), 0.0f, 1.0f);
    output->mVarData = EditorUtil::evaluateCurve<color4>(mKeys, normalizedValue);
}
bool CPUPEO_ColorCurve::updateAndRenderExtraControls() {
    return EditorUtil::updateAndRenderCurve<color4>(mKeys, [](color4& val) {
        f32v4 color = val.toVec4();
        bool changed = ImGui::ColorEdit4("Color Edit", &color.x);
        val = color4(color);
        return changed;
    });
}

YML_WRITE_DEF(std::pair<f32, color4>) {
    ryml::NodeRef& nr = *n;
    nr |= ryml::SEQ;
    nr |= ryml::_WIP_STYLE_FLOW_SL;
    nr.append_child() << o.first;
    for (int i = 0; i < 4; ++i) {
        nr.append_child() << o.second[i];
    }
}
YML_READ_DEF(std::pair<f32, color4>) {
    if (n.num_children() != 5) return false;
    int i = 0;
    
    for (auto const ch : n) {
        if (i == 0) {
            ch >> (*target).first;
            ++i;
        }
        else {
            ch >> (*target).second[i++ - 1];
        }
    }
    return true;
}

bool CPUPEO_ColorCurve::loadFromYml(ryml::ConstNodeRef node) {
    CPUParticleEmitterOperation::loadFromYml(node);
    node["keys"] >> mKeys;
    return true;
}
void CPUPEO_ColorCurve::saveYmlData(ryml::NodeRef node) const {
    CPUParticleEmitterOperation::saveYmlData(node);
    node["keys"] << mKeys;
}


void CPUPEO_HdrColorCurve::execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterVariable* output) {
    evaluateParams(emitter, id);
    f32 normalizedValue = glm::clamp(std::get<f32>(mParams[0].mVarData), 0.0f, 1.0f);
    output->mVarData = EditorUtil::evaluateCurve<f32v4>(mKeys, normalizedValue);
}
bool CPUPEO_HdrColorCurve::updateAndRenderExtraControls() {
    return EditorUtil::updateAndRenderCurve<f32v4>(mKeys, [](f32v4& val) {
        return ImGui::ColorEdit4("Color Edit", &val.x, ImGuiColorEditFlags_HDR);
    });
}

YML_WRITE_DEF(std::pair<f32, f32v4>) {
    ryml::NodeRef& nr = *n;
    nr |= ryml::SEQ;
    nr |= ryml::_WIP_STYLE_FLOW_SL;
    nr.append_child() << o.first;
    for (int i = 0; i < 4; ++i) {
        nr.append_child() << o.second[i];
    }
}
YML_READ_DEF(std::pair<f32, f32v4>) {
    if (n.num_children() != 5) return false;
    int i = 0;

    for (auto const ch : n) {
        if (i == 0) {
            ch >> (*target).first;
            ++i;
        }
        else {
            ch >> (*target).second[i++ - 1];
        }
    }
    return true;
}

bool CPUPEO_HdrColorCurve::loadFromYml(ryml::ConstNodeRef node) {
    CPUParticleEmitterOperation::loadFromYml(node);
    node["keys"] >> mKeys;
    return true;
}
void CPUPEO_HdrColorCurve::saveYmlData(ryml::NodeRef node) const {
    CPUParticleEmitterOperation::saveYmlData(node);
    node["keys"] << mKeys;
}


void CPUPEO_FloatCurve::execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterVariable* output) {
    evaluateParams(emitter, id);
    f32 normalizedValue = glm::clamp(std::get<f32>(mParams[0].mVarData), 0.0f, 1.0f);
    output->mVarData = EditorUtil::evaluateCurve<f32>(mKeys, normalizedValue);
}
bool CPUPEO_FloatCurve::updateAndRenderExtraControls() {
    return EditorUtil::updateAndRenderCurve<f32>(mKeys, [](f32& val) {
        return ImGui::InputFloat("Value", &val);
    });
}
bool CPUPEO_FloatCurve::loadFromYml(ryml::ConstNodeRef node) {
    CPUParticleEmitterOperation::loadFromYml(node);
    node["keys"] >> mKeys;
    return true;
}
void CPUPEO_FloatCurve::saveYmlData(ryml::NodeRef node) const {
    CPUParticleEmitterOperation::saveYmlData(node);
    node["keys"] << mKeys;
}

void CPUPEO_NormalizeVec3::execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterVariable* output) {
    evaluateParams(emitter, id);
    const f32v3 v = std::get<f32v3>(mParams[0].mVarData);
    if (v != f32v3(0.0f)) [[likely]] {
        output->mVarData = glm::normalize(std::get<f32v3>(mParams[0].mVarData));
    }
    else {
        output->mVarData = f32v3(0.0f);
    }
}

void CPUPEO_RandomPointInShape::execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterVariable* output) {
    evaluateParams(emitter, id);
    assert(false);
    X;
}
bool CPUPEO_RandomPointInShape::updateAndRenderExtraControls() {
    bool changed = false;
    assert(false);
    return changed;
}
bool CPUPEO_RandomPointInShape::loadFromYml(ryml::ConstNodeRef node) {
    CPUParticleEmitterOperation::loadFromYml(node);
    assert(false);
    return true;
}
void CPUPEO_RandomPointInShape::saveYmlData(ryml::NodeRef node) const {
    CPUParticleEmitterOperation::saveYmlData(node);
    assert(false);
}
