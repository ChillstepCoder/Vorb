#pragma once

enum class ParticleSystemType : ui8 {
    SIMPLE,
    FIRE
};
KEG_ENUM_DECL(ParticleSystemType);

struct ParticleSystemData {
    ParticleSystemType type = ParticleSystemType::SIMPLE;
    ui32 maxParticles = 160;
    f32v2 particleSizeRange = f32v2(1.0f);
    f32v2 particleLifetimeSecondsRange = f32v2(1.0f);
    f32 launchAngleHorizDeg = 360.0f;
    f32 launchAngleVertDeg = 360.0f;
    f32 airDamping = 1.0f;
    f32 hitDamping = 0.95f;
    f32 gravityMult = 1.0f;
    f32 duration = 1.0f;
    bool isLooping = false;
    bool isEmissive = false;
    nString materialName;
};
KEG_TYPE_DECL(ParticleSystemData);