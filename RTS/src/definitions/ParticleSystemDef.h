#pragma once

#include "definitions/ParticleEmitterDef.h"
#include "rendering/particle/ParticleSystemUserParameters.h"

#include "rendering/particle/ParticleSystemInputs.h"

class ParticleSystemDef : public IAsset {
public:
    DEFAULT_ASSET_CONSTRUCTOR(ParticleSystemDef, AssetType::ParticleSystem);

    f32 mLifetimeSec = 3.0f;
    std::vector<ParticleEmitterDef> mEmitters;

    std::shared_ptr<ParticleSystemInputs> mDefaultInputs = std::make_shared<ParticleSystemInputs>();
    ParticleSystemUserParameterMap mUserParameters;
};
// YML Serialization handled manually by ParticleSystemRepository