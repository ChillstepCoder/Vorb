#include "stdafx.h"
#include "TileRepository.h"

#include <Vorb/io/IOManager.h>

#include "resources/MaterialRepository.h"
#include "tile/Stairs.h"
#include "item/ItemRepository.h"
#include "resources/ModelRepository.h"
#include "physics/CollisionShapeRepository.h"

KEG_TYPE_DEF_SAME_NAME(TileFileData, kt) {
    kt.addValue("mat0", keg::Value::basic(offsetof(TileFileData, material0), keg::BasicType::STRING));
    kt.addValue("mat1", keg::Value::basic(offsetof(TileFileData, material1), keg::BasicType::STRING));
    kt.addValue("mat2", keg::Value::basic(offsetof(TileFileData, material2), keg::BasicType::STRING));
    kt.addValue("model", keg::Value::basic(offsetof(TileFileData, modelName), keg::BasicType::STRING));
    kt.addValue("texture_method", keg::Value::custom(offsetof(TileFileData, textureMethod), "TileTextureMethod", true));
    kt.addValue("dims", keg::Value::basic(offsetof(TileFileData, dims.x), keg::BasicType::F32_V3));
    kt.addValue("path_weight", keg::Value::basic(offsetof(TileFileData, pathWeight), keg::BasicType::UI8));
    kt.addValue("layer", keg::Value::basic(offsetof(TileFileData, layer), keg::BasicType::UI8));
    kt.addValue("col_shape", keg::Value::custom(offsetof(TileFileData, colliderShape), "CollisionShapes", true));
    kt.addValue("col_half_dims", keg::Value::basic(offsetof(TileFileData, colliderHalfExtents.x), keg::BasicType::F32_V3));
    kt.addValue("shape", keg::Value::custom(offsetof(TileFileData, tileShape), "TileShape", true));
    kt.addValue("resource", keg::Value::custom(offsetof(TileFileData, resource), "TileHarvestable", true));
    kt.addValue("drops", keg::Value::array(offsetof(TileFileData, itemDrops), keg::Value::custom(0, "ItemDropDef", false)));
    kt.addValue("recipe", keg::Value::array(offsetof(TileFileData, recipe), keg::Value::custom(0, "ItemInputDef", false)));
}
static_assert(MAX_TILE_MATERIAL_SLOTS == 3, "Update matX");

bool TileRepository::loadTileFile(vio::IOManager& ioManager, const vio::Path& path, const MaterialRepository& materialRepository, ItemRepository& itemRepository, ModelRepository& modelRepository, CollisionShapeRepository& shapeRepository) {
    // Read file
    return ioManager.parseFileAsKegObjectMap(path, makeFunctor([&](Sender s, const nString& key, keg::Node value) {
        keg::ReadContext& readContext = *((keg::ReadContext*)s);

        TileData tileData;
        TileFileData fileData;

        // Load data
        keg::parse((ui8*)&fileData, value, readContext, &KEG_GLOBAL_TYPE(TileFileData));
        tileData.name = key;

        // Copy all data
        tileData.layer = fileData.layer;
        assert(tileData.layer < TILE_LAYER_COUNT);
        tileData.pathWeight = fileData.pathWeight;
        tileData.harvestable = fileData.resource;
        tileData.textureMethod = fileData.textureMethod;
        tileData.dims = fileData.dims;
        if (fileData.colliderShape != CollisionShapes::NONE) {
            assert(fileData.colliderHalfExtents.x == fileData.colliderHalfExtents.y); // TODO: Support oblong?
            tileData.collisionShapeID = shapeRepository.getOrAddCollisionShape(fileData.colliderShape, fileData.colliderHalfExtents);
        }

        // Item drops
        tileData.itemDrops.resize(fileData.itemDrops.size());
        for (size_t i = 0; i < tileData.itemDrops.size(); ++i) {
            tileData.itemDrops[i].countRange = fileData.itemDrops[i].countRange;
            tileData.itemDrops[i].id = itemRepository.getItem(fileData.itemDrops[i].itemName).getID();
        }

        // Recipes
        Recipe recipe;
        recipe.mItemCount = fileData.recipe.size();
        recipe.mItems = std::unique_ptr<ItemStack[]>(new ItemStack[recipe.mItemCount]);
        for (ui32 i = 0; i < recipe.mItemCount; ++i) {
            recipe.mItems[i].quantity = fileData.recipe[i].count;
            recipe.mItems[i].id = itemRepository.getItem(fileData.recipe[i].itemName).getID();
        }
        sTileRecipes.emplace_back(std::move(recipe));

        TileID nextId = (TileID)sTileData.size();
        tileData.id = nextId;
        assert(nextId < UINT16_MAX); // Make sure we dont roll over
        assert(sTileIdMapping.find(key) == sTileIdMapping.end()); // Duplicate name
        // TODO: error handling  for missing  sprite
        if (fileData.modelName.size()) {
            tileData.modelId = modelRepository.getModelID(fileData.modelName);
            tileData.shape = TileShape::MODEL;
        }
        else {
            assert(fileData.material0.size());
            tileData.materialData[0] = materialRepository.getMaterialData(fileData.material0);
            if (fileData.material1.size()) {
                tileData.materialData[1] = materialRepository.getMaterialData(fileData.material1);
            }
            if (fileData.material2.size()) {
                tileData.materialData[2] = materialRepository.getMaterialData(fileData.material2);
            }
            static_assert(MAX_TILE_MATERIAL_SLOTS == 3);
            tileData.shape = fileData.tileShape;
        }

        if (tileData.pathWeight == 0) {
            tileData.navMask = 0;

            // If its extends into neighbor tiles larger than player collider radius (player collider is 0.24 radius)
            // then we are blocking
            const f32 overlapIntoNeighborX = fileData.colliderHalfExtents.x - 0.5f;
            const f32 overlapIntoNeighborDiagonal = fileData.colliderHalfExtents.x - 0.7f;
            constexpr f32 AGENT_RADIUS = 0.241f; // TODO: Enforce match to data
            // Distance where if we have two of these objects that have an empty block
            // in between, an agent can no longer path
            // TODO: Refine these values, 0.5f should be right but 0.85 is kinda arbitrary

            // RADIUS X = 0.5
            // RADIUS DIAGONAL = 0.707
            constexpr f32 MEDIUM_OVERLAP_DISTANCE = 0.5f - AGENT_RADIUS;
            constexpr f32 LARGE_OVERLAP_DISTANCE = 0.85f - AGENT_RADIUS;
            if (overlapIntoNeighborX >= LARGE_OVERLAP_DISTANCE) {
                tileData.navBlockerType = NavBlockerType::LARGE;
            } else if (overlapIntoNeighborX >= MEDIUM_OVERLAP_DISTANCE) {
                tileData.navBlockerType = NavBlockerType::MEDIUM;
            }
        }

        // Nav bits
        if (tileData.shape == TileShape::STAIRS) {
            tileData.navMask = 0b01000010; // SOUTH and NORTH access
            tileData.heightOffsetSouth = STAIR_TILE_HEIGHT + 0.1f;
            tileData.heightOffsetNorth = 0.0f;
            tileData.heightOffsetWest = 0.0f;
            tileData.heightOffsetEast = 0.0f;
        }
        else {
            tileData.heightOffsetSouth = 0.0f;
            tileData.heightOffsetWest = 0.0f;
            tileData.heightOffsetEast = 0.0f;
            tileData.heightOffsetNorth = 0.0f;
        }

        sTileIdMapping[key] = nextId;
        // TODO: Serialize the string > ID mapping
        sTileData.emplace_back(std::move(tileData));
    }));
}