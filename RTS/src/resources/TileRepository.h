#pragma once

#include "tile/Tile.h"

DECL_VIO(class IOManager);

class MaterialRepository;
class ItemRepository;
class ModelRepository;
class CollisionShapeRepository;

struct TileFileData {
    f32v3 dims = f32v3(1.0f, 1.0f, 1.0f);
    f32v3 colliderHalfExtents = f32v3(0.5f, 0.5f, 1.0f);
    CollisionShapes colliderShape = CollisionShapes::NONE;
    TileShape tileShape = TileShape::BLOCK;
    TileTextureMethod textureMethod = TileTextureMethod::SIMPLE;
    TileHarvestable resource = TileHarvestable::NONE;
    ui16 maxHealth = 100;
    ui8 pathWeight = 255;
    ui8 layer = 1;
    nString material0;
    nString material1;
    nString material2;
    nString material3;
    nString material4;
    nString material5;
    nString material6;
    nString material7;
    nString modelName;
    Array<ItemDropDef> itemDrops;
    Array<ItemInputDef> recipe;
};
KEG_TYPE_DECL(TileFileData);

// TODO: non static
class TileRepository {
    friend class ResourceManager;
public:
    static const TileData& getTileData(TileID tileId) {
        assert(tileId < sTileData.size());
        return sTileData[tileId];
    }
    static const TileData& getTileData(StrToken tileToken) {
        // TOOD: Hashed string and error handling
        TileID id = sTileIdMapping[tileToken];
        return sTileData[id];
    }
    static TileID getTile(StrToken tileToken) {
        auto&& it = sTileIdMapping.find(tileToken);
        assert(it != sTileIdMapping.end());
        return it->second;
    }

    static const std::vector<TileData>& getAllTileData() { return sTileData;  }
    static const Recipe& getRecipeForTile(TileID tileId) { return sTileRecipes[tileId]; }

    static bool loadTileFile(vio::IOManager& ioManager, const vio::Path& path, const MaterialRepository& materialRepository, ItemRepository& itemRepository, ModelRepository& modelRepository, CollisionShapeRepository& shapeRepository);

private:
    inline static std::unordered_map<StrToken, TileID> sTileIdMapping;
    inline static std::vector<TileData> sTileData;
    inline static std::vector<Recipe> sTileRecipes;
};
