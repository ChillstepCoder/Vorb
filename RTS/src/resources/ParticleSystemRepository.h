#pragma once

#include "resources/IAssetRepository.h"
#include "definitions/ParticleSystemDef.h"
#include "rendering/material/MaterialDef.h"

class MaterialRepository;

class ParticleSystemRepository : public IAssetRepository<ParticleSystemDef> {
    ASSET_REPOSITORY_COMMON_CODE(ParticleSystemRepository, ParticleSystemDef, AssetType::ParticleSystem)

    MaterialID getDefaultMaterialID() const { return mDefaultMaterial; }
    void setDefaultMaterialID(MaterialID id);

    bool saveAsset(AssetID assetId) override;

    StrToken getAssetExtension() const override { return CStrToken("psys"); }
    const char* const getAssetTypeDisplayName() const override { return "Particle System"; }

    bool renderImguiAssetActions(AssetMetadata& asset) override;

private:
    void saveParticleEmitter(ryml::NodeRef& node, const ParticleEmitterDef& particleEmitter);
    bool loadParticleEmitter(ryml::ConstNodeRef node, ParticleEmitterDef& particleEmitter);
    void fixupLoadedAsset(AssetID assetId) override;

    MaterialID mDefaultMaterial = INVALID_MATERIAL_ID;
    AssetHandlePtr<MaterialDef> mDefaultMaterialHandle;

protected:
    AssetLoadFunc getAssetLoadFunc() override;
    std::vector<nString> mEmitterStrBuf; // We need to hold on to strings while saving
};

