#pragma once

#include "effect/cli/CliEffectContext.h"

class World;

class HostEffectContext : public IEffectContext {
public:
    HostEffectContext(World& world) : IEffectContext(world), mCliContext(world) {}

    void renderEffects(f32 elapsedSec, const Camera3D& camera) override { mCliContext.renderEffects(elapsedSec, camera); }

    void playParticleEffectAtPoint(
        EffectAssetRef effectName,
        f32v3 point,
        ParticleSystemInputs inputs,
        BitFlags<EffectCreateFlags> flags
    ) override;
protected:
    CliEffectContext mCliContext;
};