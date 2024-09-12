#pragma once
#include "CPUParticleEmitterModule.h"
#include "rendering/particle/CPUParticleEmitterOperation.h"

#include "util/ArbitraryObjectArray.h"

// Common module impl
#define MODULE_DEF(x) struct ModuleData { x } mModuleData; \
void addModuleDataToArray(ArbitraryObjectArray& arry) const { \
    arry.addObject(mModuleData); \
}

#define COMMON_METHODS(ModuleType) \
public: \
    ModuleType(); \
    bool updateAndRenderEditorControls(const ParticleEmitterDef& parentEmitter) override; \
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
        CPUParticleEmitterParameter mDelay = CPUParticleEmitterParameter(f32(0.0f));
        CPUParticleEmitterParameter mSpawnCount = CPUParticleEmitterParameter(ui32(10));
        bool mFired = false;
    );
)

BUILTIN_CPU_PARTICLE_MODULE(CPUPEM_SpawnRate, ParticleEmitterModuleStage::EmitterUpdate, "Spawn Rate", "spawn_rate",
    MODULE_DEF(
        CPUParticleEmitterParameter mEmitRateSec = CPUParticleEmitterParameter(0.0f);
        CPUParticleEmitterParameter mInitialDelay = CPUParticleEmitterParameter(0.0f);
        CPUParticleEmitterParameter mSpawnCount = CPUParticleEmitterParameter(ui32(3));
        f32 mNextEmitTime = -1.0f;
    );
)

BUILTIN_CPU_PARTICLE_MODULE(CPUPEM_RingBurst, ParticleEmitterModuleStage::ParticleInit, "Ring Burst", "ring_burst",
    MODULE_DEF(
        CPUParticleEmitterParameter mSpeedRange = CPUParticleEmitterParameter(f32v2(1.0f));
        CPUParticleEmitterParameter mMaxAngleFromRingRad = CPUParticleEmitterParameter(f32(M_PI_4F));
        CPUParticleEmitterParameter mRingNormal = CPUParticleEmitterParameter(f32v3(0.0f, 0.0f, 1.0f));
    );
)

BUILTIN_CPU_PARTICLE_MODULE(CPUPEM_ConeBurst, ParticleEmitterModuleStage::ParticleInit, "Cone Burst", "cone_burst",
    MODULE_DEF(
        CPUParticleEmitterParameter mSpeedRange = CPUParticleEmitterParameter(f32v2(1.0f));
        CPUParticleEmitterParameter mAngleRange = CPUParticleEmitterParameter(f32v2(0.0f, M_PI_4F));
        CPUParticleEmitterParameter mDirection = CPUParticleEmitterParameter(f32v3(0.0f, 0.0f, 1.0f));
    );
)

BUILTIN_CPU_PARTICLE_MODULE(CPUPEM_SetPosition, e_cast(ParticleEmitterModuleStage::ParticleInit) | e_cast(ParticleEmitterModuleStage::ParticleUpdate), "Set Position", "set_position",
    MODULE_DEF(
        CPUParticleEmitterParameter mPositionVec3 = CPUParticleEmitterParameter(f32v3(0.0f));
    );
)

BUILTIN_CPU_PARTICLE_MODULE(CPUPEM_SetVelocity, e_cast(ParticleEmitterModuleStage::ParticleInit) | e_cast(ParticleEmitterModuleStage::ParticleUpdate), "Set Velocity", "set_velocity",
    MODULE_DEF(
        CPUParticleEmitterParameter mVelocityVec3 = CPUParticleEmitterParameter(f32v3(0.0f));
    );
)

BUILTIN_CPU_PARTICLE_MODULE(CPUPEM_SetRotation, e_cast(ParticleEmitterModuleStage::ParticleInit) | e_cast(ParticleEmitterModuleStage::ParticleUpdate), "Set Rotation", "set_rotation",
    MODULE_DEF(
        CPUParticleEmitterParameter mRotationVec2 = CPUParticleEmitterParameter(f32v2(0.0f));
);
)

BUILTIN_CPU_PARTICLE_MODULE(CPUPEM_SetColor, e_cast(ParticleEmitterModuleStage::ParticleInit) | e_cast(ParticleEmitterModuleStage::ParticleUpdate), "Set Color", "set_color",
    MODULE_DEF(
        CPUParticleEmitterParameter mColor = CPUParticleEmitterParameter(color4(255, 255, 255, 255));
    );
)

BUILTIN_CPU_PARTICLE_MODULE(CPUPEM_SetHdrColor, e_cast(ParticleEmitterModuleStage::ParticleInit) | e_cast(ParticleEmitterModuleStage::ParticleUpdate), "Set HDR Color", "set_hdr_color",
    MODULE_DEF(
        CPUParticleEmitterParameter mColor = CPUParticleEmitterParameter(f32v4(1.0f));
    );
)

BUILTIN_CPU_PARTICLE_MODULE(CPUPEM_SetScale, e_cast(ParticleEmitterModuleStage::ParticleInit) | e_cast(ParticleEmitterModuleStage::ParticleUpdate), "Set Scale", "set_scale",
    MODULE_DEF(
        CPUParticleEmitterParameter mScale = CPUParticleEmitterParameter(f32v2(1.0f));
    );
)

BUILTIN_CPU_PARTICLE_MODULE(CPUPEM_SetUIntVar, e_cast(ParticleEmitterModuleStage::ParticleInit) | e_cast(ParticleEmitterModuleStage::ParticleUpdate), "Set Particle UInt Var", "set_uint",
    MODULE_DEF(
        CPUParticleEmitterParameter mVariable = CPUParticleEmitterParameter(ParticleEmitterVariableNameUInt::INVALID);
        CPUParticleEmitterParameter mValue = CPUParticleEmitterParameter(ui32(0));
    );
public:
    bool compatableWithEmitter(const CpuParticleEmitter& emitter) const override;
    void addRequiredVariables(RequiredEmitterVariables& variables) const override;
)

BUILTIN_CPU_PARTICLE_MODULE(CPUPEM_SetFloatVar, e_cast(ParticleEmitterModuleStage::ParticleInit) | e_cast(ParticleEmitterModuleStage::ParticleUpdate), "Set Particle Float Var", "set_float",
    MODULE_DEF(
        CPUParticleEmitterParameter mVariable = CPUParticleEmitterParameter(ParticleEmitterVariableNameFloat::INVALID);
        CPUParticleEmitterParameter mValue = CPUParticleEmitterParameter(f32(0.0f));
    );
public:
    bool compatableWithEmitter(const CpuParticleEmitter& emitter) const override;
    void addRequiredVariables(RequiredEmitterVariables& variables) const override;
)

BUILTIN_CPU_PARTICLE_MODULE(CPUPEM_SetVec2Var, e_cast(ParticleEmitterModuleStage::ParticleInit) | e_cast(ParticleEmitterModuleStage::ParticleUpdate), "Set Particle Vec2 Var", "set_vec2",
    MODULE_DEF(
        CPUParticleEmitterParameter mVariable = CPUParticleEmitterParameter(ParticleEmitterVariableNameVec2::INVALID);
        CPUParticleEmitterParameter mValue = CPUParticleEmitterParameter(f32v2(0.0f));
    );
public:
    bool compatableWithEmitter(const CpuParticleEmitter& emitter) const override;
    void addRequiredVariables(RequiredEmitterVariables& variables) const override;
)

BUILTIN_CPU_PARTICLE_MODULE(CPUPEM_SetVec3Var, e_cast(ParticleEmitterModuleStage::ParticleInit) | e_cast(ParticleEmitterModuleStage::ParticleUpdate), "Set Particle Vec3 Var", "set_vec3",
    MODULE_DEF(
        CPUParticleEmitterParameter mVariable = CPUParticleEmitterParameter(ParticleEmitterVariableNameVec3::INVALID);
        CPUParticleEmitterParameter mValue = CPUParticleEmitterParameter(f32v3(0.0f));
    );
public:
    bool compatableWithEmitter(const CpuParticleEmitter& emitter) const override;
    void addRequiredVariables(RequiredEmitterVariables& floatVariables) const override;
)

BUILTIN_CPU_PARTICLE_MODULE(CPUPEM_SetLifespan, e_cast(ParticleEmitterModuleStage::ParticleInit), "Set Lifespan", "set_life",
    MODULE_DEF(
        CPUParticleEmitterParameter mLifespan = CPUParticleEmitterParameter(1.0f);
    );
)

BUILTIN_CPU_PARTICLE_MODULE(CPUPEM_MultiplyScale, e_cast(ParticleEmitterModuleStage::ParticleInit) | e_cast(ParticleEmitterModuleStage::ParticleUpdate), "Multiply Scale", "mult_scale",
    MODULE_DEF(
        CPUParticleEmitterParameter mScale = CPUParticleEmitterParameter(f32v2(1.0f));
    );
)

BUILTIN_CPU_PARTICLE_MODULE(CPUPEM_MultiplyVelocity, e_cast(ParticleEmitterModuleStage::ParticleInit), "Scale Velocity", "mult_vel",
    MODULE_DEF(
        CPUParticleEmitterParameter mScale = CPUParticleEmitterParameter(f32v3(1.0f));
    );
)

BUILTIN_CPU_PARTICLE_MODULE(CPUPEM_ApplyForce, ParticleEmitterModuleStage::ParticleUpdate, "Apply Force", "apply_force",
    MODULE_DEF(
        CPUParticleEmitterParameter mForce = CPUParticleEmitterParameter(f32v3(0.0f, 0.0f, GRAVITY_Z));
    );
)

BUILTIN_CPU_PARTICLE_MODULE(CPUPEM_DragForce, ParticleEmitterModuleStage::ParticleUpdate, "Drag Force", "drag_force",
    MODULE_DEF(
        CPUParticleEmitterParameter mDragFactor = CPUParticleEmitterParameter(f32(0.75f));
    );
)

BUILTIN_CPU_PARTICLE_MODULE(CPUPEM_Turbulence, ParticleEmitterModuleStage::ParticleUpdate, "Turbulence", "turbulence",
    MODULE_DEF(
        CPUParticleEmitterParameter mScaleFactor = CPUParticleEmitterParameter(f32v3(1.0f));
        // Applied to pre scaled value
        CPUParticleEmitterParameter mDirOffset = CPUParticleEmitterParameter(f32v3(0.0f));
    );
)

BUILTIN_CPU_PARTICLE_MODULE(CPUPEM_OrientToVelocity, e_cast(ParticleEmitterModuleStage::ParticleInit) | e_cast(ParticleEmitterModuleStage::ParticleUpdate), "Orient To Velocity", "orient_vel",
    MODULE_DEF(
        //CPUParticleEmitterParameter mStretchFactor = CPUParticleEmitterParameter(f32v2(1.0f, 0.0f));
    );
)

BUILTIN_CPU_PARTICLE_MODULE(CPUPEM_MeshReproductionSource, e_cast(ParticleEmitterModuleStage::ParticleInit), "Mesh Reproduction Source", "mesh_rep_src",
    MODULE_DEF(
        CPUParticleEmitterParameter mScaleClamp = CPUParticleEmitterParameter(f32v2(0.1f, 5.0f));
        CPUParticleEmitterParameter mScaleMult = CPUParticleEmitterParameter(f32(1.0f));
    );
public:
    void addRequiredVariables(RequiredEmitterVariables& variables) const override;
)

BUILTIN_CPU_PARTICLE_MODULE(CPUPEM_MeshReproductionTarget, e_cast(ParticleEmitterModuleStage::ParticleInit), "Mesh Reproduction Target", "mesh_rep_tgt",
    MODULE_DEF(
        CPUParticleEmitterParameter mScaleClamp = CPUParticleEmitterParameter(f32v2(0.1f, 5.0f));
        CPUParticleEmitterParameter mScaleMult = CPUParticleEmitterParameter(f32(1.0f));
        CPUParticleEmitterParameter mFindClosestChecks = CPUParticleEmitterParameter(ui32(1));
    );
public:
    void addRequiredVariables(RequiredEmitterVariables& variables) const override;
)