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
    SetPosition,
    SetVelocity,
    SetColor,
    COUNT
};

// Spawn Burst
class CPUPEM_SpawnBurst : public CPUParticleEmitterModule {
public:
    CPUPEM_SpawnBurst();
    bool updateAndRenderEditorControls() override;
    BitFlags<ParticleEmitterModuleStage> getStages() const override {
        return BitFlags<ParticleEmitterModuleStage>(ParticleEmitterModuleStage::EmitterUpdate);
    }
    const char* const getName() const override { return "Spawn Burst"; }
    std::unique_ptr<CPUParticleEmitterModule> clone() const override { return std::make_unique<CPUPEM_SpawnBurst>(*this); };

private:
    MODULE_DEF(
        CPUParticleEmitterVariable mDelay = CPUParticleEmitterVariable(f32(0.0f));
        CPUParticleEmitterVariable mSpawnCount = CPUParticleEmitterVariable(ui32(10));
        bool mFired = false;
    );
};

// Spawn Rate
class CPUPEM_SpawnRate : public CPUParticleEmitterModule {
public:
    CPUPEM_SpawnRate();
    bool updateAndRenderEditorControls() override;
    BitFlags<ParticleEmitterModuleStage> getStages() const override {
        return BitFlags<ParticleEmitterModuleStage>(ParticleEmitterModuleStage::EmitterUpdate);
    }
    const char* const getName() const override { return "Spawn Rate"; }
    std::unique_ptr<CPUParticleEmitterModule> clone() const override { return std::make_unique<CPUPEM_SpawnRate>(*this); };

private:
    MODULE_DEF(
        CPUParticleEmitterVariable mEmitRateSec = CPUParticleEmitterVariable(0.0f);
        CPUParticleEmitterVariable mNextEmitTime = CPUParticleEmitterVariable(0.0f); // Initial delay
        CPUParticleEmitterVariable mSpawnCount = CPUParticleEmitterVariable(ui32(3));
    );
};

// Set Position
class CPUPEM_SetPosition : public CPUParticleEmitterModule {
public:
    CPUPEM_SetPosition();
    bool updateAndRenderEditorControls() override;
    BitFlags<ParticleEmitterModuleStage> getStages() const override {
        return BitFlags<ParticleEmitterModuleStage>(ParticleEmitterModuleStage::ParticleInit, ParticleEmitterModuleStage::ParticleUpdate);
    }
    const char* const getName() const override { return "Set Position"; }
    std::unique_ptr<CPUParticleEmitterModule> clone() const override { return std::make_unique<CPUPEM_SetPosition>(*this); };

private:
    MODULE_DEF(
        CPUParticleEmitterVariable mPositionVec3 = CPUParticleEmitterVariable(f32v3(0.0f));
    );
};

// Set Velocity
class CPUPEM_SetVelocity : public CPUParticleEmitterModule {
public:
    CPUPEM_SetVelocity();
    bool updateAndRenderEditorControls() override;
    BitFlags<ParticleEmitterModuleStage> getStages() const override {
        return BitFlags<ParticleEmitterModuleStage>(ParticleEmitterModuleStage::ParticleInit, ParticleEmitterModuleStage::ParticleUpdate);
    }
    const char* const getName() const override { return "Set Velocity"; }
    std::unique_ptr<CPUParticleEmitterModule> clone() const override { return std::make_unique<CPUPEM_SetVelocity>(*this); };
private:
    MODULE_DEF(
        CPUParticleEmitterVariable mVelocityVec3 = CPUParticleEmitterVariable(f32v3(0.0f));
    );
};

class CPUPEM_SetColor : public CPUParticleEmitterModule {
public:
    CPUPEM_SetColor();
    bool updateAndRenderEditorControls() override;
    BitFlags<ParticleEmitterModuleStage> getStages() const override {
        return BitFlags<ParticleEmitterModuleStage>(ParticleEmitterModuleStage::ParticleInit, ParticleEmitterModuleStage::ParticleUpdate);
    }
    const char* const getName() const override { return "Set Color"; }
    std::unique_ptr<CPUParticleEmitterModule> clone() const override { return std::make_unique<CPUPEM_SetColor>(*this); };
private:
    MODULE_DEF(
        CPUParticleEmitterVariable mColor = CPUParticleEmitterVariable(color4(255, 255, 255, 255));
    );
};

inline std::unique_ptr<CPUParticleEmitterModule> createCPUParticleEmitterModule(BuiltinCPUParticleEditorModules type) {
    switch (type) {
        case BuiltinCPUParticleEditorModules::SpawnBurst:
            return std::make_unique<CPUPEM_SpawnBurst>();
        case BuiltinCPUParticleEditorModules::SpawnRate:
            return std::make_unique<CPUPEM_SpawnRate>();
        case BuiltinCPUParticleEditorModules::SetPosition:
            return std::make_unique<CPUPEM_SetPosition>();
        case BuiltinCPUParticleEditorModules::SetVelocity:
            return std::make_unique<CPUPEM_SetVelocity>();
        case BuiltinCPUParticleEditorModules::SetColor:
            return std::make_unique<CPUPEM_SetColor>();
    }
    static_assert(e_count(BuiltinCPUParticleEditorModules) == 5);

    return nullptr;
}