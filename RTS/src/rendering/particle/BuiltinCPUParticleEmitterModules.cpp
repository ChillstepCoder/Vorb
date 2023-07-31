#include "stdafx.h"
#include "BuiltinCPUParticleEmitterModules.h"

#include "CpuParticleEmitter.h"

#include <Vorb/ui/imgui/imgui.h>
#include <Vorb/ui/imgui/backends/imgui_impl_sdl.h>
#include <Vorb/ui/imgui/backends/imgui_impl_opengl3.h>

#define MODULE_DATA static_cast<ModuleData*>(data)


bool updateAndRenderVariable(CPUParticleEmitterVariable& variable, const char* const label) {
    bool changed = variable.updateAndRenderTweaker(label);
    // Separator after operations
    if (variable.mOperation) {
        ImGui::Separator();
    }
    return changed;
}

CPUPEM_SpawnBurst::CPUPEM_SpawnBurst() {
    mMethod = [](CpuParticleEmitter& emitter, int particleID, void* data) {
        ModuleData* moduleData = MODULE_DATA;
        if (moduleData->mFired) return;

        moduleData->mDelay.evaluate(emitter, particleID);
        f32 delay = std::get<f32>(moduleData->mDelay.mVarData);
        if (emitter.getTotalElapsedSec() >= delay) {
            moduleData->mFired = true;
            moduleData->mSpawnCount.evaluate(emitter, particleID);
            ui32 spawnCount = std::get<ui32>(moduleData->mSpawnCount.mVarData);
            emitter.emitParticles(spawnCount);
        }
    };
}

bool CPUPEM_SpawnBurst::updateAndRenderEditorControls() {
    bool changed = false;
    changed |= updateAndRenderVariable(mModuleData.mDelay, "Delay Sec");
    changed |= updateAndRenderVariable(mModuleData.mSpawnCount, "Spawn Count");
    return changed;
}

CPUPEM_SpawnRate::CPUPEM_SpawnRate() {
    mMethod = [](CpuParticleEmitter& emitter, int particleID, void* data) {
        ModuleData* moduleData = MODULE_DATA;

        moduleData->mNextEmitTime.evaluate(emitter, particleID);

        const f32 timeDiff = emitter.getTotalElapsedSec() - std::get<f32>(moduleData->mNextEmitTime.mVarData);
        if (timeDiff >= 0.0f) {
            moduleData->mSpawnCount.evaluate(emitter, particleID);
            moduleData->mEmitRateSec.evaluate(emitter, particleID);
            emitter.emitParticles(std::get<ui32>(moduleData->mSpawnCount.mVarData));
            moduleData->mNextEmitTime.mVarData = emitter.getTotalElapsedSec() + std::get<f32>(moduleData->mEmitRateSec.mVarData) - timeDiff;
        }
    };
}

bool CPUPEM_SpawnRate::updateAndRenderEditorControls() {
    bool changed = false;
    changed |= updateAndRenderVariable(mModuleData.mEmitRateSec, "Spawn Rate Sec");
    changed |= updateAndRenderVariable(mModuleData.mNextEmitTime, "Initial Delay Sec");
    changed |= updateAndRenderVariable(mModuleData.mSpawnCount, "Spawn Count");
    return changed;
}

CPUPEM_SetPosition::CPUPEM_SetPosition() {
    mMethod = [](CpuParticleEmitter& emitter, int particleID, void* data) {
        MODULE_DATA->mPositionVec3.evaluate(emitter, particleID);
        emitter.setParticlePosition(particleID, std::get<f32v3>(MODULE_DATA->mPositionVec3.mVarData));
    };
}

bool CPUPEM_SetPosition::updateAndRenderEditorControls() {
    return updateAndRenderVariable(mModuleData.mPositionVec3, "Position");
}

CPUPEM_SetVelocity::CPUPEM_SetVelocity() {
    mMethod = [](CpuParticleEmitter& emitter, int particleID, void* data) {
        MODULE_DATA->mVelocityVec3.evaluate(emitter, particleID);
        emitter.setParticleVelocity(particleID, std::get<f32v3>(MODULE_DATA->mVelocityVec3.mVarData));
    };
}

bool CPUPEM_SetVelocity::updateAndRenderEditorControls() {
    return updateAndRenderVariable(mModuleData.mVelocityVec3, "Velocity");
}

CPUPEM_SetColor::CPUPEM_SetColor() {
    mMethod = [](CpuParticleEmitter& emitter, int particleID, void* data) {
        MODULE_DATA->mColor.evaluate(emitter, particleID);
        emitter.setParticleColor(particleID, std::get<color4>(MODULE_DATA->mColor.mVarData));
    };
}

bool CPUPEM_SetColor::updateAndRenderEditorControls() {
    return updateAndRenderVariable(mModuleData.mColor, "Color");
}
