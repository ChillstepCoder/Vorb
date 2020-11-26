#pragma once

#include <Vorb/graphics/BlendState.h>

enum class ParticleSystemType : ui8 {
    SIMPLE,
    BLOOD,
    FIRE
};
KEG_ENUM_DECL(ParticleSystemType);

struct ParticleSystemData {
    ParticleSystemType type = ParticleSystemType::SIMPLE;
    ui32 maxParticles = 160;
    f32v2 particleSizeRange = f32v2(1.0f);
    f32v2 particleLifetimeSecondsRange = f32v2(1.0f);
    f32 launchAngleHorizDeg = 360.0f;
    f32v2 launchAngleVertRangeDeg = f32v2(-10.0f, 30.0f);
    f32v2 speedRange = f32v2(0.09f, 0.1f);
    f32 airDamping = 1.0f;
    f32 hitDamping = 0.95f;
    f32 gravityMult = 1.0f;
    f32 duration = 1.0f;
    ui32 layer = 0;
    vg::BlendStateType blendState;
    vg::BlendStateType postBlendState;
    bool isLooping = false;
    bool isEmissive = false;
    nString materialName;
    nString postMaterialName;
};
KEG_TYPE_DECL(ParticleSystemData);