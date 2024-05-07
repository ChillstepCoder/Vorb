#pragma once

#include "world/simulation/host/SimEntityType.h"

// Exists when we have a full entity spawned for this entity
// Allows communication between the sim -> full entity, one way
struct SimFullEntityBinding {
    // We do not store the full entity here as
    // it is not needed, this is simply for the
    // sim entity to push information to the full entity
    entt::entity simEntity = entt::null;
};

// Sim -> Full
struct EntityFullActivateData {
    entt::entity simEntity;
    f32v2 simPosition;
    SimEntityType entityType;
    SimFullEntityBinding* binding = nullptr;
};

// Full -> Sim
struct EntityFullDeactivateData {
    entt::entity simEntity;
    f32v2 simPosition;
};

typedef std::vector<EntityFullActivateData> ChunkEntityFullActivateDataList;
typedef std::vector<EntityFullDeactivateData> ChunkEntityFullDeactivateDataList;