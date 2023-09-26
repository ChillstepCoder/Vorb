#pragma once

#include "definitions/TileGrassDef.h"

#include "rendering/material/MaterialData.h"

#include "resources/IAssetRepository.h"


class TileGrassRepository : public IAssetRepository<TileGrassDef> {
    friend class TileEditorPanel;
public:
    ASSET_REPOSITORY_COMMON_CODE(TileGrassRepository, TileGrassDef, AssetType::TileGrass)

    bool saveAsset(AssetID assetId) override { panic("Cannot save tile grass yet"); }

protected:
    AssetLoadFunc getAssetLoadFunc() override;
    void onRegisteredAsset(AssetID id) override;
    void onAllAssetTypesRegistered() override;
};

