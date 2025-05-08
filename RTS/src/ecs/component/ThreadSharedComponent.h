#pragma once

#include "item/ItemStack.h"

class World;

enum class RenderThreadSharedComponentType {
    ItemSack,
    COUNT
};

struct RenderThreadSharedComponentItemSackResource {
    RenderThreadSharedComponentItemSackResource(f32v3 pos) : worldPosition(pos) {}

    std::vector<ItemStackWithUID> itemStacks;
    f32v3 worldPosition;
};

struct RenderThreadSharedComponentData {
    RenderThreadSharedComponentData(RenderThreadSharedComponentType type, EntityUid uid, entt::entity ownerEntity) : type(type), uid(uid), ownerEntity(ownerEntity) {}

    void runFuncOnItemSackResourceThreadSafe(std::function<void(RenderThreadSharedComponentItemSackResource& res)> func) {
        assert(type == RenderThreadSharedComponentType::ItemSack);
        std::lock_guard<std::mutex> lock(resourceMutex);
        func(*static_cast<RenderThreadSharedComponentItemSackResource*>(resource.get()));
    }

    // USER MUST USE resourceMutex
    const RenderThreadSharedComponentItemSackResource& getItemSackData() {
        assert(resource);
        assert(type == RenderThreadSharedComponentType::ItemSack);
        return *static_cast<RenderThreadSharedComponentItemSackResource*>(resource.get());
    }

    void setOwnerEntity(entt::entity entity) {
        ASSERT_GAME_THREAD();
        ownerEntity = entity;
    }
    entt::entity getOwnerEntity() {
        ASSERT_GAME_THREAD();
        return ownerEntity;
    }
private:
    entt::entity ownerEntity; // Game thread access only
public:
    const RenderThreadSharedComponentType type;
    std::atomic_bool wasDestroyed = false;
    const EntityUid uid;
    std::mutex resourceMutex;
    std::shared_ptr<void> resource; // Protected by lock
};
using RenderThreadSharedComponentDataPtr = std::shared_ptr<RenderThreadSharedComponentData>;

// Allows thread safe access to some resource on the entity, such as inventory for UI
class RenderThreadSharedComponent {
public:
    RenderThreadSharedComponentDataPtr mData;
};

class RenderThreadSharedComponentSystem {
public:
    static void update(World& world, entt::registry& registry);
};

class ThreadSharedComponentFactory {
public:
    // Removes any existing item sack UI and adds a new one
    static RenderThreadSharedComponent& addItemSackUISharedComponent(entt::registry& registry, entt::entity entity);
};