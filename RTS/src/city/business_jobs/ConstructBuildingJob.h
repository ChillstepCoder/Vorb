#pragma once

#include "IBusinessJob.h"
#include "item/ItemStack.h"

#include "item/ItemReservation.h"

struct BuildingBlueprint;
struct BusinessComponent;
struct OwnershipComponent;
class ItemReservation;
class BuildTask;

struct TilesToConstruct {
	bool isReserved = false;
};
static_assert(sizeof(TilesToConstruct) == 1);

enum class ConstructBuildingState {
	NONE,
	LEVEL_TERRAIN,
	BUILD_GROUND,
	BUILD_MID,
	BUILD_TOP
};

struct JobRequiredItems {
	JobRequiredItems();
	~JobRequiredItems();

	VORB_NON_COPYABLE_BUT_MOVABLE(JobRequiredItems);

    ItemID id = INVALID_ITEM_ID;
    ui32 quantityRequired = 0;
    ui32 quantityReserved = 0;
	ui32 quantityGathering = 0;
    std::vector<std::unique_ptr<ItemReservation>> mReservations;
};

class ConstructBuildingJob : public IBusinessJob
{
public:
	ConstructBuildingJob(BuildingBlueprint& blueprint);
	~ConstructBuildingJob();

	bool tick(entt::registry& registry, entt::entity business) override;

	float getProgress() const override;

	IAgentTaskPtr tryMakeTaskForWorker(entt::entity worker) override;

private:
	void tryReserveItems(JobRequiredItems& item, OwnershipComponent& ownerCmp);

	BuildingBlueprint& mBlueprint;
    std::vector<JobRequiredItems> mRequiredItems;
	ui32 mTotalResourcesReserved = 0;
	
	ConstructBuildingState mState = ConstructBuildingState::NONE;

	ui32 mNumTilesReservedInTasks = 0;
	ui32 mFirstUnfinishedBpIndex = 0;
	ui32 tickCounter = 0;
};

