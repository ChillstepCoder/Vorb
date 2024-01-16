#pragma once

#include <boost/container/flat_map.hpp>
#include "world/simulation/SimTask.h"

// Shared components between the Simulation Thread ECS and the Render Thread ECS
#include "ecs/component/InventoryComponent.h"
#include "ecs/component/PersonalityComponent.h"

struct SimPositionComponent {
    i32v2 position; // Usually "last known" position in tiles
};

struct SimCharacterComponent {
    CharacterUID characterId;
};

struct SimEmploymentComponent {
    entt::entity employerId; // Can be a person or business entity
};

enum class SimBrainComponentFlags : ui8 {
    IsFollowingLeader = BIT(0),
    IsLeader = BIT(1),
    HasTask = BIT(2),
    InCombat = BIT(3),
    TERM
};
static_assert(e_cast(SimBrainComponentFlags::TERM) <= 0xff);

// Extemely simple decision maker as we will be running tens of thousands of these on the
// sim thread
struct SimBrainComponent {
    BitFlags<SimBrainComponentFlags> flags;
};
static_assert(sizeof(SimBrainComponent) == 1, "Keep small for cache efficiency");

struct SimInProgressTaskComponent {
    SimTimestamp taskStepStartTime;
    SimTimestamp taskStepEndTime;
    SimTaskHandle currentTask;
    SimTaskPriority taskPriority = SimTaskPriority::Idle;
    ui8 padding[3];
};
static_assert(sizeof(SimInProgressTaskComponent) == 20, "Keep small for cache efficiency");

struct SimNeedsComponent {
    f32 hunger = 0.0f; // [0, 1>, 1 is starving. Can go beyond 1.
    f32 health = 1.0f; // [0, 1], 1 is healthy. 0 is dead.
    f32 fun = 0.0f; // [0, 1], 1 is fully satisfied. 
    bool wantsHome = true; // If nomadic or owns home, will be false
    bool wantsWork = true;
};

// Leads a group of characters around
struct SimGroupLeaderComponent {
    std::vector<entt::entity> groupMembers;
};

struct SimGroupFollowerComponent {
    entt::entity leader;
    SimTimestamp nextFollowCheckTime; // When to check if we should keep following
};

// Represents a list of tasks, and who is working on them for us. Does not include tasks we are doing for ourselves
struct SimTaskBossComponent {
    boost::container::flat_map<SimTaskID, SimTaskData> activeTasks;
};

struct SimResidentComponent {
    SettlementUID settlementId;
    BuildingUID homeId;
};

struct SimDescriptionComponent {
    // TODO: Allocate in one block with string_views?
    nString name;
    nString desc;
};