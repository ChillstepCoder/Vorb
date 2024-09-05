#pragma once

#include "tile/Tile.h"

#include "resources/IAssetRepository.h"

DECL_VIO(class IOManager);

class MaterialRepository;
class ItemRepository;
class ModelRepository;


// TODO: Find all references of TileRepository::get().getLoadedOrUnloadedAsset( and tileRepo.getLoadedOrUnloadedAsset(
// and replace with flyweight accessors such as getTileShape(AssetID)
class TileRepository : public IAssetRepository<TileDef> {
    friend class ResourceManager;
public:
    ASSET_REPOSITORY_COMMON_CODE_CUSTOM_INIT(TileRepository, TileDef, AssetType::Tile);
    static void initInstance(vio::IOManager& ioManager) {
            sInstance = std::make_unique<TileRepository>(ioManager);
    }
    TileRepository(vio::IOManager& ioManager);
    ~TileRepository();

    // TODO: Recipe repository
    const Recipe& getRecipeForTile(TileID tileId) const { return mTileRecipes[tileId]; }
    TileShape getTileShape(TileID tileId) const { return mAllTileShapes[tileId]; }
    ModelID getTileModelID(TileID tileId) const { return mAllTileModelIds[tileId]; }

    TileID getTileID(StrToken name) const { return (TileID)getAssetID(name); }

    DEFAULT_ASSET_SAVE_FUNC();

    StrToken getAssetExtension() const override { return CStrToken("tile"); }
    const char* const getAssetTypeDisplayName() const override { return "Tile"; }

private:

    AssetLoadFunc getAssetLoadFunc() override { return nullptr; } // TODO:?
    void onRegisteredAsset(AssetID id) override;
    void fixupRegisteredAsset(AssetID id) override;
    void onAllAssetTypesRegistered() override;
    // TODO: Recipe repository
    inline static std::vector<Recipe> mTileRecipes;

    // Flyweight lookup data for cache friendly generation and meshing
    std::vector<TileShape> mAllTileShapes;
    std::vector<ModelID> mAllTileModelIds;
};
