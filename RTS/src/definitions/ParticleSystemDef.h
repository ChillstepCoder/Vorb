#pragma once

#include "rendering/particle/CPUParticleEmitterModule.h"

#include "resources/IAsset.h"

#include "rendering/particle/ParticleEnumTypes.h"

class MaterialShader;

typedef std::vector<std::unique_ptr<CPUParticleEmitterModule>> CPUParticleEmitterModuleVector;

class ParticleEmitterModuleContainer {
public:
    ParticleEmitterModuleContainer() {};
    ParticleEmitterModuleContainer(const ParticleEmitterModuleContainer& other) {
        *this = other;
    }
    ParticleEmitterModuleContainer& operator=(const ParticleEmitterModuleContainer& other) {
        mEmitterUpdate.resize(other.mEmitterUpdate.size());
        for (size_t i = 0; i < mEmitterUpdate.size(); ++i) {
            mEmitterUpdate[i] = other.mEmitterUpdate[i]->clone();
        }
        mParticleInit.resize(other.mParticleInit.size());
        for (size_t i = 0; i < mParticleInit.size(); ++i) {
            mParticleInit[i] = other.mParticleInit[i]->clone();
        }
        mParticleUpdate.resize(other.mParticleUpdate.size());
        for (size_t i = 0; i < mParticleUpdate.size(); ++i) {
            mParticleUpdate[i] = other.mParticleUpdate[i]->clone();
        }
        return *this;
    }

    CPUParticleEmitterModuleVector mEmitterUpdate;
    CPUParticleEmitterModuleVector mParticleInit;
    CPUParticleEmitterModuleVector mParticleUpdate;
};

class ParticleEmitterDef {
public:

    bool isValid() { return mShader != nullptr; }

    ParticleEmitterModuleContainer mModules;

    const MaterialShader* mShader = nullptr;
    nString mEmitterName;
    f32v2 mDefaultScale = f32v2(0.1f);
    color4 mDefaultColor = color::White;
    ui32 mMaxParticles = 2000;
    MaterialID mDefaultMaterialID = 0;
    f32 mLifetimeSec = 3.0f;
    f32 mDefaultParticleLifespanSec = 3.0f;
    bool mLooping = true;
    ParticleBlendMode mBlendMode = ParticleBlendMode::Additive;
};

class ParticleSystemDef : public IAsset {
public:
    DEFAULT_ASSET_CONSTRUCTOR(ParticleSystemDef);

    std::vector<ParticleEmitterDef> mEmitters;
};