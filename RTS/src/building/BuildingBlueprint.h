#pragma once

#include "item/ItemStack.h"
#include "tile/Stairs.h"
#include "util/BitArray.h"

class BuildingDef;

struct BuildingBlueprintTileTarget {
    TileIndex tileIndex;
    TileID id;
    //ui8 runLength // TODO: RLE Compression
};

struct BuildingBlueprintWallTarget {
    TileIndex tileIndex;
    TileID id;
    Cartesian dir;
    bool isDoor;
    // ui8 runLength // TODO: RLE Compression
};

struct BuildingBlueprintRoomNode {
    TileIndex rootPos; // Guarenteed valid tile
    ui16 edgeStartIndex; // Start in edges allEdges
    ui8 numEdges;
    //ui8 padding? ;
};

class BuildingBlueprintRoomGraph {
    std::unique_ptr<BuildingBlueprintRoomNode[]> allNodes;
    std::unique_ptr<ui16[]> allEdges; // Represents 1 way edge
    ui32 nodeCount;
    ui32 edgeCount;
};

// Minimal data representation of a building
class BuildingBlueprint {
public:

    BitArray computeSolidTilesFirstFloor() const;
    bool isFinished() const { return desc != nullptr; }

    BuildingBlueprintRoomGraph roomGraph; // TODO: Build this?
    BitArray ownedDTiles;
    // Sorted by build priority front to back, so first floor tiles at the start
    std::unique_ptr<BuildingBlueprintTileTarget[]> tileTargets;
    // Sorted by build priority front to back, so first floor walls at the start
    std::unique_ptr<BuildingBlueprintWallTarget[]> wallTargets;
    // Required items to build
    std::unique_ptr<ItemStack[]> itemComposition; // TODO: Maybe this should be flexible... maybe we dont care what items are used? Room specific tiles? ect.
    // Each stair tile
    std::unique_ptr<StairPiece[]> stairPieces;
    const BuildingDef* desc = nullptr;
    i32 itemCompositionCount;
    i32 tileTargetCount;
    i32 constructedTileTargetCount = 0;
    i32 wallTargetCount;
    i32 constructedWallTargetCount = 0;
    i32 stairPieceCount;
    i32 constructedStairPieceCount = 0;
    DTileCoord worldPosRootDTile;
    i32v2 dimsDTile;
    i32 floorHeight;
    i32 floorCount;
    TileID stairsTileID;
    TileID stairsFlatTileID;
    TileID defaultFloorID;
    /*

    bool tileIsOwned(DTileIndex tileIndex) const {
        return ownedDTiles.getBit(tileIndex); WRONG
    }*/

};

typedef std::unique_ptr<BuildingBlueprint> BuildingBlueprintPtr;