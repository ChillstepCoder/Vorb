#pragma once

#include "city/CityConst.h"
#include "city/Building.h"
#include "crafting/CraftingConst.h"

#include "ai/tasks/IAgentTask.h"

// TODO: This is getting heavyweight, we only include for LiteTileHandle
#include "world/Chunk.h"

class City;
class World;

typedef std::vector<IAgentTaskPtr> TaskList;

// TODO: We are probably leaking IAgentTask here if the agnet is destroyed with active
// tasks, but using the destructor will probably result in us freeing from copies.
// Shared_ptr would work but is heavyweight
struct BusinessComponent {

    // TODO: Trade empires? Multi city?
    City* mCity = nullptr;
    std::vector<BuildingID> mBuildings;
    std::vector<entt::entity> mEmployees; // TODO: Death notify
    ui32 mDesiredEmployeeCount = 1; // TODO: Tiers?
    ui32 mMaxEmployeeCount = 10;

    // TODO: Allow smart task selection based on current agent distance and such
    std::map<ui32, TaskList> mTasksToDo; // Ordered by priority

    CALLER_DELETE IAgentTaskPtr aquireTask();
};

// Gather
struct GatherItemDesc {
    ItemID item;
    f32 weight;
};
struct BusinessGatherComponent {
    ui32 mPriority;
    TileResource mResourceToGather = TileResource::NONE;
    // TODO: Shared search?
    int mFramesUntilNextScan = 0;
    std::vector<LiteTileHandle> mScannedTiles;
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
    BusinessSystem(World& world);

    void update(entt::registry& registry);

    World& mWorld;
    int mFramesUntilUpdate = 0;
};