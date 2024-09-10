#pragma once

#include "definitions/ParticleEmitterDef.h"

class ParticleSystemDef : public IAsset {
public:
    DEFAULT_ASSET_CONSTRUCTOR(ParticleSystemDef, AssetType::ParticleSystem);

    f32 mLifetimeSec = 3.0f;
    std::vector<ParticleEmitterDef> mEmitters;
};
