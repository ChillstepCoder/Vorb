#pragma once

#include <boost/container/flat_map.hpp>
#include <boost/circular_buffer.hpp>
#include "world/simulation/ISimTask.h"
#include "ai/jobs/SimTaskHandle.h"
#include "ecs/component/SimEntityTypeComponent.h"

// Shared components between the Simulation Thread ECS and the Render Thread ECS
#include "ecs/component/InventoryComponent.h"
#include "ecs/component/PersonalityComponent.h"
#include "ecs/component/AttributesComponent.h"
#include "world/simulation/host/component/CharacterGroupComponents.h"

// If this component exists, the entity is on the simulation layer.
// Otherwise it is fully simulated
struct SimPositionComponent {
    friend class SimECS;
    friend class SimAISystem;
    SimPositionComponent() = default;
    SimPositionComponent(f32v2 position, ChunkID chunk) : position(position), chunk(chunk) {}

    // Return true if we are in a new chunk
    f32v2 getPosition() const { return position; }
    ChunkID getChunk() const { return chunk; }

private:
    f32v2 position; // Usually "last known" position in tiles
    ChunkID chunk = INVALID_CHUNK_ID;
};

struct SimCharacterComponent {
    CharacterUID characterId;
};


struct SimGenderComponent {
    bool isFemale = false;
};

struct SimEmploymentComponent {
    entt::entity employerId; // Can be a person or business entity
};

// Can only have one of these at a time, can spawn related tasks
//enum class SimBrainHighLevelDirective : ui8 {
//    Idle,
//    FindFood,
//    FindShelter,
//    Work,
//    Follow,
//    Sleep,
//};

enum class SimBrainComponentFlags : ui8 {
    IsFollowingCharacterGroup = BIT(0),
    IsCharacterGroupLeader = BIT(1),
    HasTaskOrJob = BIT(2),
    InCombat = BIT(3),
    TERM
};
static_assert(e_cast(SimBrainComponentFlags::TERM) <= 0xff);

// High level simple decision maker
struct SimBrainComponent {
    //SimBrainHighLevelDirective currentDirective = SimBrainHighLevelDirective::Idle;
    //entt::entity fullEntity = entt::null;
    BitFlags<SimBrainComponentFlags> flags;
};
static_assert(sizeof(SimBrainComponent) == 1, "Keep small for cache efficiency");

struct SimMovementComponent {
    f32v2 targetPosition = f32v2(-1.0f);
};

constexpr i32 MAX_SIM_TASK_QUEUE_SIZE = 8;
struct SimTaskQueueComponent {
    // Provides stable pointers to task handles
    boost::circular_buffer<SimTaskHandle> taskQueue = boost::circular_buffer<SimTaskHandle>(MAX_SIM_TASK_QUEUE_SIZE); // First is the active one
    ISimTask* activeTask = nullptr;
};
static_assert(sizeof(SimTaskQueueComponent) == 48, "Keep small for cache efficiency");

struct SimNeedsComponent {
    f32 hunger = 0.0f; // [0, 1>, 1 is starving. Can go beyond 1.
    f32 health = 1.0f; // [0, 1], 1 is healthy. 0 is dead.
    //f32 fun = 0.0f; // [0, 1], 1 is fully satisfied. 
    bool wantsHome = true; // If nomadic or owns home, will be false
    bool wantsWork = true;
};

struct SimFamilyMemberComponent {
    FamilyID familyId = INVALID_FAMILY_ID;
};

// Tracks jobs that need to be done in order of priority
struct SimJobBossComponent {
    std::vector<std::unique_ptr<ISimJob>> activeJobs;
};

enum class SimHomeState : ui8 {
    Homeless,
    Pending,
    NeedsBlueprint,
    Building,
    Done
};

enum class SimProfession : ui8 {
    Unemployed,
    Steward, // Sells property
    Quartermaster, // Allocates goods and receives donations
};

struct SimProfessionComponent {
    BusinessID businessId = INVALID_BUSINESS_ID; // If invalid, employed by settlement
    SimProfession profession = SimProfession::Unemployed;
};

//enum class SimResidentComponentFlags : ui8 {
//};
struct SimResidentComponent {
    entt::entity settlementEntity = entt::null;
    BuildingID homeId = INVALID_BUILDING_ID;
    i32v2 homePoint = i32v2(-1, -1); // Represents our tent, house, or general wandering area that we should stay near while chilling
    //BitFlags<SimResidentComponentFlags> flags;
    SimHomeState homeState;
};

struct SimOwnershipComponent {
    std::unique_ptr<SettlementPlotID[]> ownedPlots;
    i32 numOwnedPlots = 0;
};

struct SimCharacterNameComponent {
    const char* firstName = nullptr;
    const char* lastName = nullptr;
};

struct SimDescriptionComponent {
    nString desc;
};

struct FactionComponent {
    FactionID factionId;
};
