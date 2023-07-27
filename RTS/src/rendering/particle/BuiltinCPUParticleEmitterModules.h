#pragma once
#include "CPUParticleEmitterModule.h"

#include "util/ArbitraryObjectArray.h"

// Common module impl
#define MODULE_DEF(x) struct ModuleData { x } mModuleData; \
void addModuleDataToArray(ArbitraryObjectArray& arry) { \
    arry.addObject(mModuleData); \
}

// Set Position
class CPUPEM_SetPosition : CPUParticleEmitterModule {
public:
    void init() override;

private:
    MODULE_DEF(
        CPUParticleEmitterVariable mPositionVec3 = CPUParticleEmitterVariable(f32v3(0.0f));
    );
};

// Set Velocity
class CPUPEM_SetVelocity : CPUParticleEmitterModule {
public:
    void init() override;

private:
    MODULE_DEF(
        CPUParticleEmitterVariable mVelocityVec3 = CPUParticleEmitterVariable(f32v3(0.0f));
    );
};

class CPUPEM_SetColor : CPUParticleEmitterModule {
public:
    void init() override;

private:
    MODULE_DEF(
        CPUParticleEmitterVariable mColor = CPUParticleEmitterVariable(color4(255, 255, 255, 255));
    );
};
