#pragma once

#include "rendering/particle/CPUParticleEmitterModule.h"

class MaterialShader;

typedef std::vector<std::unique_ptr<CPUParticleEmitterModule>> CPUParticleEmitterModuleVector;

struct ParticleEmitterDef {

    bool isValid() { return mShader != nullptr; }

    CPUParticleEmitterModuleVector mEmitterUpdateModules;
    CPUParticleEmitterModuleVector mParticleInitModules;
    CPUParticleEmitterModuleVector mParticleUpdateModules;
    MaterialShader* mShader = nullptr;

    nString mEmitterName;
    f32v2 mDefaultScaleRange = f32v2(1.0f);
    color4 mDefaultColor = color::White;
    ui32 mMaxParticles = 2000;
    MaterialID mDefaultMaterialID = 0;
    f32 mLifetimeSec = 5.0f;
    bool mLooping = true;
};

struct ParticleSystemDef {
    nString mSystemName;
    ParticleSystemID mID;
    std::vector<ParticleEmitterDef> mEmitters;
};