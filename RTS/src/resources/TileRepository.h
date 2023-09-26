#pragma once

#include "tile/Tile.h"

DECL_VIO(class IOManager);

class MaterialRepository;
class ItemRepository;
class ModelRepository;
class CollisionShapeRepository;


// TODO: non static IAssetRepository
class TileRepository {
    friend class ResourceManager;
public:
    static const TileDef& getTileData(TileID tileId) {
        assert(tileId < sTileData.size());
        return sTileData[tileId];
    }
    static const TileDef& getTileData(StrToken tileToken) {
        // TOOD: Hashed string and error handling
        TileID id = sTileIdMapping[tileToken];
        return sTileData[id];
    }
    static TileID getTile(StrToken tileToken) {
        auto&& it = sTileIdMapping.find(tileToken);
        assert(it != sTileIdMapping.end());
        return it->second;
    }

    static const std::vector<TileDef>& getAllTileData() { return sTileData;  }
    static const Recipe& getRecipeForTile(TileID tileId) { return sTileRecipes[tileId]; }

    static bool loadTileFile(vio::IOManager& ioManager, const vio::Path& path, CollisionShapeRepository& shapeRepository);

private:
    inline static std::unordered_map<StrToken, TileID> sTileIdMapping;
    inline static std::vector<TileDef> sTileData;
    inline static std::vector<Recipe> sTileRecipes;
};
