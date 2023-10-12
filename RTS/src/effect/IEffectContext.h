#pragma once

#include "rendering/particle/ParticleSystemInputs.h"

enum class EffectCreateFlags : ui8 {
    REPLICATE = BIT(0),
    TRACKED = BIT(1) // UNUSED
};

class IEffectContext {
public:
    IEffectContext() = default;
    virtual ~IEffectContext() = default;

    virtual void playParticleEffectAtPoint(
        StrToken effectName,
        f32v3 point,
        ParticleSystemInputs inputs,
        BitFlags<EffectCreateFlags> flags
    ) = 0;
protected:
};

