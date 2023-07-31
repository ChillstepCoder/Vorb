#include "stdafx.h"
#include "CPUParticleEmitterOperation.h"

#include "CpuParticleEmitter.h"

#include <Vorb/ui/imgui/imgui.h>
#include <Vorb/ui/imgui/backends/imgui_impl_sdl.h>
#include <Vorb/ui/imgui/backends/imgui_impl_opengl3.h>

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
    
    if (mOperation) {
        assert(false);
    }
    else {
        if (std::holds_alternative<f32v4>(mVarData)) [[unlikely]] {
            return ImGui::InputFloat4(label, &std::get<f32v4>(mVarData).x);
        }
        else if (std::holds_alternative<f32v3>(mVarData)) {
            return ImGui::InputFloat3(label, &std::get<f32v3>(mVarData).x);
        }
        else if (std::holds_alternative<f32v2>(mVarData)) {
            return ImGui::InputFloat2(label, &std::get<f32v2>(mVarData).x);
        }
        else if (std::holds_alternative<f32>(mVarData)) {
            return ImGui::InputFloat(label, &std::get<f32>(mVarData));
        }
        else if (std::holds_alternative<ui32>(mVarData)) {
            int v = std::get<ui32>(mVarData);
            bool changed = ImGui::InputInt(label, &v);
            if (v < 0) v = 0;
            mVarData = (ui32)v;
            return changed;
        }
        else if (std::holds_alternative<color4>(mVarData)) {
            color4& color = std::get<color4>(mVarData);
            float colorf[4] = { color.r, color.g, color.b, color.a };
            bool changed = ImGui::ColorPicker4(label, colorf, ImGuiColorEditFlags_Uint8);
            color = color4((ui8)colorf[0], (ui8)colorf[1], (ui8)colorf[2], (ui8)colorf[3]);
            return changed;
        }
    }
    return false;
}
