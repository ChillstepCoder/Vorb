#pragma once

#include "tile/Tile.h"

#include "resources/IAssetRepository.h"

DECL_VIO(class IOManager);

class MaterialRepository;
class ItemRepository;
class ModelRepository;
class CollisionShapeRepository;


// TODO: Find all references of TileRepository::get().getLoadedOrUnloadedAsset( and tileRepo.getLoadedOrUnloadedAsset(
// and replace with flyweight accessors such as getTileShape(AssetID)
class TileRepository : public IAssetRepository<TileDef> {
    friend class ResourceManager;
public:
    ASSET_REPOSITORY_COMMON_CODE_CUSTOM_INIT(TileRepository, TileDef, AssetType::Tile);
    static void initInstance(vio::IOManager& ioManager, CollisionShapeRepository& collisionCache) {
            sInstance = std::make_unique<TileRepository>(ioManager, collisionCache);
    }
    TileRepository(vio::IOManager& ioManager, CollisionShapeRepository& collisionCache);
    ~TileRepository();

    // TODO: Recipe repository
    const Recipe& getRecipeForTile(TileID tileId) { return mTileRecipes[tileId]; }

    TileID getTileID(StrToken name) { return (TileID)getAssetID(name); }

    bool saveAsset(AssetID assetId) override { panic("Cannot save tiles yet"); }

private:
    AssetLoadFunc getAssetLoadFunc() override { return nullptr; } // TODO:?
    void onRegisteredAsset(AssetID id) override;
    void onAllAssetTypesRegistered() override;

    CollisionShapeRepository& mCollisionShapeCache;

    // TODO: Recipe repository
    inline static std::vector<Recipe> mTileRecipes;
};
