#pragma once

#include "rendering/particle/CPUParticleEmitterModule.h"

#include "resources/IAsset.h"

#include "rendering/particle/ParticleEnumTypes.h"

class MaterialShader;

typedef std::vector<std::unique_ptr<CPUParticleEmitterModule>> CPUParticleEmitterModuleVector;

class ParticleEmitterDef {
public:

    bool isValid() { return mShader != nullptr; }

    CPUParticleEmitterModuleVector mEmitterUpdateModules;
    CPUParticleEmitterModuleVector mParticleInitModules;
    CPUParticleEmitterModuleVector mParticleUpdateModules;
    const MaterialShader* mShader = nullptr;

    nString mEmitterName;
    f32v2 mDefaultScale = f32v2(0.1f);
    color4 mDefaultColor = color::White;
    ui32 mMaxParticles = 2000;
    MaterialID mDefaultMaterialID = 0;
    f32 mLifetimeSec = 5.0f;
    bool mLooping = true;
    ParticleBlendMode mBlendMode = ParticleBlendMode::Additive;
};

class ParticleSystemDef : public IAsset {
public:
    nString mSystemName;
    ParticleSystemID mID;
    std::vector<ParticleEmitterDef> mEmitters;
};