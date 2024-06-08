#pragma once

#include "world/simulation/host/SimEntityType.h"
#include "item/ItemStack.h"

#include "world/simulation/host/component/SimCharacterComponents.h"

using EntityOperationFunc = std::function<void(entt::registry&, entt::entity, bool/*isGameThread*/)>;

namespace EntityOperations {
    // Will perform an operation on either sim or game thread, depending on entity
    // simulation state. Call from sim thread
    void simPerformDual(entt::registry& simRegistry, entt::entity entity, EntityOperationFunc func);
};

// Exists when we have a full entity spawned for this entity
// Allows communication between the sim -> full entity, one way
class SimFullEntityBinding {
public:

    void processGameThread(entt::registry& registry, entt::entity entity);
    // Only call when we are returning to sim control
    void processSimThreadPreRemove(entt::registry& registry, entt::entity entity);

    void simAddOperation(EntityOperationFunc func);
    // We do not store the full entity here as
    // it is not needed, this is simply for the
    // sim entity to push information to the full entity
    entt::entity simEntity = entt::null;
private:
    std::atomic_bool hasQueuedOperations = false;
    // Data passing
    std::mutex mutex;
    std::queue<EntityOperationFunc> queuedOperations;

};

// For passing character components between registries
struct EntityComponentCharacterTransitionData {

    POOLED_ALLOC_DECL();

    void moveFromEntity(entt::registry& registry, entt::entity entity);
    void moveToEntity(World& world, entt::registry& registry, entt::entity entity, bool isFull);

    DualCharacterComponent character;
    DualTaskQueueComponent taskQueue;
    DualAttributesComponent attributes;
    DualInventoryComponent inventory;
    DualGenderComponent gender;
    DualResidentComponent resident;
    bool isValid = false;
};
typedef std::unique_ptr<EntityComponentCharacterTransitionData> EntityComponentCharacterTransitionDataPtr;

// Sim -> Full
struct EntityFullTransitionData {
    EntityFullTransitionData() = default;
    ~EntityFullTransitionData() = default;
    VORB_NON_COPYABLE_BUT_MOVABLE(EntityFullTransitionData);

    void moveFromSimEntity(entt::registry& registry, entt::entity entity, SimFullEntityBinding& entityBinding);

    f32v2 simPosition;
    SimEntityType entityType;
    SimFullEntityBinding* binding = nullptr;
    EntityComponentCharacterTransitionDataPtr characterData;
};

// Full -> Sim
struct EntitySimTransitionData {
    EntitySimTransitionData() = default;
    ~EntitySimTransitionData() = default;
    VORB_NON_COPYABLE_BUT_MOVABLE(EntitySimTransitionData);

    void moveFromFullEntity(entt::registry& registry, entt::entity entity);

    entt::entity simEntity;
    SimEntityType entityType;
    f32v2 simPosition;
    EntityComponentCharacterTransitionDataPtr characterData;
};

struct ChunkFullTransitionData {
    ChunkFullTransitionData() = default;
    ~ChunkFullTransitionData() = default;
    VORB_NON_COPYABLE_BUT_MOVABLE(ChunkFullTransitionData);

    std::vector<EntityFullTransitionData> entities;
    std::unordered_map<ItemID, std::vector<TileItemStack>> itemStacks;
};

struct ChunkSimTransitionData {
    ChunkSimTransitionData() = default;
    ~ChunkSimTransitionData() = default;
    VORB_NON_COPYABLE_BUT_MOVABLE(ChunkSimTransitionData);

    std::vector<EntitySimTransitionData> entities;
};
