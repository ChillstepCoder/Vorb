#pragma once

#include "TileConst.h"
#include "TileCollider.h"
#include "TileResource.h"
#include "item/ItemStack.h"

// TODO: Do we need rendering here?
#include "rendering/texture/SubTexture.h"

enum class TileLayer {
    Ground = 0,
    Mid = 1,
    Top = 2,
    COUNT = 3
};
static_assert(TILE_LAYER_COUNT == e_cast(TileLayer::COUNT));

enum class TileShape {
    THIN,  // Trees and flora
    BLOCK, // Most blocks
    FLOOR,
    WALL,
    DOOR,
    STAIRS,
    // Custom TODO
    COUNT
};
KEG_ENUM_DECL(TileShape);


struct ItemInputDef {
    nString itemName;
    ui32 count;
};
KEG_TYPE_DECL(ItemInputDef);

struct ItemDrop {
    ItemID id;
    ui32v2 countRange;
};

struct ItemDropDef {
    nString itemName;
    ui32v2 countRange;
};
KEG_TYPE_DECL(ItemDropDef);

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

// TODO: separate certain data into multiple arrays because right now every TileData lookup is a cache miss
// For example we only look up path weight when constructing the nav  graph, why not  have it in a separate vector?
struct TileData {
    f32v3 dims = f32v3(1.0f);
    TileID id;
   // TileCollider collider;
    //ui8v2 tileDims = ui8v2(1); // 4x4 is max size
    TileResource resource = TileResource::NONE;
    SubTexture texture; // TODO: Model instead also make it a pointer this is huge?
    TileTextureMethod textureMethod;
    ui8 layer = 2;
    TileShape shape = TileShape::BLOCK;
    ui8 pathWeight = 255;
    ui8 navMask = 0xff; // Access bits mapped to Cartesian8 based on default (SOUTH) orientation
    union {
        struct {
            f32 heightOffsetSouth;
            f32 heightOffsetEast;
            f32 heightOffsetWest;
            f32 heightOffsetNorth;
        };
        f32 heightOffsets[4];
    };
    std::string name;
    std::vector<ItemDrop> itemDrops;
    std::vector<ItemStack> recipe;
};

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
    bool isDoor = false;

    void clear() { wallID = TILE_ID_NONE; paintID = TILE_ID_NONE; }
    bool isValid() const { return wallID != TILE_ID_NONE; }
    bool canNavThrough() const { return isDoor || wallID == TILE_ID_NONE; }
};

struct TileWalls {
    TileWalls() : walls{ {TILE_ID_NONE, TILE_ID_NONE}, {TILE_ID_NONE, TILE_ID_NONE}, {TILE_ID_NONE, TILE_ID_NONE}, {TILE_ID_NONE, TILE_ID_NONE} } {}
    static_assert(sizeof(TileWall) == 6, "Make sure constructor still works");
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

// Per tile steering and navigation usage
struct TileNavData {
    union {
        struct {
            entt::entity e0; // Southwest
            entt::entity e1; // Southeast
            entt::entity e2; // Northwest
            entt::entity e3; // Northeast
        };
        entt::entity entities[4];
    };
    // Collision stuff
    mutable ui16 coarseNavNodeIndex = UINT16_MAX; // Modified by nav thread
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
    bool hasFlagsMaskAnyMainThread(TileFlagType mask) const { return tileFlags.isMaskPartiallySet(mask); }
    bool hasFlagsMaskAnyThreadSafe(TileFlagType mask) const { return tileFlags.isMaskPartiallySet(mask); }

    bool hasHarvestableResource(TileResource resource, TileLayer* outLayer) const;

    void updateThreadSafeLayers();

    // Only nav thread can access this data
    ui16 getNavNodeIndex_DEBUG_MAIN_THREAD() const { return navData.coarseNavNodeIndex; }
    ui16 getNavNodeIndex() const { assert(IS_NAV_THREAD()); return navData.coarseNavNodeIndex; }
    void setNavNodeIndex(ui16 index) const { assert(IS_NAV_THREAD()); navData.coarseNavNodeIndex = index; }
    bool canNavInDirection(Cartesian8 dir) const;
    f32 getEdgeHeightOffset(Cartesian dir) const;

    f32 getGroundZOffsetMainThread() const { assert(IS_MAIN_THREAD()); return groundZOffset; }
    f32 getGroundZOffsetThreadSafe() const { /*assert(!IS_MAIN_THREAD());*/ return groundZOffsetThreadSafe; }

	const TileID* getLayersMainThread() const { assert(IS_MAIN_THREAD()); return layers; }
    const TileID* getLayersThreadSafe() const { assert(!IS_MAIN_THREAD()); return layersThreadSafe; }

    Cartesian getOrientationMainThread(TileLayer layer) const;
    Cartesian getOrientationThreadSafe(TileLayer layer) const;

    bool isEmptyMainThread() const { assert(IS_MAIN_THREAD()); return layers[TILE_LAYER_GROUND] == TILE_ID_NONE && layers[TILE_LAYER_MID] == TILE_ID_NONE && layers[TILE_LAYER_TOP] == TILE_ID_NONE; }
    bool isEmptyThreadSafe() const { assert(!IS_MAIN_THREAD()); return layersThreadSafe[TILE_LAYER_GROUND] == TILE_ID_NONE && layersThreadSafe[TILE_LAYER_MID] == TILE_ID_NONE && layersThreadSafe[TILE_LAYER_TOP] == TILE_ID_NONE; }

private:
    // Mutators are accessed only via chunk generator or chunk methods (friend classes)
    bool canAddTileData(const TileData& tile) const;
    void addTileData(const TileData& tile, bool isReadLocked);
    void setTileLayer(TileLayer layer, TileID id, bool isReadLocked);
    void setTileFlag(TileFlags flag, bool isReadLocked);
    void setTileFlags(TileFlags flags, bool isReadLocked);
    void setOrientation(Cartesian dir, TileLayer layer, bool isReadLocked);
    void clearTileFlag(TileFlags flag, bool isReadLocked);
    void clearTileFlags(bool isReadLocked);
    void setGroundZPosition(f32 groundZPosition, bool isReadLocked);
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
    TileNavData navData;
    TileOrientation orientation = {};
    TileOrientation orientationThreadSafe = {};
    f32 groundZOffset;
    f32 groundZOffsetThreadSafe;
    BitFlags<TileFlags> tileFlags;
    BitFlags<TileFlags> tileFlagsThreadSafe;
};
// TODO: Could we limit tile counts by category? Ground tile ID would be 8? mid tile ID also 8, only top layer has ui16?
static_assert(sizeof(Tile) == 48, "Keep small");