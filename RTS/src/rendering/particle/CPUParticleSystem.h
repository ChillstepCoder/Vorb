#pragma once

#include "rendering/particle/CpuParticleEmitter.h"

class MaterialShaderDef;
class ParticleSystemDef;

typedef std::unique_ptr<CpuParticleEmitter> CpuParticleEmitterPtr;

// Versatile quad rendering system used for both particles and simple UI
class CPUParticleSystem {
public:
    CPUParticleSystem(size_t reserveEmitterCount, f32 lifespanSec);
    CPUParticleSystem(const ParticleUpdateFunction& updateFunction, ui32 maxParticles, BitFlags<ParticleComponentType> components, const MaterialShaderDef& shader, f32 particleLifespanSec = FLT_MAX, f32 emitterLifespanSec = FLT_MAX);
    CPUParticleSystem(const ParticleSystemDef& def, f32v3 position);

    // Inputs
    f32v3 getPosition() const { return mRootPosition; }
    void setPosition(f32v3 position) { mRootPosition = position; }
    void setInputs(ParticleSystemInputs inputs) {
        mInputs = inputs;
    }

    VORB_NON_COPYABLE(CPUParticleSystem);

    POOLED_ALLOC_DECL();

    // Bind shader before calling this
    // Return true if lifetime expired
    // TODO: Camera culling
    bool update(f32 elapsedSec, CpuParticleEmitterRenderList& outRenderList);

    // Return true if lifetime expired
    bool updateAndRenderEditor(f32 elapsedSec, const f32m4& VP, const std::vector<bool>& emitterVisibility);

    CpuParticleEmitter& addEmitter(const ParticleUpdateFunction& updateFunction, ui32 maxParticles, BitFlags<ParticleComponentType> components, const MaterialShaderDef& shader, f32 particleLifespanSec = FLT_MAX, f32 emitterLifespanSec = FLT_MAX);

    size_t getNumEmitters() const { return mEmitters.size(); }
    CpuParticleEmitter& getEmitter(int index) { return *mEmitters.at(index); }
    const std::vector<CpuParticleEmitterPtr>& getEmitters() const { return mEmitters; }
    int getNumParticles() const;
    int getNumParticles(const std::vector<bool>& emitterVisibility) const;
    // Returns number of iterations over dead particles each frame
    int getFragmentation() const;

private:
    std::vector<CpuParticleEmitterPtr> mEmitters;
    AssetID mSystemDefID = INVALID_ASSET_ID;
    ParticleSystemInputs mInputs;
    f32v3 mRootPosition = f32v3(0.0f);
    f32 mLifetimeRemaining = 0.0f;
};
