#pragma once

#include "item/ItemStack.h"
#include "tile/Stairs.h"
#include "util/BitArray.h"

class BuildingDef;

struct BuildingBlueprintTileTarget {
    TileIndex tileIndex;
    TileID id;
    bool isReserved : 1 = false;
    bool isBuilt : 1 = false;
    //ui8 runLength // TODO: RLE Compression
};

struct BuildingBlueprintWallTarget {
    TileIndex tileIndex;
    TileID id;
    Cartesian dir;
    bool isDoor : 1 = false;
    bool isReserved : 1 = false;
    bool isBuilt : 1 = false;
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
    friend class BuildingBlueprintGenerator;
    friend class ConstructBlueprintSimJob;
    friend class BuildingBuilder;
    friend class BuildingGrid; // TODO: Remove
    friend class TileContainerLoader;
public:

    BitArray computeSolidTilesFirstFloor() const;
    bool isFinished() const { return desc != nullptr; }
    void assignToSettlement(entt::entity settlementEntity, SettlementPlotID plotID) {
        ASSERT_SIM_THREAD();
        parentSettlement = settlementEntity;
        parentPlotID = plotID;
    }

private:

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
    DTileCoord worldPosRootDTile = DTileCoord(-1);
    i32v2 dimsDTile;
    i32 floorHeight;
    i32 floorCount;
    TileID stairsTileID;
    TileID stairsFlatTileID;
    TileID defaultFloorID;
    SettlementPlotID parentPlotID = INVALID_SETTLEMENT_PLOT_ID;
    entt::entity parentSettlement = entt::null;

    // Build information
    i32 nextTileTargetToReserve = 0;
    i32 nextWallTargetToReserve = 0;
    i32 nextStairPieceToReserve = 0;
    /*

    bool tileIsOwned(DTileIndex tileIndex) const {
        return ownedDTiles.getBit(tileIndex); WRONG
    }*/

};

typedef std::unique_ptr<BuildingBlueprint> BuildingBlueprintPtr;