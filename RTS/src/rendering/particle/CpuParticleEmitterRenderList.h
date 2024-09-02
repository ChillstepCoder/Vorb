#pragma once

class CpuParticleEmitter;

struct EmitterRenderData {
    CpuParticleEmitter* emitter;
    const f32v3* rootPosition;
};

// Sort by blend mode, then by shader
using CpuParticleEmitterRenderList = std::array<FlatMap<AssetID /*materialShader*/, std::vector<EmitterRenderData>>, e_count(ParticleBlendMode)>;