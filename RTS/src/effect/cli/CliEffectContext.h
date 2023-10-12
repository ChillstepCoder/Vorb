#pragma once

#include "effect/IEffectContext.h"

class CliEffectContext : public IEffectContext {
public:
    void playParticleEffectAtPoint(
        StrToken effectName,
        f32v3 point,
        ParticleSystemInputs inputs,
        BitFlags<EffectCreateFlags> flags
    ) override;
};

