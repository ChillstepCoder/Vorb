#include "stdafx.h"
#include "CPUParticleSystem.h"

#include "rendering/MaterialRenderer.h"

CPUParticleSystem::CPUParticleSystem(size_t reserveEmitterCount) {
    mEmitters.reserve(reserveEmitterCount);
}

CPUParticleSystem::CPUParticleSystem(const ParticleUpdateFunction& updateFunction, ui32 maxParticles, BitFlags<ParticleComponentType> components, const MaterialShader& shader) {
    addEmitter(updateFunction, maxParticles, components, shader);
}

void CPUParticleSystem::updateAndRender(f32 elapsedSec) {
    const MaterialShader* boundShader = nullptr;
    for (auto& emitter : mEmitters) {
        const MaterialShader* nextShader = &emitter->getMaterialShader();
        if (nextShader != boundShader) {
            MaterialRenderer::bindMaterialForRender(*nextShader);
            boundShader = nextShader;
        }
        emitter->updateAndRender(elapsedSec);
    }
}

CpuParticleEmitter& CPUParticleSystem::addEmitter(const ParticleUpdateFunction& updateFunction, ui32 maxParticles, BitFlags<ParticleComponentType> components, const MaterialShader& shader) {
    mEmitters.emplace_back(std::make_unique<CpuParticleEmitter>(updateFunction, maxParticles, components, shader));
    return *mEmitters.back();
}
