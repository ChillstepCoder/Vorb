#include "stdafx.h"
#include "CPUParticleSystem.h"

#include "definitions/ParticleSystemDef.h"
#include "rendering/MaterialShader.h"

#include "rendering/MaterialRenderer.h"

CPUParticleSystem::CPUParticleSystem(size_t reserveEmitterCount) {
    mEmitters.reserve(reserveEmitterCount);
}

CPUParticleSystem::CPUParticleSystem(const ParticleUpdateFunction& updateFunction, ui32 maxParticles, BitFlags<ParticleComponentType> components, const MaterialShader& shader, f32 particleLifespanSec /*= FLT_MAX*/, f32 emitterLifespanSec /*= FLT_MAX*/) {
    addEmitter(updateFunction, maxParticles, components, shader, particleLifespanSec, emitterLifespanSec);
}

CPUParticleSystem::CPUParticleSystem(const ParticleSystemDef& def)
{
    mSystemID = def.mID;
    mEmitters.reserve(def.mEmitters.size());
    for (auto&& emitterDef : def.mEmitters) {
        mEmitters.emplace_back(std::make_unique<CpuParticleEmitter>(emitterDef));
    }
}

void CPUParticleSystem::updateAndRender(f32 elapsedSec, const f32m4& VP) {
    const MaterialShader* boundShader = nullptr;
    for (auto&& iter = mEmitters.begin(); iter != mEmitters.end();) {
        CpuParticleEmitter& emitter = **iter;
        const MaterialShader* nextShader = &emitter.getMaterialShader();
        if (nextShader != boundShader) {
            MaterialRenderer::bindMaterialForRender(*nextShader);
            glUniformMatrix4fv(nextShader->getUniform("unVP"), 1, false, &VP[0][0]);
            boundShader = nextShader;
        }
        if (emitter.updateAndRender(elapsedSec)) {
            iter = mEmitters.erase(iter);
        }
        else {
            ++iter;
        }
    }
}

CpuParticleEmitter& CPUParticleSystem::addEmitter(const ParticleUpdateFunction& updateFunction, ui32 maxParticles, BitFlags<ParticleComponentType> components, const MaterialShader& shader, f32 particleLifespanSec /*= FLT_MAX*/, f32 emitterLifespanSec /*= FLT_MAX*/) {
    mEmitters.emplace_back(std::make_unique<CpuParticleEmitter>(updateFunction, maxParticles, components, shader, emitterLifespanSec))->setGlobalParticleLifespan(particleLifespanSec);
    return *mEmitters.back();
}
