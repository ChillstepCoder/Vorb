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
    mSystemID = def.getID();
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
        if (!nextShader) {
            ++iter;
            continue;
        }
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

void CPUParticleSystem::updateAndRenderEditor(f32 elapsedSec, const f32m4& VP, const std::vector<bool>& emitterVisibility) {
    int i = 0;
    const MaterialShader* boundShader = nullptr;
    if (emitterVisibility.size() != mEmitters.size()) return; // Happens first time
    for (auto&& iter = mEmitters.begin(); iter != mEmitters.end(); ++iter, ++i) {
        if (*iter && emitterVisibility[i]) {
            CpuParticleEmitter& emitter = **iter;
            const MaterialShader* nextShader = &emitter.getMaterialShader();
            if (!nextShader) {
                continue;
            }
            if (nextShader != boundShader) {
                MaterialRenderer::bindMaterialForRender(*nextShader);
                glUniformMatrix4fv(nextShader->getUniform("unVP"), 1, false, &VP[0][0]);
                boundShader = nextShader;
            }
            if (emitter.updateAndRender(elapsedSec)) {
                // Editor doesn't remove from the vector
                iter->reset();
            }
        }
    }
}

CpuParticleEmitter& CPUParticleSystem::addEmitter(const ParticleUpdateFunction& updateFunction, ui32 maxParticles, BitFlags<ParticleComponentType> components, const MaterialShader& shader, f32 particleLifespanSec /*= FLT_MAX*/, f32 emitterLifespanSec /*= FLT_MAX*/) {
    mEmitters.emplace_back(std::make_unique<CpuParticleEmitter>(updateFunction, maxParticles, components, shader, emitterLifespanSec))->setGlobalParticleLifespan(particleLifespanSec);
    return *mEmitters.back();
}
