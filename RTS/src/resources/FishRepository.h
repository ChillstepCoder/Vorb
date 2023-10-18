#pragma once

#include "definitions/FishDef.h"

DECL_VIO(class IOManager);

#include "resources/IAssetRepository.h"

class TextureRepository;
class ItemRepository;
class ModelRepository;

class FishRepository : public IAssetRepository<FishDef>
{
    friend class TileEditorPanel;
public:
    ASSET_REPOSITORY_COMMON_CODE(FishRepository, FishDef, AssetType::Fish)

    bool saveAsset(AssetID assetId) override { panic("Cannot save fish yet"); }

    StrToken getAssetExtension() const override { return CStrToken("fish"); }
    const char* const getAssetTypeDisplayName() const override { return "Fish"; }

protected:
    void onRegisteredAsset(AssetID id) override;
    AssetLoadFunc getAssetLoadFunc() override;

};

