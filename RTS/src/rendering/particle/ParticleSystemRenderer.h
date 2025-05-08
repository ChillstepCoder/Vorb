#pragma once

#include "rendering/particle/CpuParticleEmitterRenderList.h"
#include "rendering/particle/ParticleEnumTypes.h"

class Camera3D;
class CpuParticleEmitter;

class ParticleSystemRenderer {
public:
    static void renderEmitters(CpuParticleEmitterRenderList& renderList, const Camera3D& camera);
    // Editor only, inefficient
    static void renderEmitterEditor(CpuParticleEmitter* emitter, const f32m4& VP, f32v3 rootPosition);
    // Helper utility that binds depth and blend states
    static void bindStateForParticleBlendMode(ParticleBlendMode blendMode);
};

