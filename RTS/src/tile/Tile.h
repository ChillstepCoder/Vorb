#pragma once

#include "TileConst.h"
#include "tile/TileFlags.h"
#include "item/ItemStack.h"
#include "tile/TileWallContainer.h"
#include "item/Recipe.h"
#include "physics/CollisionShapes.h"
#include "tile/HarvestableSubChunkRegistry.h"

// TODO: Do we need rendering here?
#include "rendering/material/MaterialData.h"
#include "util/BitArray.h"

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
    WINDOW,
    DOOR,
    STAIRS,
    MODEL,
    // Custom TODO
    COUNT,
    NONE = COUNT
};
SERIALIZABLE_ENUM_SAME_NAME(TileShape,
    pair{ TileShape::THIN, "thin"sv},
    pair{ TileShape::BLOCK, "block"sv},
    pair{ TileShape::FLOOR, "floor"sv},
    pair{ TileShape::WALL, "wall"sv },
    pair{ TileShape::WINDOW, "window"sv},
    pair{ TileShape::DOOR, "door"sv },
    pair{ TileShape::STAIRS, "stairs"sv },
    pair{ TileShape::MODEL, "model"sv }
);

struct ItemInputDef {
    StrToken itemName;
    ui32 count;
};
SERIALIZABLE_SIMPLE(ItemInputDef,
       make_field(o.itemName, "item"sv),
       make_field(o.count, "count"sv)
);

struct ItemDrop {
    StrToken itemName;
    ui32v2 countRange;
    ItemID id;
};
SERIALIZABLE_SIMPLE(ItemDrop,
    make_field(o.itemName, "item"sv),
    make_field(o.countRange, "count"sv)
);


enum class TileTextureMethod : ui8 {
    SIMPLE,
    CONNECTED,
    CONNECTED_WALL,
    VERTICAL,
    FLORA,
    WORLD_TILING,
    COUNT
};
SERIALIZABLE_ENUM_SAME_NAME(TileTextureMethod,
    pair{ TileTextureMethod::SIMPLE, "simple"sv },
    pair{ TileTextureMethod::CONNECTED, "connected"sv },
    pair{ TileTextureMethod::CONNECTED_WALL, "connected_wall"sv },
    pair{ TileTextureMethod::VERTICAL, "vertical"sv },
    pair{ TileTextureMethod::FLORA, "flora"sv },
    pair{ TileTextureMethod::WORLD_TILING, "world_tiling"sv }
);

enum class NavBlockerType : ui8 {
    NONE,
    MEDIUM,
    LARGE,
    COUNT
};
enum class VisibilityBlockerType : ui8 {
    NONE,
    MEDIUM,
    LARGE,
    COUNT
};

constexpr int MAX_TILE_MATERIAL_SLOTS = 8;


// TODO: separate certain data into multiple arrays because right now every TileData lookup is a cache miss
// For example we only look up path weight when constructing the nav  graph, why not  have it in a separate vector?
// Same with materialData. Does it really need to be here?
// This def is accessed quite commonly
class TileDef : public IAsset {
public:
    DEFAULT_ASSET_CONSTRUCTOR(TileDef, AssetType::Tile);

    f32v3 dims = f32v3(1.0f);
   // TileCollider collider;
    //ui8v2 tileDims = ui8v2(1); // 4x4 is max size
    CollisionShapes collisionShapeType = CollisionShapes::NONE;
    f32v3 collisionHalfExtents = f32v3(0.5f, 0.5f, 1.0f);
    CollisionShapeID collisionShapeID = INVALID_COLLISION_SHAPE_ID;
    TileHarvestable harvestable = TileHarvestable::NONE;
    // TODO: Could be a giant array of material slots and these defs only store pointers and lengths.
    std::vector<StrToken> materialNames;
    std::vector<MaterialDesc> materialData;
    TileTextureMethod textureMethod;
    StrToken destroyEffect;
    StrToken modelName;
    ModelID modelId = INVALID_MODEL_ID;
    ui16 maxHealth = 100;
    ui8 layer = e_cast(TileLayer::Main);
    TileShape shape = TileShape::BLOCK;
    ui8 pathWeight = 255;
    ui8 navMask = 0xff; // Access bits mapped to Cartesian8 based on default (SOUTH) orientation
    NavBlockerType navBlockerType = NavBlockerType::NONE;
    VisibilityBlockerType visBlockerType = VisibilityBlockerType::NONE;
    bool blocksVisibility = false;
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
    std::vector<ItemInputDef> recipeData;
};
SERIALIZABLE_SIMPLE(TileDef,
    make_field(o.dims, "dims"),
    make_field(o.collisionShapeType, "col_shape"),
    make_field(o.collisionHalfExtents, "col_half_dims"),
    make_field(o.harvestable, "harvestable"),
    make_field(o.materialNames, "materials"),
    make_field(o.textureMethod, "texture_method"),
    make_field(o.destroyEffect, "destroy_effect"),
    make_field(o.modelName, "model"),
    make_field(o.maxHealth, "max_health"),
    make_field(o.layer, "layer"),
    make_field(o.shape, "shape"),
    make_field(o.pathWeight, "path_weight"),
    make_field(o.navMask, "nav_mask"),
    make_field(o.blocksVisibility, "block_vis"),
    make_field(o.name, "name"),
    make_field(o.itemDrops, "item_drops"),
    make_field(o.recipeData, "recipe")
);

struct TileOrientation {
    Cartesian orientationBase : 2;
    Cartesian orientationMain : 2;
    //Cartesian PADDING : 4; // Use this for something?
};
static_assert(sizeof(TileOrientation) == 1);


class Tile {
    friend class TileContainer;
    friend class ChunkGenerator;
    friend class FlatChunkGenerator;
    friend class CityBuilder; // TODO: Remove? Only for debug?
public:
	Tile() {};
    Tile(TileID ground, TileID mid);
    Tile(TileID ground, TileID mid, f32 zPos);
    Tile(TileID ground, TileID mid, f32 zPos, TileFlags flags);

    bool hasFlag(TileFlags flag) const { return tileFlags.isBitSet(flag); }
    bool hasFlagsMaskAny(TileFlagType mask) const { return tileFlags.isMaskPartiallySet(mask); }
    TileFlagType getFlags() const { return tileFlags.getBits(); }

    bool hasHarvestableResource(TileHarvestable resource, TileLayer* outLayer) const;

    // Only nav thread can access this data TODO: MOVE
    bool canNavInDirection(Cartesian8 dir) const;
    f32 getEdgeHeightOffset(Cartesian dir) const;

    f32 getGroundZOffset() const { return groundZOffset; }

	const TileID* getLayers() const { return layers; }
    TileID getGroundID() const { return groundLayer; }
    TileID getMainID() const { return mainLayer; }

    Cartesian getOrientation(TileLayer layer) const;

    bool isEmpty() const { return layers[TILE_LAYER_GROUND] == TILE_ID_NONE && layers[TILE_LAYER_MAIN] == TILE_ID_NONE; }

private:
    // Mutators are accessed only via chunk generator or chunk methods (friend classes)
    bool canAddTileData(const TileDef& tile) const;
    void setTileFlag(TileFlags flag);
    void overwriteTileFlags(TileFlags flags);
    void setOrientation(Cartesian dir, TileLayer layer);
    void clearTileFlag(TileFlags flag);
    void zeroTileFlags();
    void setGroundZOffset(f32 groundZPosition);

    // ================================= Data =================================
    union { // These can safely be modified at any time and will only be accessed by the main thread
        struct {
            TileID groundLayer; // floors, foundation     // ALWAYS BOX COLLISION
            TileID mainLayer;
        };
        TileID layers[TILE_LAYER_COUNT] = { TILE_ID_NONE, TILE_ID_NONE };
    };
    BitFlags<TileFlags> tileFlags;
    TileOrientation orientation = {}; // TODO: Combine these?
    // ui8 padding; // TODO Use?
    f32 groundZOffset = 0.0f;
};
// TODO: Could we limit tile counts by category? Ground tile ID would be 8? mid tile ID also 8, only top layer has ui16?
static_assert(sizeof(Tile) == 12, "Keep small");
//SIZER(Tile);

// All meshable (and visibility) data from a container, copied to prevent race conditions or mutex locks
struct ContainerMeshDataCopy {
    std::vector<Tile> mTiles;
    TileWallContainer mWalls;
};

struct ContainerNavDataCopy {
    std::vector<HarvestableSubchunkRegistry> mHarvestables;
    std::vector<Tile> mTiles;
    TileWallContainer mWalls;
    BitArray mOwnedTiles;
};