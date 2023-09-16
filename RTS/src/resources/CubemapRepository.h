#pragma once

#include "resources/IAssetRepository.h"
#include "definitions/rendering/CubemapDef.h"

class CubemapRepository : public IAssetRepository<CubemapDef> {
    ASSET_REPOSITORY_COMMON_CODE(CubemapRepository, CubemapDef, AssetType::Cubemap)

protected:
    AssetLoadFunc getAssetLoadFunc() override;
    AssetLoadFunc getAssetLoadRenderProcessFunc() override;
    std::any getUserData(AssetID id) override;

    // Must be called sequentially 0-6
    bool initFace(CubemapDef& def, int face, const gli::texture2d& rs);
    void computePBRMaps(CubemapDef& def);
    void createMipmaps(CubemapDef& def);
    void computeIrradianceMap(CubemapDef& def);
    void computePrefilterMap(CubemapDef& def);
};