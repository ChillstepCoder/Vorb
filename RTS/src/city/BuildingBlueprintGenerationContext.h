#pragma once
#include "CityConst.h"
#include "BuildingBlueprintFlags.h"
#include "tile/TileSpatialGrid.h"
#include "tile/TileWallContainer.h"
#include "city/RoomNode.h"
#include "item/ItemStack.h"

class Building;
class BuildingBlueprintGenerationContext;
class World;
class BuildingDef;
struct Recipe;
struct TileHandle;
class RandomGenerator;

enum class BlueprintTileType : ui8 {
    NONE    = 0, // THIS SHOULD ALWAYS BE 0
    FLOOR   = 1, // THIS SHOULD ALWAYS BE 1
    DOOR,
    WALL,
    WINDOW,
    STAIRS,
    STAIRS_FLAT,
    AIR,
    TYPES
};
static_assert(int(BlueprintTileType::TYPES) < (1 << 6)); // TODO: Why did we have 1 << 6 here?

// TODO: Cellular automata rule iteration for room fixup

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

struct ExteriorWallRun {
    TileIndex start;
    ui32 length;
    Cartesian dir;
};

// TODO: Pool allocate
class PlaceTileBlueprintItemsHandle {
public:
    PlaceTileBlueprintItemsHandle() = default;
    ~PlaceTileBlueprintItemsHandle();

    bool isValid() const { return mBlueprint != nullptr; }
    void fulfillFromItemStack(ItemStack& stack);

    BuildingBlueprintGenerationContext* mBlueprint = nullptr;
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

    BuildingBlueprintGenerationContext* mBlueprint = nullptr;
    TileIndex mTileIndex;
};
typedef std::unique_ptr<BuildTileBlueprintHandle> BuildTileBlueprintHandlePtr;

class BuildingBlueprintGenerationContext {
public:
    BuildingBlueprintGenerationContext() = default;
    BuildingBlueprintGenerationContext(const BuildingDef& desc, float sizeAlpha, Cartesian entrySide, i32v2 dimsDTile, DTileCoord worldPosRoot, BuildingBlueprintFlags flags);
    ~BuildingBlueprintGenerationContext();

    VORB_NON_COPYABLE_BUT_MOVABLE(BuildingBlueprintGenerationContext);

    bool isLocalTileIndexOwned(TileIndex index) const;
    bool isLocalTileIndexOwned(ui32v2 tileXY) const;

    // Indexing
    TileSpatialGrid mTileSpatialGrid;

    // TODO: Use in another context
    // For construction
    //PlaceTileBlueprintItemsHandlePtr reserveTileToPlaceItems(ItemID itemId, ui16 maxItemCount);
    //void cancelReserveTileToPlaceItems(PlaceTileBlueprintItemsHandle& handle);
    //BuildTileBlueprintHandlePtr reserveTileToBuild(entt::entity builderEntity, const f32v3& entityPosition);
    //void endTileToBuild(BuildTileBlueprintHandle& handle);

    //std::map<ItemID, std::deque<TileIndex>> tilesNeedingItems; // Pull from back first
    //std::vector<TileIndex> tilesReadyToBuild;
    //std::vector<BlueprintTileItemData> tileItemData;
    std::vector<ItemStack> requiredItemsToBuild;
    //std::vector<BlueprintTileItemDataHandle> tileItemDataHandles; // Constant size
    //std::vector<BlueprintTileBuildData> tileBuildData; // Constant size
    // End construction

    const BuildingDef* desc = nullptr;
    float sizeAlpha;
    Cartesian entrySide = Cartesian::WEST;
    ui32 floorCount = 1u;

    DTileCoord rootPosDTileCoord;
    i32v2 dimsDTile;
    ui32 floorStrideDTile;
    
    BitArray ownedDTiles;
    BitArray solidTilesFirstFloor;
    std::vector<RoomNode> rooms;
    std::vector<RoomNodeID> ownerArray;
    std::vector<BlueprintTileType> tiles;
    TileWallContainer walls;
    std::vector<std::vector<StairPiece>> stairs;
    std::map<TileIndex, RoomNodeID> exteriorDoors;
    std::vector<ExteriorWallRun> exteriorWallRuns;
    const Recipe* tileRecipes[e_cast(BlueprintTileType::TYPES)] = {};
    TileID tileIDs[e_cast(BlueprintTileType::TYPES)];

    ui32 totalTiles = 0;
    ui32 totalWalls = 0;
    BitFlags<BuildingBlueprintFlags> flags;

    std::unique_ptr<RandomGenerator> randomGen;
    ui32 generationSeed = 0;
};
//SIZER(BuildingBlueprint);
