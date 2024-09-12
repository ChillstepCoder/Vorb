#include "stdafx.h"
#include "CPUParticleEmitterOperation.h"

#include "CpuParticleEmitter.h"

#include "math/Random.h"
#include <imgui.h>

#include "ui/editor/EditorCurve.hpp"

CPUParticleEmitterParameter::CPUParticleEmitterParameter(const CPUParticleEmitterParameter& other) : mVarData(other.mVarData) {
    if (other.mOperation) {
        mOperation = other.mOperation->clone();
    }
}

void CPUParticleEmitterParameter::evaluate(CpuParticleEmitter& emitter, ParticleID id) {
    // Constants do not evaluate
    if (mOperation) {
        mOperation->execute(emitter, id, this);
    }
}

bool CPUParticleEmitterParameter::updateAndRenderTweaker(const char*const label, const ParticleEmitterDef& parentEmitter) {
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
            changed |= mOperation->updateAndRenderControls(parentEmitter);
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
         else if (std::holds_alternative<ParticleEmitterVariableNameUInt>(mVarData)) {
            ParticleEmitterVariableNameUInt& name = std::get<ParticleEmitterVariableNameUInt>(mVarData);
            changed |= ImguiUtil::EnumCombo<ParticleEmitterVariableNameUInt>("Var", name, [&](ParticleEmitterVariableNameUInt v) {
                return std::find(parentEmitter.mUIntVariables.begin(), parentEmitter.mUIntVariables.end(), v) != parentEmitter.mUIntVariables.end();
            });
         }
         else if (std::holds_alternative<ParticleEmitterVariableNameFloat>(mVarData)) {
            ParticleEmitterVariableNameFloat& name = std::get<ParticleEmitterVariableNameFloat>(mVarData);
            changed |= ImguiUtil::EnumCombo<ParticleEmitterVariableNameFloat>("Var", name, [&](ParticleEmitterVariableNameFloat v) {
                return std::find(parentEmitter.mFloatVariables.begin(), parentEmitter.mFloatVariables.end(), v) != parentEmitter.mFloatVariables.end();
            });
         }
         else if (std::holds_alternative<ParticleEmitterVariableNameVec2>(mVarData)) {
            ParticleEmitterVariableNameVec2& name = std::get<ParticleEmitterVariableNameVec2>(mVarData);
            changed |= ImguiUtil::EnumCombo<ParticleEmitterVariableNameVec2>("Var", name, [&](ParticleEmitterVariableNameVec2 v) {
                return std::find(parentEmitter.mVec2Variables.begin(), parentEmitter.mVec2Variables.end(), v) != parentEmitter.mVec2Variables.end();
            });
         }
         else if (std::holds_alternative<ParticleEmitterVariableNameVec3>(mVarData)) {
            ParticleEmitterVariableNameVec3& name = std::get<ParticleEmitterVariableNameVec3>(mVarData);
            changed |= ImguiUtil::EnumCombo<ParticleEmitterVariableNameVec3>("Var", name, [&](ParticleEmitterVariableNameVec3 v) {
                return std::find(parentEmitter.mVec3Variables.begin(), parentEmitter.mVec3Variables.end(), v) != parentEmitter.mVec3Variables.end();
            });
         }
        else {
            assert(false);
        }
        static_assert(e_count(CPUParticleEmitterParameterVariantType) == 10);
    }

    auto displayOperationsSelectorCombo = [&]() -> const CPUParticleEmitterOperation* {
        const auto& operations = yml::getAllObjects<CPUParticleEmitterOperation>();
        std::vector<const CPUParticleEmitterOperation*> operationsList;
        for (auto&& iter : operations) {
            const std::unique_ptr< CPUParticleEmitterOperation>& operation = iter.second;
            bool matches = false;
            switch (operation->getOutputType()) {
                case CPUParticleEmitterParameterVariantType::color4:
                    matches = std::holds_alternative<color4>(mVarData);
                    break;
                case CPUParticleEmitterParameterVariantType::f32v4:
                    matches = std::holds_alternative<f32v4>(mVarData);
                    break;
                case CPUParticleEmitterParameterVariantType::f32v3:
                    matches = std::holds_alternative<f32v3>(mVarData);
                    break;
                case CPUParticleEmitterParameterVariantType::f32v2:
                    matches = std::holds_alternative<f32v2>(mVarData);
                    break;
                case CPUParticleEmitterParameterVariantType::f32:
                    matches = std::holds_alternative<f32>(mVarData);
                    break;
                case CPUParticleEmitterParameterVariantType::ui32:
                    matches = std::holds_alternative<ui32>(mVarData);
                    break;
                case CPUParticleEmitterParameterVariantType::namedUInt:
                    matches = std::holds_alternative<ParticleEmitterVariableNameUInt>(mVarData);
                    break;
                case CPUParticleEmitterParameterVariantType::namedFloat:
                    matches = std::holds_alternative<ParticleEmitterVariableNameFloat>(mVarData);
                    break;
                case CPUParticleEmitterParameterVariantType::namedVec2:
                    matches = std::holds_alternative<ParticleEmitterVariableNameVec2>(mVarData);
                    break;
                case CPUParticleEmitterParameterVariantType::namedVec3:
                    matches = std::holds_alternative<ParticleEmitterVariableNameVec3>(mVarData);
                    break;
                default:
                    assert(false);
            }
            static_assert(e_count(CPUParticleEmitterParameterVariantType) == 10);
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
            if (op->canAddToEmitter(parentEmitter)) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(color.r / 255.f, color.g / 255.f, color.b / 255.f, color.a / 255.f));
                if (ImGui::Button(op->getDisplayName(), ImVec2(300.f, 0.f))) {
                    ImGui::PopStyleColor(1);
                    return op;
                }
                ImGui::PopStyleColor(1);
            }
            else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(50.0f / 255.f, 50.0f / 255.f, 50.0f / 255.f, color.a / 255.f));
                ImGui::Button(op->getDisplayName(), ImVec2(300.f, 0.f));
                ImGui::SetItemTooltip("Missing required components");
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

bool CPUParticleEmitterParameter::loadFromYml(ryml::ConstNodeRef node, std::string_view name) {
    c4::csubstr cname = c4::to_csubstr(name);
    if (!node.has_child(cname)) {
        return false;
    }
    ryml::ConstNodeRef thisNode = node[cname];
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

void CPUParticleEmitterParameter::saveYmlData(ryml::NodeRef node, std::string_view name) const {
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

bool CPUParticleEmitterOperation::updateAndRenderControls(const ParticleEmitterDef& parentEmitter) {
    ImVec2 frameMin = ImGui::GetCursorScreenPos(); // Top left of frame
    ImGui::BeginGroup();
    ImGui::Text(getDisplayName());

    bool changed = false;

    changed |= updateAndRenderExtraPreControls(parentEmitter);
    for (size_t i = 0; i < mParams.size(); ++i) {
        changed |= mParams[i].updateAndRenderTweaker(getParamName(i), parentEmitter);
    }
    changed |= updateAndRenderExtraPostControls(parentEmitter);
   
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

void CPUPEO_QueryPosition::execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterParameter* output) {
    output->mVarData = emitter.getParticlePosition(id);
}

void CPUPEO_QueryVelocity::execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterParameter* output) {
    output->mVarData = emitter.getParticleVelocity(id);
}

void CPUPEO_QuerySpeed::execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterParameter* output) {
    output->mVarData = glm::length(emitter.getParticleVelocity(id));
}

void CPUPEO_QueryScale::execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterParameter* output) {
    output->mVarData = emitter.getParticleScale(id);
}

void CPUPEO_QueryRotation::execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterParameter* output) {
    output->mVarData = emitter.getParticleRotation(id);
}

void CPUPEO_QueryNormalizedLifetime::execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterParameter* output) {
    output->mVarData = emitter.getParticleNormalizedLifetime(id);
}

void CPUPEO_InputImpactDirection::execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterParameter* output) {
    if (emitter.getInputs()) [[likely]] {
        output->mVarData = emitter.getInputs()->getVec3Input(ParticleSystemInputName::Vec3ImpactDirection, f32v3(0.0f, 0.0f, 1.0f));
    }
    else {
        output->mVarData = f32v3(0.0f, 0.0f, 1.0f);
    }
}

void CPUPEO_InputImpactSurfaceNormal::execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterParameter* output) {
    if (emitter.getInputs()) [[likely]] {
        output->mVarData = emitter.getInputs()->getVec3Input(ParticleSystemInputName::Vec3ImpactSurfaceNormal, f32v3(0.0f, 1.0f, 0.0f));
    }
    else {
        output->mVarData = f32v3(0.0f, 1.0f, 0.0f);
    }
}

void CPUPEO_UIntVariable::execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterParameter* output) {
    output->mVarData = emitter.getUIntVariable(std::get<ParticleEmitterVariableNameUInt>(mParams[0].mVarData), id);
}

void CPUPEO_FloatVariable::execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterParameter* output) {
    output->mVarData = emitter.getFloatVariable(std::get<ParticleEmitterVariableNameFloat>(mParams[0].mVarData), id);
}

void CPUPEO_Vec2Variable::execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterParameter* output) {
    output->mVarData = emitter.getVec2Variable(std::get<ParticleEmitterVariableNameVec2>(mParams[0].mVarData), id);
}

void CPUPEO_Vec3Variable::execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterParameter* output) {
    output->mVarData = emitter.getVec3Variable(std::get<ParticleEmitterVariableNameVec3>(mParams[0].mVarData), id);
}

void CPUPEO_RandomFloatInRange::execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterParameter* output) {
    evaluateParams(emitter, id);
    const f32 p0 = std::get<f32>(mParams[0].mVarData);
    const f32 p1 = std::get<f32>(mParams[1].mVarData);
    output->mVarData = (f32)util::lerp(p0, p1, mSeedByParticleID ? Random::getCachedRandomfSpecific((ui32)id) : Random::getCachedRandomf());
}
bool CPUPEO_RandomFloatInRange::updateAndRenderExtraPostControls(const ParticleEmitterDef& parentEmitter) {
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

void CPUPEO_ColorCurve::execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterParameter* output) {
    evaluateParams(emitter, id);
    f32 normalizedValue = glm::clamp(std::get<f32>(mParams[0].mVarData), 0.0f, 1.0f);
    output->mVarData = EditorUtil::evaluateCurve<color4>(mKeys, normalizedValue);
}
bool CPUPEO_ColorCurve::updateAndRenderExtraPostControls(const ParticleEmitterDef& parentEmitter) {
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


void CPUPEO_HdrColorCurve::execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterParameter* output) {
    evaluateParams(emitter, id);
    f32 normalizedValue = glm::clamp(std::get<f32>(mParams[0].mVarData), 0.0f, 1.0f);
    output->mVarData = EditorUtil::evaluateCurve<f32v4>(mKeys, normalizedValue);
}
bool CPUPEO_HdrColorCurve::updateAndRenderExtraPostControls(const ParticleEmitterDef& parentEmitter) {
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


void CPUPEO_FloatCurve::execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterParameter* output) {
    evaluateParams(emitter, id);
    f32 normalizedValue = glm::clamp(std::get<f32>(mParams[0].mVarData), 0.0f, 1.0f);
    output->mVarData = EditorUtil::evaluateCurve<f32>(mKeys, normalizedValue);
}
bool CPUPEO_FloatCurve::updateAndRenderExtraPostControls(const ParticleEmitterDef& parentEmitter) {
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

void CPUPEO_NormalizeVec3::execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterParameter* output) {
    evaluateParams(emitter, id);
    const f32v3 v = std::get<f32v3>(mParams[0].mVarData);
    if (v != f32v3(0.0f)) [[likely]] {
        output->mVarData = glm::normalize(std::get<f32v3>(mParams[0].mVarData));
    }
    else {
        output->mVarData = f32v3(0.0f);
    }
}

void CPUPEO_RandomPointInShape::execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterParameter* output) {
    evaluateParams(emitter, id);

    switch (mShapeType) {
        case QueryPointFromShapeType::Sphere: {
            f32 radius = std::get<f32>(mParams[0].mVarData);
            output->mVarData = util::queryRandomPointFromSphere(radius);
            break;
        }
        case QueryPointFromShapeType::Box: {
            f32v3 halfExtents = std::get<f32v3>(mParams[0].mVarData);
            output->mVarData = util::queryRandomPointFromBox(halfExtents);
            break;
        }
        default:
            assert(false);
            break;

    }
    static_assert(e_count(QueryPointFromShapeType) == 2);
}
const char* const CPUPEO_RandomPointInShape::getParamName(size_t paramIndex) const {
    switch (mShapeType) {
        case QueryPointFromShapeType::Sphere:
            assert(paramIndex == 0);
            return "radius";
        case QueryPointFromShapeType::Box:
            assert(paramIndex == 0);
            return "half_extents";
        default:
            panic("Unknown param");
            break;
    }
    static_assert(e_count(QueryPointFromShapeType) == 2);
}
bool CPUPEO_RandomPointInShape::updateAndRenderExtraPreControls(const ParticleEmitterDef& parentEmitter) {
    bool changed = false;
    
    static constexpr const char* shapeNames[e_count(QueryPointFromShapeType)] = {
        "Sphere",
        "Box"
    };
    static_assert(e_count(QueryPointFromShapeType) == 2);

    if (ImGui::Combo("Shape", (int*)&mShapeType, shapeNames, e_count(QueryPointFromShapeType))) {
        changed = true;
        onShapeTypeUpdated();
        static_assert(e_count(QueryPointFromShapeType) == 2);
    }
    ImGui::Spacing();

    return changed;
}
bool CPUPEO_RandomPointInShape::loadFromYml(ryml::ConstNodeRef node) {
    node["type"] >> mShapeType;
    onShapeTypeUpdated();
    CPUParticleEmitterOperation::loadFromYml(node);
    return true;
}
void CPUPEO_RandomPointInShape::saveYmlData(ryml::NodeRef node) const {
    node["type"] << mShapeType;
    CPUParticleEmitterOperation::saveYmlData(node);
}

void CPUPEO_RandomPointInShape::onShapeTypeUpdated() {
    switch (mShapeType) {
        case QueryPointFromShapeType::Sphere:
            mParams = std::vector<CPUParticleEmitterParameter>{ CPUParticleEmitterVariantData(f32(1.0f)) };
            break;
        case QueryPointFromShapeType::Box:
            mParams = std::vector<CPUParticleEmitterParameter>{ CPUParticleEmitterVariantData(f32v3(1.0f)) };
            break;
        default:
            assert(false);
            break;
    }
    static_assert(e_count(QueryPointFromShapeType) == 2);
}

void CPUPEO_ClampFloat::execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterParameter* output) {
    evaluateParams(emitter, id);
    const f32 val = std::get<f32>(mParams[0].mVarData);
    const f32v2 range = std::get<f32v2>(mParams[1].mVarData);
    output->mVarData = glm::clamp(val, range.x, range.y);
}

void CPUPEO_ClampVec2::execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterParameter* output) {
    evaluateParams(emitter, id);
    const f32v2 val = std::get<f32v2>(mParams[0].mVarData);
    const f32v2 range = std::get<f32v2>(mParams[1].mVarData);
    output->mVarData = glm::clamp(val, range.x, range.y);
}

void CPUPEO_ClampVec3::execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterParameter* output) {
    evaluateParams(emitter, id);
    const f32v3 val = std::get<f32v3>(mParams[0].mVarData);
    const f32v2 range = std::get<f32v2>(mParams[1].mVarData);
    output->mVarData = glm::clamp(val, range.x, range.y);
}

void CPUPEO_MakeVec2::execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterParameter* output) {
    evaluateParams(emitter, id);
    const f32 x = std::get<f32>(mParams[0].mVarData);
    const f32 y = std::get<f32>(mParams[1].mVarData);
    output->mVarData = f32v2(x, y);
}

void CPUPEO_MakeVec3::execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterParameter* output) {
    evaluateParams(emitter, id);
    const f32 x = std::get<f32>(mParams[0].mVarData);
    const f32 y = std::get<f32>(mParams[1].mVarData);
    const f32 z = std::get<f32>(mParams[2].mVarData);
    output->mVarData = f32v3(x, y, z);
}

void CPUPEO_LerpFloat::execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterParameter* output) {
    evaluateParams(emitter, id);
    output->mVarData = util::lerp(std::get<f32>(mParams[0].mVarData), std::get<f32>(mParams[1].mVarData), std::get<f32>(mParams[2].mVarData));
}

void CPUPEO_LerpVec2::execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterParameter* output) {
    evaluateParams(emitter, id);
    output->mVarData = util::lerp(std::get<f32v2>(mParams[0].mVarData), std::get<f32v2>(mParams[1].mVarData), std::get<f32>(mParams[2].mVarData));
}

void CPUPEO_LerpVec3::execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterParameter* output) {
    evaluateParams(emitter, id);
    output->mVarData = util::lerp(std::get<f32v3>(mParams[0].mVarData), std::get<f32v3>(mParams[1].mVarData), std::get<f32>(mParams[2].mVarData));
}