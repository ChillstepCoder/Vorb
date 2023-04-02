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

// TODO: Cellular automata rule iteration for room fixup
typedef ui32 BuildingBlueprintId;
#define INVALID_BLUEPRINT_ID UINT32_MAX

enum class BuildingBlueprintFlags : ui8 {
    BLUEPRINT_FLAG_CREATE_EARLY_STOCKPILE = BIT(0),
    BLUEPRINT_FLAG_FAILED_TO_GENERATE = BIT(1)
};

struct BlueprintTileItemData {
    ItemID mItemId;
    ui16 mMissingQuantity;
    ui16 mCurrentQuantity = 0;
    ui16 mPromisedQuantity = 0;
};
struct BlueprintTileItemDataHandle {
    ui32 mItemDataOffset = 0;
    ui16 mItemDataCountRequired = 0;
    ui16 mItemDataCountFinished = 0;
};
struct BlueprintTileBuildData {
    float mProgress = 0.0f; // 0-1
    entt::entity mReservedBy = INVALID_ENTITY;
};

// TODO: Pool allocate
class PlaceTileBlueprintItemsHandle {
public:
    PlaceTileBlueprintItemsHandle() = default;
    ~PlaceTileBlueprintItemsHandle();

    bool isValid() const { return mBlueprint != nullptr; }
    void fulfillFromItemStack(ItemStack& stack);

    BuildingBlueprint* mBlueprint = nullptr;
    TileIndex mTileIndex;
    ItemID mItemId;
    ui16 mPromisedItemCount = 0;
};
typedef std::unique_ptr<PlaceTileBlueprintItemsHandle> PlaceTileBlueprintItemsHandlePtr;

// TODO: Pool allocate
class BuildTileBlueprintHandle {
public:
    BuildTileBlueprintHandle() = default;
    ~BuildTileBlueprintHandle();

    // Return true when done
    bool tick(f32 buildProgressIncrease);

    bool isValid() const { return mBlueprint != nullptr; }

    BuildingBlueprint* mBlueprint = nullptr;
    TileIndex mTileIndex;
};
typedef std::unique_ptr<BuildTileBlueprintHandle> BuildTileBlueprintHandlePtr;

class BuildingBlueprint {
public:
    BuildingBlueprint() = default;
    BuildingBlueprint(const BuildingDef& desc, float sizeAlpha, Cartesian entrySide, ui32v2 dims, ui32v2 bottomLeftWorldPos, entt::entity ownerEntity, BuildingBlueprintFlags flags);

    VORB_NON_COPYABLE_BUT_MOVABLE(BuildingBlueprint);

    TileHandle getTileHandle(TileIndex tileIndex) const {
        assert(IS_GAME_THREAD());
        return TileHandle(building->getTileContainer(), tileIndex);
    }

    f32v3 getTileWorldPos(TileIndex i) const {
        const i32 layerSize = aabb.dims.x * aabb.dims.y;
        const f32v3 worldRoot(aabb.pos.x, aabb.pos.y, zPos);
        return f32v3(worldRoot.x + (i % aabb.dims.x), worldRoot.y + ((i % layerSize) / aabb.dims.x), worldRoot.z + (i / layerSize) * floorHeight);
    }

    // For construction
    PlaceTileBlueprintItemsHandlePtr reserveTileToPlaceItems(ItemID itemId, ui16 maxItemCount);
    void cancelReserveTileToPlaceItems(PlaceTileBlueprintItemsHandle& handle);
    BuildTileBlueprintHandlePtr reserveTileToBuild(entt::entity builderEntity, const f32v3& entityPosition);
    void endTileToBuild(BuildTileBlueprintHandle& handle);

    std::map<ItemID, std::deque<TileIndex>> tilesNeedingItems; // Pull from back first
    std::vector<TileIndex> tilesReadyToBuild;
    std::vector<BlueprintTileItemData> tileItemData;
    std::vector<ItemStackUnbounded> requiredItemsToBuild;
    std::vector<BlueprintTileItemDataHandle> tileItemDataHandles; // Constant size
    std::vector<BlueprintTileBuildData> tileBuildData; // Constant size
    f32 mDesiredTerrainFlattenHeight = 0.0f;
    // End construction

    Building* building = nullptr;
    const BuildingDef* desc = nullptr;
    float sizeAlpha;
    Cartesian entrySide = Cartesian::WEST;
    i32AABB2 aabb;
    ui32 floorCount = 1u;
    CityPlotIndex plotIndex = INVALID_PLOT_INDEX;

    BitArray tilesNeedingTerrainFlatten;
    std::vector<RoomNode> rooms;
    std::vector<RoomNodeID> ownerArray;
    std::vector<BlueprintTileType> tiles;
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
    ui32 refCount = 0;
};