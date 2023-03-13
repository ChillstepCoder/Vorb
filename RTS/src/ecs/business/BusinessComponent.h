#pragma once

#include "city/CityConst.h"
#include "city/Building.h"
#include "crafting/CraftingConst.h"

#include "ai/tasks/IAgentTask.h"
#include "city/business_jobs/IBusinessJob.h"

#include "tile/TileHandle.h"
#include "tile/TileConst.h"

#include <boost/circular_buffer.hpp>


class City;
class ItemStockpile;
class ConstructBuildingJob;
struct BuildingBlueprint;
struct CityPlot;
struct BusinessDef;

typedef std::unique_ptr<IBusinessJob> IBusinessJobPtr;
typedef boost::circular_buffer<entt::entity> IdleWorkerList;

// TODO: We are probably leaking IAgentTask here if the agent is destroyed with active
// tasks, but using the destructor will probably result in us freeing from copies.
// Shared_ptr would work but is heavyweight
struct BusinessComponent {
    BusinessComponent();

    VORB_NON_COPYABLE_BUT_MOVABLE(BusinessComponent);

    // CALLER_DELETE IAgentTaskPtr aquireTask();
    void addIdleWorker(entt::entity worker);

    // TODO: Trade empires? Multi city?
    City* mCity = nullptr;
    std::vector<entt::entity> mEmployees; // TODO: Death notify
    ui32 mDesiredEmployeeCount = 1; // TODO: Tiers?
    ui32 mMaxEmployeeCount = 10;

    IdleWorkerList mIdleWorkers;
    std::vector<IBusinessJobPtr> mActiveJobs;

    BusinessDef* mBusinessDef = nullptr;
};

// Gather
struct GatherItemDesc {
    ItemID item;
    f32 weight;
};

struct BusinessGatherComponent {
    ui32 mPriority;
    TileHarvestable mResourceToGather = TileHarvestable::NONE;
};

// Construct
struct BusinessBuildComponent {
    ui32 mPriority;
    BuildingBlueprint* mCurrentBlueprint = nullptr;
    ConstructBuildingJob* mCurrentJob = nullptr;
};

// Produce
struct ProduceItemDesc {
    ItemID mItem;
    f32 mWeight;
    CraftingRecipeID mPreferredRecipe;
};
// TODO: Orders
struct BusinessProduceComponent {
    ui32 mPriority;
    std::vector<ProduceItemDesc> mItemsToProduce; // Sorted by weight
};

//Buy/sell
struct BusinessRetailComponent {
    ui32 mPriority;
    bool canBuy = false;
    bool canSell = false;
};

class BusinessSystem {
public:
    BusinessSystem();

    void update(entt::registry& registry);
    void debugRender(entt::registry& registry);

    int mFramesUntilUpdate = 0;
};