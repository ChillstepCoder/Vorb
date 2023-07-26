#include "stdafx.h"
#include "BuiltinCPUParticleEmitterModules.h"

#include "CpuParticleEmitter.h"

#define MODULE_DATA static_cast<ModuleData*>(data)

void CPUPEM_SetPosition::refresh() {
    mMethod = [](CpuParticleEmitter& emitter, int particleID, void* data) {
        emitter.setParticlePosition(particleID, MODULE_DATA->mPosition);
    };
}

void CPUPEM_SetVelocity::refresh() {
    mMethod = [](CpuParticleEmitter& emitter, int particleID, void* data) {
        emitter.setParticleVelocity(particleID, MODULE_DATA->mVelocity);
    };
}

void CPUPEM_SetColor::refresh() {
    mMethod = [](CpuParticleEmitter& emitter, int particleID, void* data) {
        emitter.setParticleColor(particleID, MODULE_DATA->mColor);
    };
}
