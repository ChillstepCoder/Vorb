#include "stdafx.h"
#include "BuiltinCPUParticleEmitterModules.h"

#include "CpuParticleEmitter.h"

#include <Vorb/ui/imgui/imgui.h>
#include <Vorb/ui/imgui/backends/imgui_impl_sdl.h>
#include <Vorb/ui/imgui/backends/imgui_impl_opengl3.h>

#include "math/Random.h"

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
    refresh();
}

void CPUPEM_SpawnBurst::refresh() {
    mMethod = [](CpuParticleEmitter& emitter, int particleID, void* data, f32 elapsedSec) {
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
    refresh();
}

void CPUPEM_SpawnRate::refresh() {
    mMethod = [](CpuParticleEmitter& emitter, int particleID, void* data, f32 elapsedSec) {
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
    refresh();
}

void CPUPEM_SetPosition::refresh() {
    mMethod = [](CpuParticleEmitter& emitter, int particleID, void* data, f32 elapsedSec) {
        MODULE_DATA->mPositionVec3.evaluate(emitter, particleID);
        emitter.setParticlePosition(particleID, std::get<f32v3>(MODULE_DATA->mPositionVec3.mVarData));
    };
}

bool CPUPEM_SetPosition::updateAndRenderEditorControls() {
    return updateAndRenderVariable(mModuleData.mPositionVec3, "Position");
}

CPUPEM_SetVelocity::CPUPEM_SetVelocity() {
    refresh();
}

void CPUPEM_SetVelocity::refresh() {
    mMethod = [](CpuParticleEmitter& emitter, int particleID, void* data, f32 elapsedSec) {
        MODULE_DATA->mVelocityVec3.evaluate(emitter, particleID);
        emitter.setParticleVelocity(particleID, std::get<f32v3>(MODULE_DATA->mVelocityVec3.mVarData));
    };
}

bool CPUPEM_SetVelocity::updateAndRenderEditorControls() {
    return updateAndRenderVariable(mModuleData.mVelocityVec3, "Velocity");
}

CPUPEM_SetColor::CPUPEM_SetColor() {
    refresh();
}

void CPUPEM_SetColor::refresh() {
    mMethod = [](CpuParticleEmitter& emitter, int particleID, void* data, f32 elapsedSec) {
        MODULE_DATA->mColor.evaluate(emitter, particleID);
        emitter.setParticleColor(particleID, std::get<color4>(MODULE_DATA->mColor.mVarData));
    };
}

bool CPUPEM_SetColor::updateAndRenderEditorControls() {
    return updateAndRenderVariable(mModuleData.mColor, "Color");
}

CPUPEM_SetScale::CPUPEM_SetScale() {
    refresh();
}

void CPUPEM_SetScale::refresh() {
    mMethod = [](CpuParticleEmitter& emitter, int particleID, void* data, f32 elapsedSec) {
        MODULE_DATA->mScale.evaluate(emitter, particleID);
        emitter.setParticleScale(particleID, std::get<f32v2>(MODULE_DATA->mScale.mVarData));
    };
}

bool CPUPEM_SetScale::updateAndRenderEditorControls() {
    return updateAndRenderVariable(mModuleData.mScale, "Scale");
}

CPUPEM_SetPositionFromShape::CPUPEM_SetPositionFromShape() {
    refresh();
}

void CPUPEM_SetPositionFromShape::refresh() {

    switch (mModuleData.mShapeType) {
        case ShapeType::Sphere:
            mMethod = [](CpuParticleEmitter& emitter, int particleID, void* data, f32 elapsedSec) {
                MODULE_DATA->mRadius.evaluate(emitter, particleID);
                f32 theta = 2.f * M_PIF * Random::getCachedRandomf(); // azimuthal angle
                f32 phi = acosf(2.f * Random::getCachedRandomf() - 1.f); // polar angle
                f32 r = std::get<f32>(MODULE_DATA->mRadius.mVarData) * Random::getCachedRandomf(); // cube root to ensure points are uniformly distributed

                f32v3 point;
                point.x = r * sin(phi) * cos(theta);
                point.y = r * sin(phi) * sin(theta);
                point.z = r * cos(phi);
                emitter.setParticlePosition(particleID, point);
            };
            break;
        case ShapeType::Box:
            mMethod = [](CpuParticleEmitter& emitter, int particleID, void* data, f32 elapsedSec) {
                assert(false);
            };
            break;
        default:
            assert(false);
            break;
    }
    static_assert(e_count(ShapeType) == 2);
}

bool CPUPEM_SetPositionFromShape::updateAndRenderEditorControls() {
    bool changed = false;
    changed |= updateAndRenderVariable(mModuleData.mRadius, "Radius");

    const char* itemNames[e_count(ShapeType)] = {
        "Sphere",
        "Box"
    };
    static_assert(e_count(ShapeType) == 2);

    changed |= ImGui::Combo("Shape", (int*)&mModuleData.mShapeType, itemNames, e_count(ShapeType));

    return changed;
}

CPUPEM_ApplyForce::CPUPEM_ApplyForce() {
    refresh();
}

void CPUPEM_ApplyForce::refresh() {
    mMethod = [](CpuParticleEmitter& emitter, int particleID, void* data, f32 elapsedSec) {
        MODULE_DATA->mForce.evaluate(emitter, particleID);
        f32v3 velocity = emitter.getParticleVelocity(particleID);
        velocity += elapsedSec * std::get<f32v3>(MODULE_DATA->mForce.mVarData);
        emitter.setParticleVelocity(particleID, velocity);
    };
}

bool CPUPEM_ApplyForce::updateAndRenderEditorControls() {
    return updateAndRenderVariable(mModuleData.mForce, "Force");
}