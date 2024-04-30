#include "stdafx.h"
#include "TileRepository.h"

#include <Vorb/io/IOManager.h>

#include "resources/MaterialRepository.h"
#include "tile/Stairs.h"
#include "item/ItemRepository.h"
#include "resources/ModelRepository.h"
#include "physics/CollisionShapeRepository.h"


TileRepository::TileRepository(vio::IOManager& ioManager, CollisionShapeRepository& collisionCache) : mCollisionShapeCache(collisionCache), IAssetRepository<TileDef>(ioManager) {
}
TileRepository::~TileRepository() = default;

void TileRepository::onRegisteredAsset(AssetID id) {
    assert(id < TILE_ID_NONE);

    TileDef& def = *mAssets[id];
    YmlSerializer::readFileData(readFileToString(mAssetRegistry[id].mFilePath), def);

    // Copy all data
    assert(def.layer < TILE_LAYER_COUNT);
  
    // TODO: Change when bullet is replaced
    if (def.collisionShapeType != CollisionShapes::NONE) {
        assert(def.collisionHalfExtents.x == def.collisionHalfExtents.y); // TODO: Support oblong?
        def.collisionShapeID = mCollisionShapeCache.getOrAddCollisionShape(def.collisionShapeType, def.collisionHalfExtents);
    }

    // Nav blocking
    if (def.pathWeight == 0) {
        def.navMask = 0;

        // If its extends into neighbor tiles larger than player collider radius (player collider is 0.24 radius)
        // then we are blocking
        const f32 overlapIntoNeighborX = def.collisionHalfExtents.x - 0.5f;
        const f32 overlapIntoNeighborDiagonal = def.collisionHalfExtents.x - 0.7f;
        constexpr f32 AGENT_RADIUS = 0.241f; // TODO: Enforce match to data
        // Distance where if we have two of these objects that have an empty block
        // in between, an agent can no longer path
        // TODO: Refine these values, 0.5f should be right but 0.85 is kinda arbitrary

        // RADIUS X = 0.5
        // RADIUS DIAGONAL = 0.707
        constexpr f32 MEDIUM_OVERLAP_DISTANCE = 0.5f - AGENT_RADIUS;
        constexpr f32 LARGE_OVERLAP_DISTANCE = 0.85f - AGENT_RADIUS;
        if (overlapIntoNeighborX >= LARGE_OVERLAP_DISTANCE) {
            def.navBlockerType = NavBlockerType::LARGE;
        }
        else if (overlapIntoNeighborX >= MEDIUM_OVERLAP_DISTANCE) {
            def.navBlockerType = NavBlockerType::MEDIUM;
        }
    }

    // For now all tiles are always loaded
    mLoadedAssets[id]->store(true);
}

void TileRepository::fixupRegisteredAsset(AssetID id) {

    if (mTileRecipes.size() != mAssets.size()) {
        mTileRecipes.resize(mAssets.size());
    }

    MaterialRepository& materialRepo = MaterialRepository::get();
    ItemRepository& itemRepo = ItemRepository::get();
    TileDef& def = *mAssets[id];
    for (size_t i = 0; i < def.itemDrops.size(); ++i) {
        def.itemDrops[i].id = itemRepo.getAssetID(def.itemDrops[i].itemName);
    }

    // Recipes
    Recipe& recipe = mTileRecipes[id];
    recipe.mItemCount = def.recipeData.size();
    recipe.mItems = std::make_unique_for_overwrite<ItemStack[]>(recipe.mItemCount);
    for (ui32 i = 0; i < recipe.mItemCount; ++i) {
        recipe.mItems[i].quantity = def.recipeData[i].count;
        recipe.mItems[i].id = itemRepo.getAssetID(def.recipeData[i].itemName);
    }

    // Model
    if (def.modelRef.isValid()) {
        def.modelId = def.modelRef.getAssetID();
        def.shape = TileShape::MODEL;

        // Make sure the model variant is valid
        const ModelDef& modelDef = ModelRepository::get().getLoadedOrUnloadedAsset(def.modelId);
        for (auto it = def.modelVariants.begin(); it != def.modelVariants.end(); ++it) {
            if (*it >= modelDef.mVariants.size()) {
                LOG_ERROR("Had to remove invalid model variant {} from tile {} - referencing model {}", *it, def.getName().toString().c_str(), modelDef.getName().toString().c_str());
                *it = 0;
            }
        }

        // Always at least one
        if (def.modelVariants.size() == 0) {
            def.modelVariants.push_back(0);
        }
    }
    else {
        def.modelVariants.clear();
        assert(def.materialNames.size());
        assert(def.materialNames.size() < MAX_TILE_MATERIAL_SLOTS);
        def.materialData.resize(def.materialNames.size());
        for (size_t j = 0; j < def.materialNames.size(); ++j) {
            assert(def.materialNames[j].isValid());
            def.materialData[j] = materialRepo.getMaterialDesc(def.materialNames[j]);
        };
    }

    // Nav bits
    if (def.shape == TileShape::STAIRS) {
        def.navMask = 0b01000010; // SOUTH and NORTH access
        def.heightOffsetSouth = STAIR_TILE_HEIGHT + 0.1f;
        def.heightOffsetNorth = 0.0f;
        def.heightOffsetWest = 0.0f;
        def.heightOffsetEast = 0.0f;
    }
    else {
        def.heightOffsetSouth = 0.0f;
        def.heightOffsetWest = 0.0f;
        def.heightOffsetEast = 0.0f;
        def.heightOffsetNorth = 0.0f;
    }
}
