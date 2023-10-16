#include "stdafx.h"
#include "CPUParticleSystem.h"

#include "definitions/ParticleSystemDef.h"
#include "rendering/MaterialShaderDef.h"

#include "rendering/MaterialRenderer.h"

POOLED_ALLOC_DEF_NOT_THREADSAFE(CPUParticleSystem, 32, ASSERT_RENDER_THREAD());

CPUParticleSystem::CPUParticleSystem(size_t reserveEmitterCount, f32 lifespanSec) : mLifetimeRemaining(lifespanSec) {
    mEmitters.reserve(reserveEmitterCount);
}

CPUParticleSystem::CPUParticleSystem(const ParticleUpdateFunction& updateFunction, ui32 maxParticles, BitFlags<ParticleComponentType> components, const MaterialShaderDef& shader, f32 particleLifespanSec /*= FLT_MAX*/, f32 emitterLifespanSec /*= FLT_MAX*/) {
    mLifetimeRemaining = emitterLifespanSec;
    addEmitter(updateFunction, maxParticles, components, shader, particleLifespanSec, emitterLifespanSec);
}

CPUParticleSystem::CPUParticleSystem(const ParticleSystemDef& def, f32v3 position) : mRootPosition(position) {
    mLifetimeRemaining = def.mLifetimeSec;
    mSystemDefID = def.getID();
    mEmitters.reserve(def.mEmitters.size());
    for (auto&& emitterDef : def.mEmitters) {
        mEmitters.emplace_back(std::make_unique<CpuParticleEmitter>(emitterDef, &mInputs));
    }
}

bool CPUParticleSystem::updateAndRender(f32 elapsedSec, const f32m4& VP) {

    const MaterialShaderDef* boundShader = nullptr;
    for (auto&& iter = mEmitters.begin(); iter != mEmitters.end();) {
        CpuParticleEmitter& emitter = **iter;
        const MaterialShaderDef* nextShader = &emitter.getMaterialShader();
        if (!nextShader) {
            ++iter;
            continue;
        }
        if (nextShader != boundShader) {
            MaterialRenderer::bindMaterialShaderForRender(*nextShader);
            glUniformMatrix4fv(nextShader->getUniform("unVP"), 1, false, &VP[0][0]);
            glUniformMatrix4fv(nextShader->getUniform("unRootPos"), 1, false, &VP[0][0]);
            boundShader = nextShader;
        }
        if (emitter.updateAndRender(elapsedSec)) {
            iter = mEmitters.erase(iter);
        }
        else {
            ++iter;
        }
    }

    mLifetimeRemaining -= elapsedSec;
    if (mLifetimeRemaining <= 0.0f) {
        return true;
    }
}

bool CPUParticleSystem::updateAndRenderEditor(f32 elapsedSec, const f32m4& VP, const std::vector<bool>& emitterVisibility) {
    PROFILE_FUNCTION();
    int i = 0;
    size_t numAliveEmitters = mEmitters.size(); // We will destroy when all our emitters are done
    const MaterialShaderDef* boundShader = nullptr;
    if (emitterVisibility.size() != mEmitters.size()) return false; // Happens first time
    for (auto&& iter = mEmitters.begin(); iter != mEmitters.end(); ++iter, ++i) {
        if (*iter) {
            if (emitterVisibility[i]) {
                CpuParticleEmitter& emitter = **iter;
                const MaterialShaderDef* nextShader = &emitter.getMaterialShader();
                if (!nextShader) {
                    continue;
                }
                if (nextShader != boundShader) {
                    MaterialRenderer::bindMaterialShaderForRender(*nextShader);
                    glUniformMatrix4fv(nextShader->getUniform("unVP"), 1, false, &VP[0][0]);
                    boundShader = nextShader;
                }
                if (emitter.updateAndRender(elapsedSec)) {
                    // Editor doesn't remove from the vector
                    iter->reset();
                    --numAliveEmitters;
                }
            }
        }
        else {
            --numAliveEmitters;
        }
    }

    mLifetimeRemaining -= elapsedSec;

    return (mLifetimeRemaining <= 0.0f && numAliveEmitters == 0u);
}

CpuParticleEmitter& CPUParticleSystem::addEmitter(const ParticleUpdateFunction& updateFunction, ui32 maxParticles, BitFlags<ParticleComponentType> components, const MaterialShaderDef& shader, f32 particleLifespanSec /*= FLT_MAX*/, f32 emitterLifespanSec /*= FLT_MAX*/) {
    mEmitters.emplace_back(std::make_unique<CpuParticleEmitter>(updateFunction, maxParticles, components, shader, &mInputs, emitterLifespanSec))->setGlobalParticleLifespan(particleLifespanSec);
    return *mEmitters.back();
}

int CPUParticleSystem::getNumParticles() const {
    int total = 0;
    for (auto&& emitter : mEmitters) {
        if (emitter) {
            total += emitter->getNumActiveParticles();
        }
    }
    return total;
}

int CPUParticleSystem::getFragmentation() const {
    int total = 0;
    for (auto&& emitter : mEmitters) {
        if (emitter) {
            total += emitter->getFragmentation();
        }
    }
    return total;
}
