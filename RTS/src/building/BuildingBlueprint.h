#pragma once

#include "item/ItemStack.h"
#include "item/Recipe.h"
#include "item/SimpleItemReservation.h"
#include "tile/Stairs.h"
#include "util/BitArray.h"
#include "ai/jobs/BuildContextTargetData.h"

class BuildingDef;

struct BuildingBlueprintTileTarget {
    SimpleItemReservationTargetHandlePtr itemReservation = nullptr;
    FillableRecipe fillableRecipe;
    TileIndex tileIndex;
    TileID id;
    //ui8 runLength // TODO: RLE Compression
};
struct BuildingBlueprintWallTarget {
    SimpleItemReservationTargetHandlePtr itemReservation = nullptr;
    FillableRecipe fillableRecipe;
    TileIndex tileIndex;
    TileID id;
    Cartesian dir;
    bool isDoor : 1 = false;
    // ui8 runLength // TODO: RLE Compression
};

struct StairTileTarget {
    SimpleItemReservationTargetHandlePtr itemReservation = nullptr;
    FillableRecipe fillableRecipe;
    StairPiece piece;
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
    friend class ConstructBuildingSimJob;
    friend class ConstructBuildingSimTask;
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
    TileCoord getCenterPosTile() const {
        assert(dimsDTile.x != -1);
        return TileCoord(worldPosRootDTile + dimsDTile / 2);
    }
    i32 getMaxPromiseSize(ItemID id) const {
        for (i32 i = 0; i < itemCompositionCount; ++i) {
            if (itemComposition[i].itemId == id) {
                return itemComposition[i].getMaxPromiseSize();
            }
        }
        return 0;
    }
    FillableRecipe& getRecipeForTargetData(BuildContextTargetData data) const;

private:

    void onEndItemReservation(ui32 reservationId);

    BuildingBlueprintRoomGraph roomGraph; // TODO: Build this?
    BitArray ownedDTiles;
    // Sorted by build priority front to back, so first floor tiles at the start
    std::unique_ptr<BuildingBlueprintTileTarget[]> tileTargets;
    // Sorted by build priority front to back, so first floor walls at the start
    std::unique_ptr<BuildingBlueprintWallTarget[]> wallTargets;
    // Each stair tile
    std::unique_ptr<StairTileTarget[]> stairTargets;
    // Required items to build. Filled items are not necessarily placed, but are promised by characters
    std::unique_ptr<FillableSimpleItemStack[]> itemComposition; // TODO: Maybe this should be flexible... maybe we dont care what items are used? Room specific tiles? ect.
    const BuildingDef* desc = nullptr;
    i32 itemCompositionCount = 0;
    i32 tileTargetCount = 0;
    i32 wallTargetCount = 0;
    i32 stairTargetCount = 0;
    DTileCoord worldPosRootDTile = DTileCoord(-1);
    DTileCoord dimsDTile = DTileCoord(-1);
    i32 floorHeight;
    i32 floorCount;
    TileID stairsTileID;
    TileID stairsFlatTileID;
    TileID defaultFloorID;
    SettlementPlotID parentPlotID = INVALID_SETTLEMENT_PLOT_ID;
    entt::entity parentSettlement = entt::null;

    // Item tracking
    i32 totalItemsUnfulfilled = 0;
    i32 totalItemsUnpromised = 0;
    i32 totalTargetsUnbuilt = 0;
    ui32 nextItemReservationId = 0;
    // Tracks items that are promised to this blueprint from workers.
    // As workers reserve items, the are filled in itemComposition. Once the items are slotted successfully into
    // tiles, the reservation is updated or completed.
    std::map<ui32, SimpleItemReservationTargetHandlePtr> itemReservationHandles;

    /*

    bool tileIsOwned(DTileIndex tileIndex) const {
        return ownedDTiles.getBit(tileIndex); WRONG
    }*/
    // TODO: Overflow stockpile
};

typedef std::unique_ptr<BuildingBlueprint> BuildingBlueprintPtr;