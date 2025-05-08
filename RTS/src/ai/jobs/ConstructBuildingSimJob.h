#pragma once

#include "world/simulation/ISimJob.h"
#include "building/BuildingBlueprint.h"
#include "tile/TileHandle.h"

#include "tile/SimTileReservation.h"
#include "BuildContextTargetData.h"

#include "item/SimChunkTileItemReservation.h"

class SimECS;
class ISimTask;
class Building;
class ConstructBuildingSimTask;

// TODO:
enum class ItemAquisitionSourceType : ui8 {
	ChunkTile,
	ItemOnGround,
	StockpileOwned,
	StockpilePurchase,
	COUNT
};
//struct ItemAquisitionSource {
//	LiteTileHandle tileHandle;
//	i32v2 worldPos2D;
//	i16 estimatedQuantity;
//	ItemAquisitionSourceType type;
//};

class ConstructBuildingContext {
public:
	ConstructBuildingContext(Building& building, World& world);

	void init();

	// Tasks are grabbed and returned from here to guarentee no two actors are modifying the same target
	std::optional<BuildContextTargetData> tryAquireTargetForItem(ItemID itemId);
	void returnTargetForItem(ItemID itemId, BuildContextTargetData target);
	std::optional<BuildContextTargetData> tryAquireTargetToConstruct();
	void returnTargetToConstruct(BuildContextTargetData target) {
        assert(target.isValid());
		mTilesToConstruct.push(target);
	}

	bool shouldFlattenTile(TileIndex i) const;
	void markFlattened(TileIndex i);

	// Items
	void trackItemIfNeeded(TileItemUID itemUID, ItemID itemId, TileCoord worldPos, ui16 quantity);
	SimChunkTileItemReservationPtr tryGetClosestItemToPickup(i16 maxCount, TileCoord pos, i32 maxDistance = 46340);

    struct ReservedItems {
        std::vector<SimChunkTileItemReservationPtr> reservations;
        std::vector<TileCoord> positions; // For fast distance checks
    };

	f32v2 getClosestValidInteractPosition(f32v2 pos) const;

public:
    BuildingBlueprint& blueprint;
	World& world;
    Building& building;
private:
	// Reversed vectors for efficient pop_back
	FlatMap<ItemID, std::vector<BuildContextTargetData>> mItemsToTileTargets;
	FlatMap<ItemID, ReservedItems> mReservedItems;
	std::queue<BuildContextTargetData> mTilesToConstruct;
    BitArray mTilesNeedingFlatten;
	std::vector<f32v2> mInteractPositions;
	mutable std::mutex mMutex;
};

// Step 1: Acquire items for job and fill blueprint + flatten terrain
// Step 2: Build each tile that has all items
class ConstructBuildingSimJob : public ISimJob {
	friend class ConstructBuildingSimTask;
public:
	ConstructBuildingSimJob(World& world, Building& building, entt::entity simJobOwner);
	~ConstructBuildingSimJob() = default;

	POOLED_ALLOC_DECL(ConstructBuildingSimJob);

	std::unique_ptr<ISimTask> tryAquireNextSubtaskForSimCharacter(entt::registry& simRegistry, entt::entity simCharacter) override;
	std::unique_ptr<ISimTask> tryAquireNextSubtaskForFullCharacter(entt::registry& fullRegistry, entt::entity fullCharacter) override;

	void onAbortTask(ISimTask& task) override;
    void onCompleteTask(ISimTask& task) override;

    const char* getName() const override { return "Construct Building"; }

private:
	bool isFinished();

	ConstructBuildingContext mContext;
};
