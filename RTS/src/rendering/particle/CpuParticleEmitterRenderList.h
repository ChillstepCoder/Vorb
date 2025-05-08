#pragma once

#include "rendering/particle/ParticleEnumTypes.h"

class CpuParticleEmitter;
class CPUParticleSystem;

struct EmitterRenderData {
    CpuParticleEmitter* emitter;
    CPUParticleSystem* system;
};

// Sort by blend mode, then by shader
using CpuParticleEmitterRenderList = std::array<FlatMap<AssetID /*materialShader*/, std::vector<EmitterRenderData>>, e_count(ParticleBlendMode)>;