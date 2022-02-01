#pragma once

#include "IBusinessJob.h"
#include "item/ItemStack.h"

struct BuildingBlueprint;
struct BusinessComponent;
class ItemReservation;

enum class ConstructTileState {
	WAITING_RESOURCE_GROUND,
	WAITING_CONSTRUCT_GROUND,
	WAITING_RESOURCE_MID,
	WAITING_CONSTRUCT_MID,
	WAITING_RESOURCE_TOP,
	WAITING_CONSTRUCT_TOP,
    WAITING_CONSTRUCT_ROOF,
    DONE,
};

enum class ConstructBuildingState {
	NONE,
	LEVEL_TERRAIN,
	BUILD_GROUND,
	BUILD_MID,
	BUILD_TOP
};

struct JobRequiredItems {
    ItemID id = INVALID_ITEM_ID;
    ui32 quantityRequired = 0;
	ui32 quantityReserved = 0;
};

class ConstructBuildingJob : public IBusinessJob
{
public:
	ConstructBuildingJob(BuildingBlueprint* blueprint);
	~ConstructBuildingJob();

	bool tick(World& world, entt::registry& registry, entt::entity business) override;

    void assignWorker(entt::entity worker) override;

	float getProgress() const override;

private:
	bool tryAssignTaskToWorker(entt::entity worker);
	void tryReserveItems(JobRequiredItems& item, BusinessComponent& businessCmp);

	BuildingBlueprint* mBlueprint;
    std::vector<ConstructTileState> mTileStates;
    std::vector<JobRequiredItems> mRequiredItems;
	std::vector<std::unique_ptr<ItemReservation>> mItemReservations;
	ConstructBuildingState mState = ConstructBuildingState::NONE;

	ui32 mNumGroundTilesToConstruct = 0;
	ui32 mNumMidTilesToConstruct = 0;
	ui32 mNumTopTilesToConstruct = 0;
	ui32 mTotalTilesToConstruct = 0;
};

