#pragma once

#include "world/simulation/host/SimEntityType.h"

// Exists when we have a full entity spawned for this entity
// Allows communication between the sim and full entity
struct SimFullEntityBinding {
    std::mutex mutex;
    entt::entity fullEntity = entt::null;
};

struct EntityFullActivateData {
    entt::entity simEntity;
    f32v2 simPosition;
    SimEntityType entityType;
    SimFullEntityBinding* binding = nullptr;
};

typedef std::vector<EntityFullActivateData> ChunkEntityFullActivateDataList;