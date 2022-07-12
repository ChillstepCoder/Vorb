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


struct TileOrientation {
    Cartesian orientationBase : 2;
    Cartesian orientationMid : 2;
    Cartesian orientationTop : 2;
    //Cartesian PADDING : 2; // Use this for something?
};
static_assert(sizeof(TileOrientation) == 1);

struct TileWall {
    TileID wallID = TILE_ID_NONE;
    TileID paintID = TILE_ID_NONE;

    void clear() { wallID = TILE_ID_NONE; paintID = TILE_ID_NONE; }
};

struct TileWalls {
    TileWalls() : walls{ {TILE_ID_NONE, TILE_ID_NONE}, {TILE_ID_NONE, TILE_ID_NONE}, {TILE_ID_NONE, TILE_ID_NONE}, {TILE_ID_NONE, TILE_ID_NONE} } {}
    static_assert(sizeof(TileWall) == 4, "Make sure constructor still works");
    union {
        TileWall walls[4];
        struct {
            TileWall south;
            TileWall west;
            TileWall east;
            TileWall north;
        };
    };
};

class Tile {
    friend class TileContainer;
    friend class ChunkGenerator;
public:
	Tile() {};
    Tile(TileID ground, TileID mid, TileID top);
    Tile(TileID ground, TileID mid, TileID top, f32 zPos);
    Tile(TileID ground, TileID mid, TileID top, f32 zPos, TileFlags flags);

    bool hasFlagMainThread(TileFlags flag) const { return tileFlags.isBitSet(flag); }
    bool hasFlagThreadSafe(TileFlags flag) const { return tileFlagsThreadSafe.isBitSet(flag); }

    bool hasHarvestableResource(TileResource resource, TileLayer* outLayer) const;

    void updateThreadSafeLayers();

    // Only nav thread can access this data
    ui16 getNavNodeIndex() const { assert(IS_NAV_THREAD()); return navNodeIndex; }
    void setNavNodeIndex(ui16 index) const { assert(IS_NAV_THREAD()); navNodeIndex = index; }

    ui8 getPathWeightMainThread() const { assert(IS_MAIN_THREAD()); return pathWeight; }
    ui8 getPathWeightNavThread() const { assert(IS_NAV_THREAD()); return pathWeightThreadSafe; }

    f32 getGroundZPositionUncompressedMainThread() const { assert(IS_MAIN_THREAD()); return (f32)groundZPositionCompressed* UNCOMPRESS_Z_UNITS_PER_TILE_MULT + (f32)MIN_WORLD_HEIGHT; }
    f32 getGroundZPositionUncompressedThreadSafe() const { /*assert(!IS_MAIN_THREAD());*/ return (f32)groundZPositionCompressedThreadSafe * UNCOMPRESS_Z_UNITS_PER_TILE_MULT + (f32)MIN_WORLD_HEIGHT; }

	const TileID* getLayersMainThread() const { assert(IS_MAIN_THREAD()); return layers; }
    const TileID* getLayersThreadSafe() const { assert(!IS_MAIN_THREAD()); return layersThreadSafe; }

    const TileOrientation& getOrientationMainThread() const { assert(IS_MAIN_THREAD()); return orientation; }
    const TileOrientation& getOrientationThreadSafe() const { assert(!IS_MAIN_THREAD()); return orientation; }

    bool isEmptyMainThread() const { assert(IS_MAIN_THREAD()); return layers[TILE_LAYER_GROUND] == TILE_ID_NONE && layers[TILE_LAYER_MID] == TILE_ID_NONE && layers[TILE_LAYER_TOP] == TILE_ID_NONE; }
    bool isEmptyThreadSafe() const { assert(!IS_MAIN_THREAD()); return layersThreadSafe[TILE_LAYER_GROUND] == TILE_ID_NONE && layersThreadSafe[TILE_LAYER_MID] == TILE_ID_NONE && layersThreadSafe[TILE_LAYER_TOP] == TILE_ID_NONE; }

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
    void setPathWeight(ui8 weight, bool isReadLocked);
    void setGroundZPosition(f32 groundZPosition, bool isReadLocked);
    void updateCollision(bool isReadLocked);
    bool isUpdateQueued() { return tileFlags.isBitSet(TileFlags::TILE_FLAG_QUEUED_THREADSAFE_UPDATE); }

    // ================================= Data =================================
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
    TileOrientation orientation;
    TileOrientation orientationThreadSafe;
    ui16 groundZPositionCompressed;
    ui16 groundZPositionCompressedThreadSafe;
    BitFlags<TileFlags> tileFlags;
    BitFlags<TileFlags> tileFlagsThreadSafe;
	// Collision stuff
    mutable ui16 navNodeIndex = UINT16_MAX; // Modified by nav thread
    ui8 pathWeight = 255u;
    ui8 pathWeightThreadSafe = 255u;
};
// TODO: Could we limit tile counts by category? Ground tile ID would be 8? mid tile ID also 8, only top layer has ui16?
static_assert(sizeof(Tile) == 24, "Keep small");