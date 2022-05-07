#pragma once

#include "TileConst.h"
#include "TileCollider.h"
#include "item/ItemStack.h"

// TODO: Do we need rendering here?
#include "rendering/texture/SubTexture.h"

constexpr inline ui16 compressTileZPosition(f32 zPosition) {
	return (ui32)((zPosition - MIN_WORLD_HEIGHT) * SCALED_Z_UNITS_PER_TILE);
}

enum class TileTextureMethod : ui8 {
    SIMPLE,
    CONNECTED,
    CONNECTED_WALL,
    VERTICAL,
    FLORA,
    WORLD_TILING,
    COUNT
};
KEG_ENUM_DECL(TileTextureMethod);

struct TileData {
    f32v3 dims = f32v3(1.0f);
    TileID id;
    ui8 layer = 2;
    ui8 pathWeight = 255;
    TileCollider collider;
    //ui8v2 tileDims = ui8v2(1); // 4x4 is max size
    TileShape shape = TileShape::BLOCK;
    TileResource resource = TileResource::NONE;
    SubTexture texture; // TODO: Model instead also make it a pointer this is huge?
    TileTextureMethod textureMethod;
    std::string name;
    std::vector<ItemDrop> itemDrops;
    std::vector<ItemStack> recipe;
};
#ifdef DEBUG // Release has different size
static_assert(sizeof(TileData) == 200, "Keep it small as possible");
#endif
// There is exactly 1 of these per integer x,y coordinate pair over the entire world
class TileBase {
    ui32 mStructureID; // Complicated structures like large rock formations, or buildings, or anything
    TileID mTerrainAlignedTile; // Rocks, trees, berry bushes, whatever
    ui16 navNodeIndex = UINT16_MAX; // Modified by nav thread only
    TileBaseFlags mFlags;
    ui8 mPathWeight;
};
static_assert(sizeof(TileBase) == 12, "Keep small");

class Tile {
    friend class Chunk;
    friend class ChunkGenerator;
public:
	Tile() {};
    Tile(TileID ground, TileID mid, TileID top);
    Tile(TileID ground, TileID mid, TileID top, f32 zPos);
    Tile(TileID ground, TileID mid, TileID top, f32 zPos, TileFlags flags);

    bool hasFlagMainThread(TileFlags flag) const { return tileFlags & flag; }
    bool hasFlagThreadSafe(TileFlags flag) const { return tileFlagsThreadSafe & flag; }

    bool hasHarvestableResource(TileResource resource, TileLayer* outLayer) const;

    void updateThreadSafeLayers();

    // Only nav thread can access this data
    ui16 getNavNodeIndex() const { assert(IS_NAV_THREAD()); return navNodeIndex; }
    void setNavNodeIndex(ui16 index) { assert(IS_NAV_THREAD()); navNodeIndex = index; }

    ui8 getPathWeightMainThread() const { assert(IS_MAIN_THREAD()); return pathWeight; }
    ui8 getPathWeightNavThread() const { assert(IS_NAV_THREAD()); return pathWeightThreadSafe; }

    f32 getBaseZPositionUncompressedMainThread() const { assert(IS_MAIN_THREAD()); return (f32)baseZPositionCompressed* UNCOMPRESS_Z_UNITS_PER_TILE_MULT + (f32)MIN_WORLD_HEIGHT; }
    f32 getBaseZPositionUncompressedThreadSafe() const { return (f32)baseZPositionCompressedThreadSafe * UNCOMPRESS_Z_UNITS_PER_TILE_MULT + (f32)MIN_WORLD_HEIGHT; }

    const TileCollider* tryGetColliderMainThread() const;
    const TileCollider* tryGetColliderThreadSafe() const;

	const TileID* getLayersMainThread() const { assert(IS_MAIN_THREAD()); return layers; }
    const TileID* getLayersThreadSafe() const { return layersThreadSafe; }

private:
    // Mutators are accessed only via chunk generator or chunk methods (friend classes)
    bool canAddTile(const TileData& tile) const;
    void addTile(const TileData& tile, bool isReadLocked);
    bool tryAddTile(const TileData& tile, bool isReadLocked);
    void setTileLayer(TileLayer layer, TileID id, bool isReadLocked);
    void setTileFlag(TileFlags flag, bool isReadLocked);
    void setTileFlags(TileFlags flags, bool isReadLocked);
    void clearTileFlag(TileFlags flag, bool isReadLocked);
    void clearTileFlags(bool isReadLocked);
    void clearTileCollisionFlags(bool isReadLocked);
    void setPathWeight(ui8 weight, bool isReadLocked);
    void setBaseZPosition(f32 baseZPosition, bool isReadLocked);
    void updateCollision(bool isReadLocked);
    bool isUpdateQueued() { return tileFlags.isBitSet(TileFlags::TILE_FLAG_QUEUED_THREADSAFE_UPDATE); }

    union { // These can safely be modified at any time and will only be accessed by the main thread
        struct {
            TileID groundLayer; // Walls, floors, foundation     // ALWAYS BOX COLLISION
            TileID midLayer;    // Rugs, things on top of furniture, flora  // NO COLLIDE ONLY
            TileID topLayer;    // Furniture, props, walls trees // ALLOWS CUSTOM COLLISION
        };
        TileID layers[TILE_LAYER_COUNT] = { TILE_ID_NONE, TILE_ID_NONE, TILE_ID_NONE };
    };
    union { // These can only be modified when there is no read lock or active nav tasks, these are read by worker threads
        struct {
            TileID groundLayerThreadSafe;
            TileID midLayerThreadSafe;
            TileID topLayerThreadSafe;
        };
        TileID layersThreadSafe[TILE_LAYER_COUNT] = { TILE_ID_NONE, TILE_ID_NONE, TILE_ID_NONE };
    };
    ui16 baseZPositionCompressed;
    ui16 baseZPositionCompressedThreadSafe;
    BitFlags<TileFlags> tileFlags;
    BitFlags<TileFlags> tileFlagsThreadSafe;
	// Collision stuff
    ui16 navNodeIndex = UINT16_MAX; // Modified by nav thread
    ui8 pathWeight = 255u;
    ui8 pathWeightThreadSafe = 255u;
};
// TODO: Could we limit tile counts by category? Ground tile ID would be 8? mid tile ID also 8, only top layer has ui16?
static_assert(sizeof(Tile) == 22, "Keep small");