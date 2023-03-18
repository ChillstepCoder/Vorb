#pragma once

#include "Building.h"

enum class BlueprintTileType : ui8 {
    NONE    = 0, // THIS SHOULD ALWAYS BE 0
    FLOOR   = 1, // THIS SHOULD ALWAYS BE 1
    DOOR    = 2,
    WALL    = 3,
    STAIRS  = 4,
    STAIRS_FLAT = 5,
    AIR     = 6,
    TYPES   = 7
};
static_assert(int(BlueprintTileType::TYPES) < 1 << 6);

struct BlueprintTile {
    BlueprintTileType type : 6;
    bool isBuilt : 1;
    bool isReserved : 1;
};
static_assert(sizeof(BlueprintTile) == 1, "Keep it small");

// TODO: Cellular automata rule iteration for room fixup
typedef ui32 BuildingBlueprintId;
#define INVALID_BLUEPRINT_ID UINT32_MAX

enum class BuildingBlueprintFlags : ui8 {
    BLUEPRINT_FLAG_CREATE_EARLY_STOCKPILE = 1 << 0
};

struct BlueprintTileItemData {
    ItemID mItemId;
    ui16 mMissingQuantity;
    ui16 mCurrentQuantity = 0;
    ui16 mPromisedQuantity = 0;
};
struct BlueprintTileHandle {
    TileIndex mTileIndex = INVALID_TILE_INDEX;
    ui32 mItemDataOffset;
    ui32 mItemDataCount;
};

struct PlaceTileBlueprintItemsHandle {
    PlaceTileBlueprintItemsHandle() = default;
    ~PlaceTileBlueprintItemsHandle() {
        if (mItemCount) {
            mBlueprint->cancelReserveTileToPlaceItems(*this);
        }
    }
    BlueprintTileHandle mTileHandle;
    ItemID mItemId;
    ui16 mItemCount = 0;
    BuildingBlueprint* mBlueprint = nullptr;

    bool isValid() const { return mTileHandle.mTileIndex != INVALID_TILE_INDEX; }
};
typedef std::unique_ptr<PlaceTileBlueprintItemsHandle> PlaceTileBlueprintItemsHandlePtr;

struct BuildingBlueprint {
    BuildingBlueprint(const BuildingDef& desc, float sizeAlpha, Cartesian entrySide, ui32v2 dims, ui32v2 bottomLeftWorldPos, entt::entity ownerEntity, BuildingBlueprintFlags flags);

    TileHandle getTileHandle(TileIndex tileIndex) const {
        assert(IS_GAME_THREAD());
        return TileHandle(building->getTileContainer(), tileIndex);
    }

    f32v3 getTileWorldPos(TileIndex i) const {
        const i32 layerSize = aabb.dims.x * aabb.dims.y;
        f32v3 worldRoot(aabb.pos.x, aabb.pos.y, zPos);
        return f32v3(worldRoot.x + (i % aabb.dims.x), worldRoot.y + ((i % layerSize) / aabb.dims.x), worldRoot.z + (i / layerSize) * floorHeight);
    }

    PlaceTileBlueprintItemsHandlePtr reserveTileToPlaceItems(ItemID itemId, ui16 maxItemCount);
    void cancelReserveTileToPlaceItems(PlaceTileBlueprintItemsHandle& handle);

    // For construction
    std::map<ItemID, std::vector<BlueprintTileHandle>> tilesNeedingItems;
    std::vector<BlueprintTileHandle> tilesReadyToBuild;
    std::vector<BlueprintTileItemData> tileItemData;
    std::vector<ItemStackUnbounded> requiredItemsToBuild;
    // End construction

    Building* building = nullptr;
    const BuildingDef& desc;
    float sizeAlpha;
    Cartesian entrySide = Cartesian::WEST;
    i32AABB2 aabb;
    ui32 floorCount = 1u;
    CityPlotIndex plotIndex = INVALID_PLOT_INDEX;

    std::vector<RoomNode> rooms;
    std::vector<RoomNodeID> ownerArray;
    std::vector<BlueprintTile> tiles;
    std::vector<TileWalls> walls;
    std::vector<std::vector<StairPiece>> stairs;
    std::map<TileIndex, RoomNodeID> exteriorDoors;
    const Recipe* tileRecipes[e_cast(BlueprintTileType::TYPES)] = {};
    TileID tileIDs[e_cast(BlueprintTileType::TYPES)];

    BuildingBlueprintId id = INVALID_BLUEPRINT_ID;
    ui32 tilesBuilt = 0;
    ui32 totalTilesToBuild = 0;
    entt::entity mOwnerEntity = INVALID_ENTITY;
    bool isGenerating = true;
    bool isBuilding = false;
    BitFlags<BuildingBlueprintFlags> flags;
    f32 zPos = 0.0f;
    ui32 floorHeight = 3;
};