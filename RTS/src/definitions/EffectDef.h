#pragma once

#include "definitions/ParticleSystemDef.h"

class EffectDef : public IAsset {
public:
    friend class EffectRepository;

    DEFAULT_ASSET_CONSTRUCTOR(EffectDef, AssetType::Effect);

    //StrToken mSFXName;
    //int priority
    //flags flags (isTracked, isReplicated, etc)

    const ParticleSystemDef* getLoadedParticleSystemDef() const;
    StrToken mParticleSystemName;
};
SERIALIZABLE_SIMPLE(EffectDef,
    make_field(o.mParticleSystemName, "psys"sv)
);
