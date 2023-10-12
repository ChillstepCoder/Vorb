#pragma once

#include "effect/cli/CliEffectContext.h"

class HostEffectContext : public IEffectContext {
public:
    void playParticleEffectAtPoint(
        StrToken effectName,
        f32v3 point,
        ParticleSystemInputs inputs,
        BitFlags<EffectCreateFlags> flags
    ) override;
protected:
    CliEffectContext mCliContext;
};