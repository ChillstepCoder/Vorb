#pragma once

#include <boost/container/flat_map.hpp>
#include "world/simulation/SimTask.h"

// Shared components between the Simulation Thread ECS and the Render Thread ECS
#include "ecs/component/InventoryComponent.h"
#include "ecs/component/PersonalityComponent.h"
#include "ecs/component/AttributesComponent.h"
#include "world/simulation/host/component/CharacterGroupComponents.h"

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

struct SimCharacterGenderComponent {
    bool isFemale = false;
};

struct SimEmploymentComponent {
    entt::entity employerId; // Can be a person or business entity
};

enum class SimBrainComponentFlags : ui8 {
    IsFollowingCharacterGroup = BIT(0),
    IsCharacterGroupLeader = BIT(1),
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
    TimestampMs taskStepStartTime;
    TimestampMs taskStepEndTime;
    SimTaskHandle currentTask;
    SimTaskPriority taskPriority = SimTaskPriority::Idle;
    ui8 padding[7];
};
static_assert(sizeof(SimInProgressTaskComponent) == 32, "Keep small for cache efficiency");

struct SimNeedsComponent {
    f32 hunger = 0.0f; // [0, 1>, 1 is starving. Can go beyond 1.
    f32 health = 1.0f; // [0, 1], 1 is healthy. 0 is dead.
    f32 fun = 0.0f; // [0, 1], 1 is fully satisfied. 
    bool wantsHome = true; // If nomadic or owns home, will be false
    bool wantsWork = true;
};

// Represents a list of tasks, and who is working on them for us. Does not include tasks we are doing for ourselves
struct SimTaskBossComponent {
    boost::container::flat_map<SimTaskID, SimTaskData> activeTasks;
};

struct SimResidentComponent {
    entt::entity settlementEntity = entt::null;
    BuildingUID homeId = INVALID_BUILDING_UID;
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
