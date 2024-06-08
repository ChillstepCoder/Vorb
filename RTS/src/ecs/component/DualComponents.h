#pragma once

enum class SimHomeState : ui8 {
    Homeless,
    Pending,
    NeedsBlueprint,
    Building,
    Done
};

struct DualResidentComponent {
    entt::entity simSettlementEntity = entt::null;
    BuildingID homeId = INVALID_BUILDING_ID;
    i32v2 homePoint = i32v2(-1, -1); // Represents our tent, house, or general wandering area that we should stay near while chilling
    //BitFlags<SimResidentComponentFlags> flags;
    SimHomeState homeState;
};

constexpr i32 MAX_SIM_TASK_QUEUE_SIZE = 8;
struct DualTaskQueueComponent {
    // Provides stable pointers to task handles
    boost::circular_buffer<SimTaskHandle> taskQueue = boost::circular_buffer<SimTaskHandle>(MAX_SIM_TASK_QUEUE_SIZE); // First is the active one
    ISimTask* activeTask = nullptr;
};
static_assert(sizeof(DualTaskQueueComponent) == 48, "Keep small for cache efficiency");

struct DualGenderComponent {
    bool isFemale = false;
};

struct DualCharacterComponent {
    CharacterUID characterId;
};
