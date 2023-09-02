#pragma once
#include "CPUParticleEmitterModule.h"

#include "util/ArbitraryObjectArray.h"

#include "serialization/YmlSerializer.h"

// Common module impl
#define MODULE_DEF(x) struct ModuleData { x } mModuleData; \
void addModuleDataToArray(ArbitraryObjectArray& arry) const { \
    arry.addObject(mModuleData); \
}

#define COMMON_METHODS(ModuleType) \
public: \
    ModuleType(); \
    bool updateAndRenderEditorControls() override; \
    std::unique_ptr<CPUParticleEmitterModule> clone() const override { return std::make_unique<ModuleType>(*this); };

#define BUILTIN_CPU_PARTICLE_MODULE(ModuleType, Stage, ModuleName, YmlName, ...) \
class ModuleType : public CPUParticleEmitterModule { \
    COMMON_METHODS(ModuleType) \
    void refresh() override; \
    BitFlags<ParticleEmitterModuleStage> getStages() const override { \
        return BitFlags<ParticleEmitterModuleStage>(Stage); \
    } \
    constexpr const char* const getName() const override { return ModuleName; } \
    constexpr const char* const getYmlName() const override { return YmlName; } \
    bool loadFromYml(ryml::ConstNodeRef node) override; \
    void saveYmlData(ryml::NodeRef node) const override; \
private: \
   __VA_ARGS__ \
}; \
REGISTER_YML_OBJECT(YmlName, ModuleType, CPUParticleEmitterModule);

BUILTIN_CPU_PARTICLE_MODULE(CPUPEM_SpawnBurst, ParticleEmitterModuleStage::EmitterUpdate, "Spawn Burst", "spawn_burst",
    MODULE_DEF(
        CPUParticleEmitterVariable mDelay = CPUParticleEmitterVariable(f32(0.0f));
        CPUParticleEmitterVariable mSpawnCount = CPUParticleEmitterVariable(ui32(10));
        bool mFired = false;
    );
)

BUILTIN_CPU_PARTICLE_MODULE(CPUPEM_SpawnRate, ParticleEmitterModuleStage::EmitterUpdate, "Spawn Rate", "spawn_rate",
    MODULE_DEF(
        CPUParticleEmitterVariable mEmitRateSec = CPUParticleEmitterVariable(0.0f);
        CPUParticleEmitterVariable mInitialDelay = CPUParticleEmitterVariable(0.0f);
        CPUParticleEmitterVariable mSpawnCount = CPUParticleEmitterVariable(ui32(3));
        f32 mNextEmitTime = -1.0f;
    );
)

BUILTIN_CPU_PARTICLE_MODULE(CPUPEM_RingBurst, ParticleEmitterModuleStage::ParticleInit, "Ring Burst", "ring_burst",
    MODULE_DEF(
        CPUParticleEmitterVariable mSpeedRange = CPUParticleEmitterVariable(f32v2(1.0f));
        CPUParticleEmitterVariable mMaxAngleFromRingRad = CPUParticleEmitterVariable(f32(M_PI_4F));
        CPUParticleEmitterVariable mRingNormal = CPUParticleEmitterVariable(f32v3(0.0f, 0.0f, 1.0f));
    );
)

BUILTIN_CPU_PARTICLE_MODULE(CPUPEM_ConeBurst, ParticleEmitterModuleStage::ParticleInit, "Cone Burst", "cone_burst",
    MODULE_DEF(
        CPUParticleEmitterVariable mSpeedRange = CPUParticleEmitterVariable(f32v2(1.0f));
        CPUParticleEmitterVariable mAngleRange = CPUParticleEmitterVariable(f32v2(0.0f, M_PI_4F));
        CPUParticleEmitterVariable mDirection = CPUParticleEmitterVariable(f32v3(0.0f, 0.0f, 1.0f));
    );
)

BUILTIN_CPU_PARTICLE_MODULE(CPUPEM_SetPosition, e_cast(ParticleEmitterModuleStage::ParticleInit) | e_cast(ParticleEmitterModuleStage::ParticleUpdate), "Set Position", "set_position",
    MODULE_DEF(
        CPUParticleEmitterVariable mPositionVec3 = CPUParticleEmitterVariable(f32v3(0.0f));
    );
)

BUILTIN_CPU_PARTICLE_MODULE(CPUPEM_SetVelocity, e_cast(ParticleEmitterModuleStage::ParticleInit) | e_cast(ParticleEmitterModuleStage::ParticleUpdate), "Set Velocity", "set_velocity",
    MODULE_DEF(
        CPUParticleEmitterVariable mVelocityVec3 = CPUParticleEmitterVariable(f32v3(0.0f));
    );
)

BUILTIN_CPU_PARTICLE_MODULE(CPUPEM_SetColor, e_cast(ParticleEmitterModuleStage::ParticleInit) | e_cast(ParticleEmitterModuleStage::ParticleUpdate), "Set Color", "set_color",
    MODULE_DEF(
        CPUParticleEmitterVariable mColor = CPUParticleEmitterVariable(color4(255, 255, 255, 255));
    );
)

BUILTIN_CPU_PARTICLE_MODULE(CPUPEM_SetHdrColor, e_cast(ParticleEmitterModuleStage::ParticleInit) | e_cast(ParticleEmitterModuleStage::ParticleUpdate), "Set HDR Color", "set_hdr_color",
    MODULE_DEF(
        CPUParticleEmitterVariable mColor = CPUParticleEmitterVariable(f32v4(1.0f));
    );
)

BUILTIN_CPU_PARTICLE_MODULE(CPUPEM_SetScale, e_cast(ParticleEmitterModuleStage::ParticleInit) | e_cast(ParticleEmitterModuleStage::ParticleUpdate), "Set Scale", "set_scale",
    MODULE_DEF(
        CPUParticleEmitterVariable mScale = CPUParticleEmitterVariable(f32v2(1.0f, 1.0f));
    );
)

BUILTIN_CPU_PARTICLE_MODULE(CPUPEM_ApplyForce, ParticleEmitterModuleStage::ParticleUpdate, "Apply Force", "apply_force",
    MODULE_DEF(
        CPUParticleEmitterVariable mForce = CPUParticleEmitterVariable(f32v3(0.0f, 0.0f, GRAVITY_Z));
    );
)

BUILTIN_CPU_PARTICLE_MODULE(CPUPEM_DragForce, ParticleEmitterModuleStage::ParticleUpdate, "Drag Force", "drag_force",
    MODULE_DEF(
        CPUParticleEmitterVariable mDragFactor = CPUParticleEmitterVariable(f32(0.75f));
    );
)

BUILTIN_CPU_PARTICLE_MODULE(CPUPEM_Turbulence, ParticleEmitterModuleStage::ParticleUpdate, "Turbulence", "turbulence",
    MODULE_DEF(
        CPUParticleEmitterVariable mScaleFactor = CPUParticleEmitterVariable(f32v3(1.0f));
        // Applied to pre scaled value
        CPUParticleEmitterVariable mDirOffset = CPUParticleEmitterVariable(f32v3(0.0f));
    );
)