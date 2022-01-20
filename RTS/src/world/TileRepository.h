#pragma once

#include "Tile.h"
#include "rendering/SpriteData.h"

#include "item/ItemStack.h"

struct TileFileData {
    f32v3 colliderDims = f32v3(0.5f, 0.5f, 1.0f);
    TileShape tileShape = TileShape::BLOCK;
    TileCollisionShape colliderShape = TileCollisionShape::NONE;
    TileResource resource = TileResource::NONE;
    ui8 pathWeight = 255;
    ui8 layer = 2;
    std::string textureName;
    Array<ItemDropDef> itemDrops;
    Array<ItemInputDef> recipes;
};
KEG_TYPE_DECL(TileFileData);

struct TileData {
    TileID id;
    ui8 layer = 2;
    ui8 pathWeight = 255;
    TileCollider collider;
    //ui8v2 tileDims = ui8v2(1); // 4x4 is max size
    TileShape shape = TileShape::BLOCK;
    TileResource resource = TileResource::NONE;
    SpriteData spriteData;
    std::string name;
    std::vector<ItemDrop> itemDrops;
    std::vector<ItemStack> recipe;
};
#ifdef DEBUG // Release has different size
static_assert(sizeof(TileData) == 200, "Keep it small as possible");
#endif

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
