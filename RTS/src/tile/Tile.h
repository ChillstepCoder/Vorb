#pragma once

#include "TileConst.h"
#include "TileResource.h"
#include "tile/TileFlags.h"
#include "item/ItemStack.h"
#include "physics/CollisionShapes.h"

// TODO: Do we need rendering here?
#include "rendering/texture/SubTexture.h"

enum class TileLayer : ui8 {
    Ground = 0,
    Main = 1,
    COUNT = 2
};
static_assert(TILE_LAYER_COUNT == e_cast(TileLayer::COUNT));

enum class TileShape {
    THIN,  // Trees and flora
    BLOCK, // Most blocks
    FLOOR,
    WALL,
    DOOR,
    STAIRS,
    MODEL,
    // Custom TODO
    COUNT,
    NONE = COUNT
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
    CollisionShapeID collisionShapeID = INVALID_COLLISION_SHAPE_ID;
    TileResource resource = TileResource::NONE;
    SubTexture texture; // TODO: We dont use this when we have a model, make this a pointer? Its big
    MaterialID materialId;
    TileTextureMethod textureMethod;
    ModelID modelId = INVALID_MODEL_ID;
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
    Cartesian orientationMain : 2;
    //Cartesian PADDING : 4; // Use this for something?
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
    Tile(TileID ground, TileID mid);
    Tile(TileID ground, TileID mid, f32 zPos);
    Tile(TileID ground, TileID mid, f32 zPos, TileFlags flags);

    bool hasFlagMainThread(TileFlags flag) const { return tileFlags.isBitSet(flag); }
    bool hasFlagsMaskAnyMainThread(TileFlagType mask) const { return tileFlags.isMaskPartiallySet(mask); }

    bool hasHarvestableResource(TileResource resource, TileLayer* outLayer) const;

    // Only nav thread can access this data
    ui16 getNavNodeIndex_DEBUG_MAIN_THREAD() const { return navData.coarseNavNodeIndex; }
    ui16 getNavNodeIndex() const { assert(IS_NAV_THREAD()); return navData.coarseNavNodeIndex; }
    void setNavNodeIndex(ui16 index) const { assert(IS_NAV_THREAD()); navData.coarseNavNodeIndex = index; }
    bool canNavInDirection(Cartesian8 dir) const;
    f32 getEdgeHeightOffset(Cartesian dir) const;

    f32 getGroundZOffset() const { return groundZOffset; }

	const TileID* getLayers() const { return layers; }

    Cartesian getOrientation(TileLayer layer) const;

    bool isEmpty() const { return layers[TILE_LAYER_GROUND] == TILE_ID_NONE && layers[TILE_LAYER_MAIN] == TILE_ID_NONE; }

private:
    // Mutators are accessed only via chunk generator or chunk methods (friend classes)
    bool canAddTileData(const TileData& tile) const;
    void addTileData(const TileData& tile);
    void setTileLayer(TileLayer layer, TileID id);
    void setTileFlag(TileFlags flag);
    void setTileFlags(TileFlags flags);
    void setOrientation(Cartesian dir, TileLayer layer);
    void clearTileFlag(TileFlags flag);
    void clearTileFlags();
    void setGroundZPosition(f32 groundZPosition);

    // ================================= Data =================================
    union { // These can safely be modified at any time and will only be accessed by the main thread
        struct {
            TileID groundLayer; // floors, foundation     // ALWAYS BOX COLLISION
            TileID mainLayer;
        };
        TileID layers[TILE_LAYER_COUNT] = { TILE_ID_NONE, TILE_ID_NONE };
    };
    TileNavData navData; // TODO: Get tf out of tile data
    BitFlags<TileFlags> tileFlags;
    TileOrientation orientation = {}; // TODO: Combine these?
    f32 groundZOffset;
};
// TODO: Could we limit tile counts by category? Ground tile ID would be 8? mid tile ID also 8, only top layer has ui16?
static_assert(sizeof(Tile) == 32, "Keep small");
//SIZER(Tile);