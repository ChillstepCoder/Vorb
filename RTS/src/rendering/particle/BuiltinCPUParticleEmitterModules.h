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
    void refresh() override;

private:
    MODULE_DEF(
        f32v3 mPosition = f32v3(0.0f);
    );
};

// Set Velocity
class CPUPEM_SetVelocity : CPUParticleEmitterModule {
public:
    void refresh() override;

private:
    MODULE_DEF(
        f32v3 mVelocity = f32v3(0.0f);
    );
};

class CPUPEM_SetColor : CPUParticleEmitterModule {
public:
    void refresh() override;

private:
    MODULE_DEF(
        color4 mColor = color4(255, 255, 255, 255);
    );
};
