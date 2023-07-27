#include "stdafx.h"
#include "BuiltinCPUParticleEmitterModules.h"

#include "CpuParticleEmitter.h"

#define MODULE_DATA static_cast<ModuleData*>(data)

void CPUPEM_SetPosition::init() {
    mMethod = [](CpuParticleEmitter& emitter, int particleID, void* data) {
        MODULE_DATA->mPositionVec3.evaluate(emitter, particleID);
        emitter.setParticlePosition(particleID, std::get<f32v3>(MODULE_DATA->mPositionVec3.mVarData));
    };
}

void CPUPEM_SetVelocity::init() {
    mMethod = [](CpuParticleEmitter& emitter, int particleID, void* data) {
        MODULE_DATA->mVelocityVec3.evaluate(emitter, particleID);
        emitter.setParticleVelocity(particleID, std::get<f32v3>(MODULE_DATA->mVelocityVec3.mVarData));
    };
}

void CPUPEM_SetColor::init() {
    mMethod = [](CpuParticleEmitter& emitter, int particleID, void* data) {
        MODULE_DATA->mColor.evaluate(emitter, particleID);
        emitter.setParticleColor(particleID, std::get<color4>(MODULE_DATA->mColor.mVarData));
    };
}
