#include "stdafx.h"
#include "TileRepository.h"

#include <Vorb/io/IOManager.h>

#include "resources/MaterialRepository.h"
#include "tile/Stairs.h"
#include "item/ItemRepository.h"
#include "resources/ModelRepository.h"


TileRepository::TileRepository(vio::IOManager& ioManager) : IAssetRepository<TileDef>(ioManager) {
}
TileRepository::~TileRepository() = default;

void TileRepository::onRegisteredAsset(AssetID id) {
    assert(id < TILE_ID_NONE);

    TileDef& def = *mAssets[id];
    YmlSerializer::readFileData(readFileToString(mAssetRegistry[id].mFilePath), def);

    // Copy all data
    assert(def.layer < TILE_LAYER_COUNT);
  
    // Nav blocking
    if (def.pathWeight == 0) {
        def.navMask = 0;
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
    // TODO: DROPS
   /* for (size_t i = 0; i < def.itemDrops.size(); ++i) {
        def.itemDrops[i].id = itemRepo.getAssetID(def.itemDrops[i].itemName);
    }*/

    // Recipes
    Recipe& recipe = mTileRecipes[id];
    if (def.recipeData.size() > MAX_ITEMS_IN_RECIPE) {
        LOG_ERROR("Recipe {} found with item count of {} greater than max of {}, deleting entries",
            def.getName().toString(), def.recipeData.size(), MAX_ITEMS_IN_RECIPE);
        def.recipeData.resize(MAX_ITEMS_IN_RECIPE);
    }
    recipe.numItems = def.recipeData.size();
    for (size_t i = 0; i < def.recipeData.size(); ++i) {
        recipe.quantities[i] = def.recipeData[i].count;
        recipe.itemIds[i] = itemRepo.getAssetID(def.recipeData[i].itemName);
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

    std::fill(def.transformations.begin(), def.transformations.end(), TILE_ID_NONE);

    // Transformations
    for (auto& t : def.transformationDefs) {
        assert(t.type != TileTransformationType::COUNT);
        assert(def.transformations[e_cast(t.type)] == TILE_ID_NONE);
        def.transformations[e_cast(t.type)] = t.target.getAssetID();
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
