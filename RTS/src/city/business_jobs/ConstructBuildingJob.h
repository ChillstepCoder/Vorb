#pragma once

#include "IBusinessJob.h"
#include "item/ItemStack.h"

#include "item/ItemReservation.h"

struct BuildingBlueprint;
struct BusinessComponent;
struct OwnershipComponent;
class ItemReservation;
class BuildTask;

enum class ConstructTileState : ui8 {
	WAITING_RESOURCE_GROUND,
	WAITING_CONSTRUCT_GROUND,
	WAITING_RESOURCE_MID,
	WAITING_CONSTRUCT_MID,
	WAITING_RESOURCE_TOP,
	WAITING_CONSTRUCT_TOP,
    WAITING_CONSTRUCT_ROOF,
    DONE,
};

struct TilesToConstruct {
	ConstructTileState state : 7;
	bool isReserved : 1;
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
    std::vector<std::unique_ptr<ItemReservation>> mReservations;
};

class ConstructBuildingJob : public IBusinessJob
{
public:
	ConstructBuildingJob(BuildingBlueprint& blueprint);
	~ConstructBuildingJob();

	bool tick(World& world, entt::registry& registry, entt::entity business) override;

	float getProgress() const override;

	IAgentTaskPtr tryMakeTaskForWorker(entt::entity worker) override;

private:
	void tryReserveItems(JobRequiredItems& item, OwnershipComponent& ownerCmp);

	BuildingBlueprint& mBlueprint;
	std::vector<TilesToConstruct> mTilesToConstruct;
    std::vector<JobRequiredItems> mRequiredItems;
	ui32 mTotalResourcesReserved = 0;
	
	ConstructBuildingState mState = ConstructBuildingState::NONE;

	ui32 mNumGroundTilesToConstruct = 0;
	ui32 mNumMidTilesToConstruct = 0;
	ui32 mNumTopTilesToConstruct = 0;
	ui32 mNumTilesReservedInTasks = 0;
	ui32 mFirstUnfinishedBpIndex = 0;
	ui32 tickCounter = 0;
};

