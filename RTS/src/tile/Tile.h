#pragma once


#include "definitions/TileDef.h"
#include "tile/TileTypeDataVariant.h"

// TODO: Too many includes?
#include "tile/TileFlags.h"
#include "tile/TileDamageData.h"
#include "tile/TileWallContainer.h"
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
    Tile(TileID id);
    Tile(TileID id, f32 zPos);
    Tile(TileID id, f32 zPos, TileFlags flags);

    bool hasFlag(TileFlags flag) const { return tileFlags.isBitSet(flag); }
    bool hasFlagsMaskAny(TileFlagType mask) const { return tileFlags.isMaskPartiallySet(mask); }
    TileFlagType getFlags() const { return tileFlags.getBits(); }

    bool isHarvestableResource(TileHarvestable resource) const;

    // Only nav thread can access this data TODO: MOVE
    bool canNavInDirection(Cartesian8 dir) const;
    f32 getEdgeHeightOffset(Cartesian dir) const;

    f32 getGroundZOffset() const { return groundZOffset; }

    TileID getMainID() const { return mainLayer; }
    ui8 getMainLayerVariant() const { return variant; }

    Cartesian getOrientation() const;

    bool isEmpty() const { return mainLayer == TILE_ID_NONE; }
    bool isRoofed() const { return tileFlags.isBitSet(TileFlags::ROOFED); }
    bool isBuildingExterior() const { return tileFlags.isBitSet(TileFlags::IS_BUILDING_EXTERIOR); }

    // Ready only, managed by sim thread
    // To modify, make a request of the sim thread
    const TileTypeDataVariant& getTypeData() const { return typeDataCopy; }

private:
    // Mutators are accessed only via chunk generator or chunk methods (friend classes)
    bool canAddTileData(const TileDef& tile) const;
    void setTileFlag(TileFlags flag);
    void overwriteTileFlags(TileFlags flags);
    void setOrientation(Cartesian dir);
    void clearTileFlag(TileFlags flag);
    void zeroTileFlags();
    void setGroundZOffset(f32 groundZPosition);

    // ================================= Data =================================
    TileID mainLayer = TILE_ID_NONE;
    BitFlags<TileFlags> tileFlags;
    f32 groundZOffset = 0.0f;
    // TODO: Some kind of generic "view" class that lets us readonly with updates from sim thread?
    TileTypeDataVariant typeDataCopy; // State owned by sim thread
    Cartesian orientation : 2;
    ui8 variant : 4 = {};
};
static_assert(sizeof(Tile) == 16, "Keep small");

// TODO: We have to include tile wall container because of these
// All meshable (and visibility) data from a container, copied to prevent race conditions or mutex locks
struct ContainerMeshDataCopy {
    std::vector<TileID> floorIds;
    std::vector<Tile> tiles;
    TileWallContainer walls;
    TileSpatialGrid spatialGrid;
    FlatMap<TileIndex, TileDamageData> damageData;
};

struct ContainerNavDataCopy {
    std::vector<HarvestableSubchunkRegistry> harvestables; // TODO: hmmm....
    std::vector<TileID> floorIds;
    std::vector<Tile> tiles;
    TileWallContainer walls;
    TileSpatialGrid spatialGrid;
    BitArray ownedDTiles; // If empty, we own all
};

// TODO: REMOVE
extern f32 getTileModelRotationAtPosition(f32v2 worldPos);