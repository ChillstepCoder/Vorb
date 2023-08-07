#pragma once
#include "CPUParticleEmitterModule.h"

#include "util/ArbitraryObjectArray.h"

// Common module impl
#define MODULE_DEF(x) struct ModuleData { x } mModuleData; \
void addModuleDataToArray(ArbitraryObjectArray& arry) const { \
    arry.addObject(mModuleData); \
}

enum class BuiltinCPUParticleEditorModules {
    SpawnBurst,
    SpawnRate,
    RingBurst,
    ConeBurst,
    SetPosition,
    SetVelocity,
    SetColor,
    SetScale,
    SetPositionFromShape,
    ApplyForce,
    DragForce,
    COUNT
};

#define COMMON_METHODS(ModuleType) \
public: \
    ModuleType(); \
    bool updateAndRenderEditorControls() override; \
    std::unique_ptr<CPUParticleEmitterModule> clone() const override { return std::make_unique<ModuleType>(*this); };

#define BUILTIN_CPU_PARTICLE_MODULE(ModuleType, Stage, ModuleName) \
class ModuleType : public CPUParticleEmitterModule { \
    COMMON_METHODS(ModuleType) \
    void refresh() override; \
    BitFlags<ParticleEmitterModuleStage> getStages() const override { \
        return BitFlags<ParticleEmitterModuleStage>(Stage); \
    } \
    const char* const getName() const override { return ModuleName; } \
private:

BUILTIN_CPU_PARTICLE_MODULE(CPUPEM_SpawnBurst, ParticleEmitterModuleStage::EmitterUpdate, "Spawn Burst")
    MODULE_DEF(
        CPUParticleEmitterVariable mDelay = CPUParticleEmitterVariable(f32(0.0f));
        CPUParticleEmitterVariable mSpawnCount = CPUParticleEmitterVariable(ui32(10));
        bool mFired = false;
    );
};

BUILTIN_CPU_PARTICLE_MODULE(CPUPEM_SpawnRate, ParticleEmitterModuleStage::EmitterUpdate, "Spawn Rate")
    MODULE_DEF(
        CPUParticleEmitterVariable mEmitRateSec = CPUParticleEmitterVariable(0.0f);
        CPUParticleEmitterVariable mNextEmitTime = CPUParticleEmitterVariable(0.0f); // Initial delay
        CPUParticleEmitterVariable mSpawnCount = CPUParticleEmitterVariable(ui32(3));
    );
};

BUILTIN_CPU_PARTICLE_MODULE(CPUPEM_RingBurst, ParticleEmitterModuleStage::ParticleInit, "Ring Burst")
    MODULE_DEF(
        CPUParticleEmitterVariable mSpeedRange = CPUParticleEmitterVariable(f32v2(1.0f));
        CPUParticleEmitterVariable mMaxAngleFromRingRad = CPUParticleEmitterVariable(f32(M_PI_4F));
        CPUParticleEmitterVariable mRingNormal = CPUParticleEmitterVariable(f32v3(0.0f, 0.0f, 1.0f));
    );
};

BUILTIN_CPU_PARTICLE_MODULE(CPUPEM_ConeBurst, ParticleEmitterModuleStage::ParticleInit, "Cone Burst")
    MODULE_DEF(
        CPUParticleEmitterVariable mSpeedRange = CPUParticleEmitterVariable(f32v2(1.0f));
        CPUParticleEmitterVariable mAngleRange = CPUParticleEmitterVariable(f32v2(0.0f, M_PI_4F));
        CPUParticleEmitterVariable mDirection = CPUParticleEmitterVariable(f32v3(0.0f, 0.0f, 1.0f));
    );
};

BUILTIN_CPU_PARTICLE_MODULE(CPUPEM_SetPosition, e_cast(ParticleEmitterModuleStage::ParticleInit) | e_cast(ParticleEmitterModuleStage::ParticleUpdate), "Set Position")
    MODULE_DEF(
        CPUParticleEmitterVariable mPositionVec3 = CPUParticleEmitterVariable(f32v3(0.0f));
    );
};

BUILTIN_CPU_PARTICLE_MODULE(CPUPEM_SetVelocity, e_cast(ParticleEmitterModuleStage::ParticleInit) | e_cast(ParticleEmitterModuleStage::ParticleUpdate), "Set Velocity")
    MODULE_DEF(
        CPUParticleEmitterVariable mVelocityVec3 = CPUParticleEmitterVariable(f32v3(0.0f));
    );
};

BUILTIN_CPU_PARTICLE_MODULE(CPUPEM_SetColor, e_cast(ParticleEmitterModuleStage::ParticleInit) | e_cast(ParticleEmitterModuleStage::ParticleUpdate), "Set Color")
    MODULE_DEF(
        CPUParticleEmitterVariable mColor = CPUParticleEmitterVariable(color4(255, 255, 255, 255));
    );
};

BUILTIN_CPU_PARTICLE_MODULE(CPUPEM_SetScale, e_cast(ParticleEmitterModuleStage::ParticleInit) | e_cast(ParticleEmitterModuleStage::ParticleUpdate), "Set Scale")
    MODULE_DEF(
        CPUParticleEmitterVariable mScale = CPUParticleEmitterVariable(f32v2(1.0f, 1.0f));
    );
};

BUILTIN_CPU_PARTICLE_MODULE(CPUPEM_SetPositionFromShape, ParticleEmitterModuleStage::ParticleInit, "Set Position From Shape")
    enum class ShapeType : int {
       Sphere,
       Box,
       COUNT
    };
    MODULE_DEF(
        CPUParticleEmitterVariable mRadius = CPUParticleEmitterVariable(10.0f);
        ShapeType mShapeType = ShapeType::Sphere;
    );
};

BUILTIN_CPU_PARTICLE_MODULE(CPUPEM_ApplyForce, ParticleEmitterModuleStage::ParticleUpdate, "Apply Force")
    MODULE_DEF(
        CPUParticleEmitterVariable mForce = CPUParticleEmitterVariable(f32v3(0.0f, 0.0f, GRAVITY_Z));
    );
};

BUILTIN_CPU_PARTICLE_MODULE(CPUPEM_DragForce, ParticleEmitterModuleStage::ParticleUpdate, "Drag Force")
    MODULE_DEF(
        CPUParticleEmitterVariable mDragFactor = CPUParticleEmitterVariable(f32(0.75f));
    );
};

inline std::unique_ptr<CPUParticleEmitterModule> createCPUParticleEmitterModule(BuiltinCPUParticleEditorModules type) {
    switch (type) {
        case BuiltinCPUParticleEditorModules::SpawnBurst:
            return std::make_unique<CPUPEM_SpawnBurst>();
        case BuiltinCPUParticleEditorModules::SpawnRate:
            return std::make_unique<CPUPEM_SpawnRate>();
        case BuiltinCPUParticleEditorModules::RingBurst:
            return std::make_unique<CPUPEM_RingBurst>();
        case BuiltinCPUParticleEditorModules::ConeBurst:
            return std::make_unique<CPUPEM_ConeBurst>();
        case BuiltinCPUParticleEditorModules::SetPosition:
            return std::make_unique<CPUPEM_SetPosition>();
        case BuiltinCPUParticleEditorModules::SetVelocity:
            return std::make_unique<CPUPEM_SetVelocity>();
        case BuiltinCPUParticleEditorModules::SetColor:
            return std::make_unique<CPUPEM_SetColor>();
        case BuiltinCPUParticleEditorModules::SetScale:
            return std::make_unique<CPUPEM_SetScale>();
        case BuiltinCPUParticleEditorModules::SetPositionFromShape:
            return std::make_unique<CPUPEM_SetPositionFromShape>();
        case BuiltinCPUParticleEditorModules::ApplyForce:
            return std::make_unique<CPUPEM_ApplyForce>();
        case BuiltinCPUParticleEditorModules::DragForce:
            return std::make_unique<CPUPEM_DragForce>();
    }
    static_assert(e_count(BuiltinCPUParticleEditorModules) == 11);

    return nullptr;
}
