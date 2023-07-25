#pragma once
#include "CPUParticleEmitterModule.h"

// Common module impl
#define MODULE_DEF(x) struct ModuleData { x } mModuleData; \
void addModuleDataToArray(ArbitraryObjectArray& arry) { \
    arry.addObject(mModuleData); \
}

// Set Position
class CPUPEM_SetPosition : CPUParticleEmitterModule {
public:
    CPUPEM_SetPosition();

private:
    MODULE_DEF(
        f32v3 mPosition = f32v3(0.0f);
    );
};

// Set Velocity
class CPUPEM_SetVelocity : CPUParticleEmitterModule {
public:
    CPUPEM_SetVelocity();

private:
    MODULE_DEF(
        f32v3 mVelocity = f32v3(0.0f);
    );
};

class CPUPEM_SetColor : CPUParticleEmitterModule {
public:
    CPUPEM_SetColor();

private:
    MODULE_DEF(
        color4 mColor = color4(255, 255, 255, 255);
    );
};
