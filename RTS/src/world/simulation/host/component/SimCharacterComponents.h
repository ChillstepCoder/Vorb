#pragma once

#include "world/simulation/ISimTask.h"

// If this component exists, the entity is on the simulation layer.
// Otherwise it is fully simulated
struct SimPositionComponent {
    friend class SimECS;
    friend class SimAISystem;
    friend class SimEntityTransitionManager;

    SimPositionComponent() = default;
    SimPositionComponent(f32v2 position, ChunkID chunk) : position(position), chunk(chunk) {}

    // Return true if we are in a new chunk
    f32v2 getPosition() const { return position; }
    ChunkID getChunk() const { return chunk; }

private:
    f32v2 position; // Usually "last known" position in tiles
    ChunkID chunk = INVALID_CHUNK_ID;
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
    InCombat = BIT(2),
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
    // Only need X to signify invalid
    void clearTarget() { targetPosition.x = -1.0f; }

    f32v2 targetPosition = f32v2(-1.0f);
};

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
