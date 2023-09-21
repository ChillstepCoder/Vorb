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

protected:
    AssetLoadFunc getAssetLoadFunc() override;

};

