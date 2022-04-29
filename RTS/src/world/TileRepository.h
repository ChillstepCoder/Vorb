#pragma once

#include "Tile.h"

struct TileFileData {
    f32v3 dims = f32v3(1.0f, 1.0f, 1.0f);
    f32v3 colliderDims = f32v3(0.5f, 0.5f, 1.0f);
    TileShape tileShape = TileShape::BLOCK;
    TileCollisionShape colliderShape = TileCollisionShape::NONE;
    TileTextureMethod textureMethod = TileTextureMethod::SIMPLE;
    TileResource resource = TileResource::NONE;
    ui8 pathWeight = 255;
    ui8 layer = 2;
    std::string textureName;
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
    static const TileData& getTileData(const std::string& name) {
        // TOOD: Hashed string and error handling
        TileID id = sTileIdMapping[name];
        return sTileData[id];
    }
    static TileID getTile(const std::string& name) {
        return sTileIdMapping[name];
    }

    static const std::vector<TileData>& getAllTileData() { return sTileData;  }

private:
    static std::unordered_map<std::string, TileID> sTileIdMapping;
    static std::vector<TileData> sTileData;
};
