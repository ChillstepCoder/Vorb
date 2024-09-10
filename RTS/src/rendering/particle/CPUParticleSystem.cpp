#include "stdafx.h"
#include "CPUParticleSystem.h"

#include "definitions/ParticleSystemDef.h"
#include "rendering/MaterialShaderRepository.h"

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
        mEmitters.emplace_back(std::make_unique<CpuParticleEmitter>(emitterDef, mInputs.get()));
    }
}

bool CPUParticleSystem::update(f32 elapsedSec, CpuParticleEmitterRenderList& outRenderList) {

    const MaterialShaderDef* boundShader = nullptr;
    for (auto&& iter = mEmitters.begin(); iter != mEmitters.end();) {
        if ((*iter)->update(elapsedSec)) {
            iter = mEmitters.erase(iter);
        }
        else {
            ++iter;
        }
    }

    // TODO: Just always use emitter lifetime?
    mLifetimeRemaining -= elapsedSec;
    if (mLifetimeRemaining <= 0.0f && mEmitters.size() == 0) {
        return true;
    }

    // Add active emitters to the render list
    for (auto&& emitter : mEmitters) {
        outRenderList[e_cast(emitter->getBlendMode())][emitter->getShaderID()].push_back(EmitterRenderData{ emitter.get(), &mRootPosition });
    }

    return false;
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
                const MaterialShaderDef* nextShader = MaterialShaderRepository::get().tryGetLoadedAsset(emitter.getShaderID());
                if (!nextShader) {
                    continue;
                }
                if (nextShader != boundShader) {
                    MaterialRenderer::bindMaterialShaderForRender(*nextShader);
                    glUniformMatrix4fv(nextShader->getUniform("unVP"), 1, false, &VP[0][0]);
                    glUniform3fv(nextShader->getUniform("unRootPos"), 1, &mRootPosition[0]);
                    boundShader = nextShader;
                }
                if (emitter.update(elapsedSec)) {
                    // Editor doesn't remove from the vector
                    iter->reset();
                    --numAliveEmitters;
                }
                else {
                    bindStateForParticleBlendMode(emitter.getBlendMode());
                    emitter.render();
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
    mEmitters.emplace_back(std::make_unique<CpuParticleEmitter>(updateFunction, maxParticles, components, shader, mInputs.get(), emitterLifespanSec))->setGlobalParticleLifespan(particleLifespanSec);
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

int CPUParticleSystem::getNumParticles(const std::vector<bool>& emitterVisibility) const
{
    assert(emitterVisibility.size() == mEmitters.size());
    int i = 0;
    int total = 0;
    for (auto&& emitter : mEmitters) {
        if (emitter && emitterVisibility[i++]) {
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
