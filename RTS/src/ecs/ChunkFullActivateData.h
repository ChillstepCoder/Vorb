#pragma once

#include "world/simulation/host/SimEntityType.h"
#include "item/ItemStack.h"

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
    f32v2 simPosition;
    SimEntityType entityType;
    SimFullEntityBinding* binding = nullptr;
};

// Full -> Sim
struct EntityFullDeactivateData {
    entt::entity simEntity;
    f32v2 simPosition;
};

struct ChunkFullActivateData {
    std::vector<EntityFullActivateData> entities;
    std::unordered_map<ItemID, std::vector<TileItemStack>> itemStacks;
};


typedef std::vector<EntityFullDeactivateData> ChunkEntityFullDeactivateDataList;