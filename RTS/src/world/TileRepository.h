#pragma once

#include "Tile.h"
#include "rendering/SpriteData.h"

struct TileData {
    SpriteData spriteData;
    TileCollisionShape collisionShape = TileCollisionShape::NONE;
    f32 colliderHeight = 1.0f;
    f32v2 colliderDimsXY = f32v2(0.5f, -1.0f);
    ui8 pathWeight = 255;
    ui8v2 dims = ui8v2(1); // 4x4 is max size
    ui8 rootPos = 0;
    std::string name;
    std::string textureName;
    std::string resourceName;
    TileShape shape = TileShape::BLOCK;
    TileResource resource = TileResource::NONE;
    Array<ItemDropDef> itemDrops;
};
KEG_TYPE_DECL(TileData);

// TODO: non static
class TileRepository {
    friend class ResourceManager;
public:
    static const TileData& getTileData(TileID tileId) {
        assert(sTileData.find(tileId) != sTileData.end());
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

private:
    static std::unordered_map<std::string, TileID> sTileIdMapping;
    static std::unordered_map<TileID, TileData> sTileData;
};
