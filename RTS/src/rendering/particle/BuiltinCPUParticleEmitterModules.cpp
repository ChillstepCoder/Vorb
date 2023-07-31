#include "stdafx.h"
#include "BuiltinCPUParticleEmitterModules.h"

#include "CpuParticleEmitter.h"

#include <Vorb/ui/imgui/imgui.h>
#include <Vorb/ui/imgui/backends/imgui_impl_sdl.h>
#include <Vorb/ui/imgui/backends/imgui_impl_opengl3.h>

#define MODULE_DATA static_cast<ModuleData*>(data)


void CPUPEM_SpawnBurst::init() {
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
    changed |= mModuleData.mDelay.updateAndRenderTweaker("Delay");
    changed |= mModuleData.mSpawnCount.updateAndRenderTweaker("Spawn Count");
    return changed;
}

void CPUPEM_SpawnRate::init() {
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
    changed |= mModuleData.mEmitRateSec.updateAndRenderTweaker("Spawn Rate");
    changed |= mModuleData.mNextEmitTime.updateAndRenderTweaker("Initial Delay");
    changed |= mModuleData.mSpawnCount.updateAndRenderTweaker("Spawn Count");
    return changed;
}

void CPUPEM_SetPosition::init() {
    mMethod = [](CpuParticleEmitter& emitter, int particleID, void* data) {
        MODULE_DATA->mPositionVec3.evaluate(emitter, particleID);
        emitter.setParticlePosition(particleID, std::get<f32v3>(MODULE_DATA->mPositionVec3.mVarData));
    };
}

bool CPUPEM_SetPosition::updateAndRenderEditorControls() {
    return mModuleData.mPositionVec3.updateAndRenderTweaker("Position");
}

void CPUPEM_SetVelocity::init() {
    mMethod = [](CpuParticleEmitter& emitter, int particleID, void* data) {
        MODULE_DATA->mVelocityVec3.evaluate(emitter, particleID);
        emitter.setParticleVelocity(particleID, std::get<f32v3>(MODULE_DATA->mVelocityVec3.mVarData));
    };
}

bool CPUPEM_SetVelocity::updateAndRenderEditorControls() {
    return mModuleData.mVelocityVec3.updateAndRenderTweaker("Velocity");
}

void CPUPEM_SetColor::init() {
    mMethod = [](CpuParticleEmitter& emitter, int particleID, void* data) {
        MODULE_DATA->mColor.evaluate(emitter, particleID);
        emitter.setParticleColor(particleID, std::get<color4>(MODULE_DATA->mColor.mVarData));
    };
}

bool CPUPEM_SetColor::updateAndRenderEditorControls() {
    return mModuleData.mColor.updateAndRenderTweaker("Color");
}
