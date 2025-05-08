#pragma once

#include "world/simulation/host/SimEntityType.h"
#include "item/ItemStack.h"

#include "world/simulation/host/component/SimCharacterComponents.h"

#include "ecs/component/DualComponents.h"

// Shared components between the Simulation Thread ECS and the Render Thread ECS
#include "ecs/component/DualInventoryComponent.h"
#include "ecs/component/DualAttributesComponent.h"


using DualEntityOperationFunc = std::function<void(entt::registry&, entt::entity, bool/*isGameThread*/)>;

namespace SimEntityOperations {
    // Will perform an operation on either sim or game thread, depending on entity
    // simulation state. Call from sim thread
    void simPerformDual(entt::registry& simRegistry, entt::entity entity, DualEntityOperationFunc func);
};

// Exists when we have a full entity spawned for this entity
// Allows communication between the sim -> full entity, one way
class SimFullEntityBinding {
public:

    void processGameThread(entt::registry& registry, entt::entity entity);
    // Only call when we are returning to sim control
    void processSimThreadPreRemove(entt::registry& registry, entt::entity entity);

    void simAddOperation(DualEntityOperationFunc func);

    ui16 getRefCount() const { return refCount; }
    void incRefCount() { ++refCount; }
    void decRefCount() { --refCount; }

    // We do not store the full entity here as
    // it is not needed
    entt::entity simEntity = entt::null;
private:
    std::atomic_bool hasQueuedOperations = false;
    std::atomic_uint16_t refCount = 0;
    // Data passing
    std::mutex mutex;
    std::queue<DualEntityOperationFunc> queuedOperations;

};

// For passing character components between registries
struct EntityComponentCharacterTransitionData {

    POOLED_ALLOC_DECL();

    // Move components from the entity into this object
    void moveFromEntity(entt::registry& registry, entt::entity entity);
    // Move components from the object into entity and initialize
    void moveToEntity(World& world, entt::registry& registry, entt::entity entity, bool isFull);

    DualCharacterComponent character;
    DualTaskQueueComponent taskQueue;
    DualAttributesComponent attributes;
    DualInventoryComponent inventory;
    DualGenderComponent gender;
    DualResidentComponent resident;
    std::optional<DualResourceBundleComponent> resourceBundle = std::nullopt;
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
    FlatMap<ItemID, std::vector<TileItemStack>> itemStacks;
};

struct ChunkSimTransitionData {
    ChunkSimTransitionData() = default;
    ~ChunkSimTransitionData() = default;
    VORB_NON_COPYABLE_BUT_MOVABLE(ChunkSimTransitionData);

    std::vector<EntitySimTransitionData> entities;
};
