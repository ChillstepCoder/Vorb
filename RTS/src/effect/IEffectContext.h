#pragma once

#include "world/WorldContextObject.h"
#include "rendering/particle/ParticleSystemInputs.h"

class Camera3D;

enum class EffectCreateFlags : ui8 {
    REPLICATE = BIT(0),
    TRACKED = BIT(1) // UNUSED
};

class IEffectContext : public WorldContextObject {
public:
    IEffectContext(World& world) : WorldContextObject(world) {}
    virtual ~IEffectContext() = default;

    virtual void renderEffects(f32 elapsedSec, const Camera3D& camera) = 0;

    virtual void playParticleEffectAtPoint(
        StrToken effectName,
        f32v3 point,
        ParticleSystemInputs inputs,
        BitFlags<EffectCreateFlags> flags
    ) = 0;

protected:
};

