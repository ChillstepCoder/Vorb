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
        f32q orientation,
        ParticleSystemInputsPtr inputs,
        BitFlags<EffectCreateFlags> flags
    ) override;

    void playMutationEffect(
        const f32m4& transform,
        ModelID startModel,
        ModelID endModel,
        MutationType mutationType,
        BitFlags<EffectCreateFlags> flags
    ) override;
protected:
    CliEffectContext mCliContext;
};