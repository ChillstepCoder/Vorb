#include "stdafx.h"
#include "ParticleSystemData.h"

KEG_ENUM_DEF(ParticleSystemType, ParticleSystemType, kt) {
    kt.addValue("simple", ParticleSystemType::SIMPLE);
    kt.addValue("blood", ParticleSystemType::BLOOD);
    kt.addValue("fire", ParticleSystemType::FIRE);
}

KEG_TYPE_DEF_SAME_NAME(ParticleSystemData, kt) {
    kt.addValue("type", keg::Value::custom(offsetof(ParticleSystemData, type), "ParticleSystemType", true));
    kt.addValue("max_particles", keg::Value::basic(offsetof(ParticleSystemData, maxParticles), keg::BasicType::UI32));
    kt.addValue("particle_size", keg::Value::basic(offsetof(ParticleSystemData, particleSizeRange), keg::BasicType::F32_V2));
    kt.addValue("particle_lifetime", keg::Value::basic(offsetof(ParticleSystemData, particleLifetimeSecondsRange), keg::BasicType::F32_V2));
    kt.addValue("launch_angle_horizontal", keg::Value::basic(offsetof(ParticleSystemData, launchAngleHorizDeg), keg::BasicType::F32));
    kt.addValue("launch_angle_vertical", keg::Value::basic(offsetof(ParticleSystemData, launchAngleVertRangeDeg), keg::BasicType::F32_V2));
    kt.addValue("speed", keg::Value::basic(offsetof(ParticleSystemData, speedRange), keg::BasicType::F32_V2));
    kt.addValue("air_damping", keg::Value::basic(offsetof(ParticleSystemData, airDamping), keg::BasicType::F32));
    kt.addValue("hit_damping", keg::Value::basic(offsetof(ParticleSystemData, hitDamping), keg::BasicType::F32));
    kt.addValue("gravity_mult", keg::Value::basic(offsetof(ParticleSystemData, gravityMult), keg::BasicType::F32));
    kt.addValue("material", keg::Value::basic(offsetof(ParticleSystemData, materialName), keg::BasicType::STRING));
    kt.addValue("post_material", keg::Value::basic(offsetof(ParticleSystemData, postMaterialName), keg::BasicType::STRING));
    kt.addValue("duration", keg::Value::basic(offsetof(ParticleSystemData, duration), keg::BasicType::F32));
    kt.addValue("looping", keg::Value::basic(offsetof(ParticleSystemData, isLooping), keg::BasicType::BOOL));
    kt.addValue("emissive", keg::Value::basic(offsetof(ParticleSystemData, isEmissive), keg::BasicType::BOOL));
    kt.addValue("layer", keg::Value::basic(offsetof(ParticleSystemData, layer), keg::BasicType::UI32));
    kt.addValue("blend", keg::Value::custom(offsetof(ParticleSystemData, blendState), "BlendStateType", true));
    kt.addValue("post_blend", keg::Value::custom(offsetof(ParticleSystemData, postBlendState), "BlendStateType", true));
}
