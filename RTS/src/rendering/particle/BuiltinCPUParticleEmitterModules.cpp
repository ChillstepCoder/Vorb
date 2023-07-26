#include "stdafx.h"
#include "BuiltinCPUParticleEmitterModules.h"

#include "CpuParticleEmitter.h"

#define MODULE_DATA static_cast<ModuleData*>(data)

CPUPEM_SetPosition::CPUPEM_SetPosition() {
    mMethod = [](CpuParticleEmitter& emitter, int particleID, void* data) {
        emitter.setParticlePosition(particleID, MODULE_DATA->mPosition);
    };
}

CPUPEM_SetVelocity::CPUPEM_SetVelocity() {
    mMethod = [](CpuParticleEmitter& emitter, int particleID, void* data) {
        emitter.setParticleVelocity(particleID, MODULE_DATA->mVelocity);
    };
}

CPUPEM_SetColor::CPUPEM_SetColor() {
    mMethod = [](CpuParticleEmitter& emitter, int particleID, void* data) {
        emitter.setParticleColor(particleID, MODULE_DATA->mColor);
    };
}
