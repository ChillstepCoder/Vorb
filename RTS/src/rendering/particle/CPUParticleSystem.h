#pragma once

#include "rendering/particle/CpuParticleEmitter.h"

class MaterialShader;
class ParticleSystemDef;

typedef std::unique_ptr<CpuParticleEmitter> CpuParticleEmitterPtr;

// Versatile quad rendering system used for both particles and simple UI
class CPUParticleSystem
{
public:
    CPUParticleSystem(size_t reserveEmitterCount);
    CPUParticleSystem(const ParticleUpdateFunction& updateFunction, ui32 maxParticles, BitFlags<ParticleComponentType> components, const MaterialShader& shader, f32 particleLifespanSec = FLT_MAX, f32 emitterLifespanSec = FLT_MAX);
    CPUParticleSystem(const ParticleSystemDef& def);

    VORB_NON_COPYABLE(CPUParticleSystem);

    // Bind shader before calling this
    void updateAndRender(f32 elapsedSec, const f32m4& VP);
    void updateAndRenderEditor(f32 elapsedSec, const f32m4& VP, const std::vector<bool>& emitterVisibility);

    CpuParticleEmitter& addEmitter(const ParticleUpdateFunction& updateFunction, ui32 maxParticles, BitFlags<ParticleComponentType> components, const MaterialShader& shader, f32 particleLifespanSec = FLT_MAX, f32 emitterLifespanSec = FLT_MAX);

    size_t getNumEmitters() const { return mEmitters.size(); }
    CpuParticleEmitter& getEmitter(int index) { return *mEmitters.at(index); }
    const std::vector<CpuParticleEmitterPtr>& getEmitters() const { return mEmitters; }
    int getNumParticles() const;
    // Returns number of iterations over dead particles each frame
    int getFragmentation() const;

private:
    std::vector<CpuParticleEmitterPtr> mEmitters;
    AssetID mSystemID = INVALID_ASSET_ID;
};