#include "stdafx.h"
#include "ParticleSystemData.h"

KEG_ENUM_DEF(ParticleSystemType, ParticleSystemType, kt) {
    kt.addValue("Simple", ParticleSystemType::SIMPLE);
    kt.addValue("Fire", ParticleSystemType::FIRE);
}

KEG_TYPE_DEF_SAME_NAME(ParticleSystemData, kt) {
    kt.addValue("type", keg::Value::custom(offsetof(ParticleSystemData, type), "ParticleSystemType", true));
    kt.addValue("max_particles", keg::Value::basic(offsetof(ParticleSystemData, maxParticles), keg::BasicType::UI32));
    kt.addValue("particle_Size", keg::Value::basic(offsetof(ParticleSystemData, particleSize), keg::BasicType::F32));
    kt.addValue("particle_lifetime", keg::Value::basic(offsetof(ParticleSystemData, particleLifetimeSeconds), keg::BasicType::F32));
    kt.addValue("launch_angle_horizontal", keg::Value::basic(offsetof(ParticleSystemData, launchAngleHorizDeg), keg::BasicType::F32));
    kt.addValue("launch_angle_vertical", keg::Value::basic(offsetof(ParticleSystemData, launchAngleVertDeg), keg::BasicType::F32));
    kt.addValue("air_damping", keg::Value::basic(offsetof(ParticleSystemData, airDamping), keg::BasicType::F32));
    kt.addValue("hit_damping", keg::Value::basic(offsetof(ParticleSystemData, hitDamping), keg::BasicType::F32));
    kt.addValue("gravity_mult", keg::Value::basic(offsetof(ParticleSystemData, gravityMult), keg::BasicType::F32));
    kt.addValue("material", keg::Value::basic(offsetof(ParticleSystemData, materialName), keg::BasicType::STRING));
    kt.addValue("is_emitter", keg::Value::basic(offsetof(ParticleSystemData, isEmitter), keg::BasicType::BOOL));
}
