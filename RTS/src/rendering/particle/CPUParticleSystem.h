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
    CPUParticleSystem(const ParticleUpdateFunction& updateFunction, ui32 maxParticles, BitFlags<ParticleComponentType> components, const MaterialShader& shader);
    CPUParticleSystem(const ParticleSystemDef& def);

    VORB_NON_COPYABLE(CPUParticleSystem);

    // Bind shader before calling this
    void updateAndRender(f32 elapsedSec, const f32m4& VP);

    CpuParticleEmitter& addEmitter(const ParticleUpdateFunction& updateFunction, ui32 maxParticles, BitFlags<ParticleComponentType> components, const MaterialShader& shader);

    size_t getNumEmitters() const { return mEmitters.size(); }
    CpuParticleEmitter& getEmitter(int index) { return *mEmitters.at(index); }
    const std::vector<CpuParticleEmitterPtr>& getEmitters() const { return mEmitters; }

private:
    std::vector<CpuParticleEmitterPtr> mEmitters;
    ParticleSystemID mSystemID = INVALID_PARTICLE_SYSTEM_ID;
};