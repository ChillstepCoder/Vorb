#pragma once

#include "world/simulation/ISimJob.h"
#include "building/BuildingBlueprint.h"
#include "tile/TileHandle.h"

#include "tile/SimTileReservation.h"
#include "tile/SimTileReservation.h"

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
struct ItemAquisitionSource {
	LiteTileHandle tileHandle;
	i32v2 worldPos2D;
	i16 estimatedQuantity;
	ItemAquisitionSourceType type;
};


// Step 1: Aquire items for job and fill blueprint + flatten terrain
// Step 2: Build each tile that has all items
class ConstructBuildingSimJob : public ISimJob {
	friend class ConstructBuildingSimTask;
public:
	ConstructBuildingSimJob(Building& building, SimECS& simEcs, entt::entity simJobOwner);
	~ConstructBuildingSimJob() = default;

	POOLED_ALLOC_DECL(ConstructBuildingSimJob);

	std::unique_ptr<ISimTask> tryAquireNextSubtaskForSimCharacter(World& world, entt::registry& simRegistry, entt::entity simCharacter) override;
	std::unique_ptr<ISimTask> tryAquireNextSubaskForFullCharacter(World& world, entt::registry& fullRegistry, entt::entity fullCharacter) override;

	void onAbortTask(ISimTask& task) override;
	void onCompleteTask(ISimTask& task) override;

private:

	// Places where we are attempting to aquire items from
    std::vector<std::map<LiteTileHandle, ItemAquisitionSource>> mItemAquisitions;
    std::vector<SimChunkTileReservationHandle> reservedTiles;

	Building& mBuilding;
	BuildingBlueprint& mBlueprint;
	SimECS& mSimEcs;
};

