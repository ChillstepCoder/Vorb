#pragma once


#include "definitions/TileDef.h"

// TODO: Too many includes?
#include "TileConst.h"
#include "tile/TileFlags.h"
#include "tile/TileDamageData.h"
#include "item/ItemStack.h"
#include "tile/TileWallContainer.h"
#include "item/Recipe.h"
#include "tile/HarvestableSubChunkRegistry.h"

#include "util/BitArray.h"

struct TileOrientation {
    Cartesian orientationBase : 2;
    Cartesian orientationMain : 2;
    //Cartesian PADDING : 4; // Use this for something?
};
static_assert(sizeof(TileOrientation) == 1);


class Tile {
    friend class TileContainer;
    friend class ChunkGenerator;
    friend class TileContainerLoader;
    friend class FlatChunkGenerator;
    friend class BuildingBuilder; // TODO: Remove? Only for debug?
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
    ui8 getGroundLayerVariant() const { return groundLayerVariant; }
    TileID getMainID() const { return mainLayer; }
    ui8 getMainLayerVariant() const { return mainLayerVariant; }

    Cartesian getOrientation(TileLayer layer) const;

    bool isEmpty() const { return layers[TILE_LAYER_GROUND] == TILE_ID_NONE && layers[TILE_LAYER_MAIN] == TILE_ID_NONE; }
    bool isRoofed() const { return tileFlags.isBitSet(TileFlags::ROOFED); }
    bool isBuildingExterior() const { return tileFlags.isBitSet(TileFlags::IS_BUILDING_EXTERIOR); }

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
    ui8 groundLayerVariant : 4 = {};
    ui8 mainLayerVariant : 4 = {};
    f32 groundZOffset = 0.0f;
};
// TODO: Could we limit tile counts by category? Ground tile ID would be 8? mid tile ID also 8, only top layer has ui16?
static_assert(sizeof(Tile) == 12, "Keep small");
//SIZER(Tile);

// TODO: We have to include tile wall container because of these
// All meshable (and visibility) data from a container, copied to prevent race conditions or mutex locks
struct ContainerMeshDataCopy {
    std::vector<Tile> tiles;
    TileWallContainer walls;
    TileSpatialGrid spatialGrid;
    FlatMap<TileIndex, TileDamageData> damageData;
};

struct ContainerNavDataCopy {
    std::vector<HarvestableSubchunkRegistry> harvestables; // TODO: hmmm....
    std::vector<Tile> tiles;
    TileWallContainer walls;
    TileSpatialGrid spatialGrid;
    BitArray ownedDTiles; // If empty, we own all
};

// TODO: REMOVE
extern f32 getTileModelRotationAtPosition(f32v2 worldPos);