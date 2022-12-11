#pragma once

#include "tile/Tile.h"
#include "util/StrToken.h"

DECL_VIO(class IOManager);

class TextureRepository;
class ItemRepository;
class ModelRepository;
class CollisionShapeRepository;

struct TileFileData {
    f32v3 dims = f32v3(1.0f, 1.0f, 1.0f);
    f32v3 colliderHalfExtents = f32v3(0.5f, 0.5f, 1.0f);
    CollisionShapes colliderShape = CollisionShapes::NONE;
    TileShape tileShape = TileShape::BLOCK;
    TileTextureMethod textureMethod = TileTextureMethod::SIMPLE;
    TileResource resource = TileResource::NONE;
    ui8 pathWeight = 255;
    ui8 layer = 1;
    nString textureName;
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
        return sTileIdMapping[tileToken];
    }

    static const std::vector<TileData>& getAllTileData() { return sTileData;  }

    static bool loadTileFile(vio::IOManager& ioManager, const vio::Path& path, TextureRepository& textureRepository, ItemRepository& itemRepository, ModelRepository& modelRepository, CollisionShapeRepository& shapeRepository);

private:
    static std::unordered_map<StrToken, TileID> sTileIdMapping;
    static std::vector<TileData> sTileData;
};
