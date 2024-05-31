#pragma once

#include "world/simulation/ISimJob.h"
#include "building/BuildingBlueprint.h"
#include "tile/TileHandle.h"

#include "tile/SimTileReservation.h"
#include "tile/SimTileReservation.h"
#include "BuildContextTargetData.h"

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
	ConstructBuildingContext(Building& building);

	// Tasks are grabbed and returned from here to guarentee no two actors are modifying the same target
	std::optional<BuildContextTargetData> tryAquireTargetForItem(ItemID itemId);
	void returnTargetForItem(ItemID itemId, BuildContextTargetData target);
	std::optional<BuildContextTargetData> tryAquireTargetToConstruct();
	void returnTargetToConstruct(BuildContextTargetData target) {
        assert(target.isValid());
		tilesToConstruct.push(target);
	}

	bool shouldFlattenTile(TileIndex i) const;
	void markFlattened(TileIndex i);

    Building& building;
    BuildingBlueprint& blueprint;
	// Reversed vectors for efficient pop_back
	std::map<ItemID, std::vector<BuildContextTargetData>> itemsToTileTargets;
	std::queue<BuildContextTargetData> tilesToConstruct;
	i32 firstIncompleteFloor = 0;
	BitArray tilesNeedingFlatten;
};

// Step 1: Acquire items for job and fill blueprint + flatten terrain
// Step 2: Build each tile that has all items
class ConstructBuildingSimJob : public ISimJob {
	friend class ConstructBuildingSimTask;
public:
	ConstructBuildingSimJob(World& world, Building& building, SimECS& simEcs, entt::entity simJobOwner);
	~ConstructBuildingSimJob() = default;

	POOLED_ALLOC_DECL(ConstructBuildingSimJob);

	std::unique_ptr<ISimTask> tryAquireNextSubtaskForSimCharacter(entt::registry& simRegistry, entt::entity simCharacter) override;
	std::unique_ptr<ISimTask> tryAquireNextSubaskForFullCharacter(entt::registry& fullRegistry, entt::entity fullCharacter) override;

	void onAbortTask(ISimTask& task) override;
	void onCompleteTask(ISimTask& task) override;

private:
	void initContext();
	bool isFinished();

	// Places where we are attempting to aquire items from
    //std::vector<std::map<LiteTileHandle, ItemAquisitionSource>> mItemAquisitions;
    std::vector<SimChunkTileReservationHandle> reservedTiles;

	ConstructBuildingContext mContext;
	SimECS& mSimEcs;
};
