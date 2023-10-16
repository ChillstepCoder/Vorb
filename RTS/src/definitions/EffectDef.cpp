#include "stdafx.h"
#include "EffectDef.h"

#include "resources/asset/AssetHandleBundle.h"
#include "resources/IAssetRepository.h"

const ParticleSystemDef* EffectDef::getLoadedParticleSystemDef() const {
    if (mParticleSystemName.isValid() && mDependencies) {
        return &mDependencies->getLoadedAsset<ParticleSystemDef>(mParticleSystemName);
    }
    return nullptr;
}
