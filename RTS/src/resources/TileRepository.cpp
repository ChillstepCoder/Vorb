#include "stdafx.h"
#include "TileRepository.h"

#include <Vorb/io/IOManager.h>

#include "resources/MaterialRepository.h"
#include "tile/Stairs.h"
#include "item/ItemRepository.h"
#include "resources/ModelRepository.h"
#include "physics/CollisionShapeRepository.h"

std::unordered_map<StrToken, TileID> TileRepository::sTileIdMapping;
std::vector<TileData> TileRepository::sTileData;

KEG_TYPE_DEF_SAME_NAME(TileFileData, kt) {
    kt.addValue("mat", keg::Value::basic(offsetof(TileFileData, materialName), keg::BasicType::STRING));
    kt.addValue("model", keg::Value::basic(offsetof(TileFileData, modelName), keg::BasicType::STRING));
    kt.addValue("texture_method", keg::Value::custom(offsetof(TileFileData, textureMethod), "TileTextureMethod", true));
    kt.addValue("dims", keg::Value::basic(offsetof(TileFileData, dims.x), keg::BasicType::F32_V3));
    kt.addValue("path_weight", keg::Value::basic(offsetof(TileFileData, pathWeight), keg::BasicType::UI8));
    kt.addValue("layer", keg::Value::basic(offsetof(TileFileData, layer), keg::BasicType::UI8));
    kt.addValue("col_shape", keg::Value::custom(offsetof(TileFileData, colliderShape), "CollisionShapes", true));
    kt.addValue("col_half_dims", keg::Value::basic(offsetof(TileFileData, colliderHalfExtents.x), keg::BasicType::F32_V3));
    kt.addValue("shape", keg::Value::custom(offsetof(TileFileData, tileShape), "TileShape", true));
    kt.addValue("resource", keg::Value::custom(offsetof(TileFileData, resource), "TileResource", true));
    kt.addValue("drops", keg::Value::array(offsetof(TileFileData, itemDrops), keg::Value::custom(0, "ItemDropDef", false)));
    kt.addValue("recipe", keg::Value::array(offsetof(TileFileData, recipe), keg::Value::custom(0, "ItemInputDef", false)));
}

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
        tileData.resource = fileData.resource;
        tileData.textureMethod = fileData.textureMethod;
        tileData.dims = fileData.dims;
        if (fileData.colliderShape != CollisionShapes::NONE) {
            tileData.collisionShapeID = shapeRepository.getOrAddCollisionShape(fileData.colliderShape, fileData.colliderHalfExtents);
        }

        // Item drops
        tileData.itemDrops.resize(fileData.itemDrops.size());
        for (size_t i = 0; i < tileData.itemDrops.size(); ++i) {
            tileData.itemDrops[i].countRange = fileData.itemDrops[i].countRange;
            tileData.itemDrops[i].id = itemRepository.getItem(fileData.itemDrops[i].itemName).getID();
        }

        // Recipes
        tileData.recipe.resize(fileData.recipe.size());
        for (size_t i = 0; i < tileData.recipe.size(); ++i) {
            tileData.recipe[i].quantity = fileData.recipe[i].count;
            tileData.recipe[i].id = itemRepository.getItem(fileData.recipe[i].itemName).getID();
        }

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
            assert(fileData.materialName.size());
            tileData.materialData = materialRepository.getMaterialData(fileData.materialName);
            tileData.shape = fileData.tileShape;
        }

        if (tileData.pathWeight == 0) {
            tileData.navMask = 0;
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