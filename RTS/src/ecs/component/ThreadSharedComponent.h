#pragma once

#include "item/ItemStack.h"

class World;

enum class RenderThreadSharedComponentType {
    ItemSack,
    COUNT
};

struct RenderThreadSharedComponentItemSackResource {
    std::vector<ItemStackWithUID> itemStacks;
};

struct RenderThreadSharedComponentData {
    RenderThreadSharedComponentData(RenderThreadSharedComponentType type, EntityUid uid) : type(type), uid(uid) {}

    RenderThreadSharedComponentItemSackResource& getItemSackData() {
        assert(resource);
        assert(type == RenderThreadSharedComponentType::ItemSack);
        return *static_cast<RenderThreadSharedComponentItemSackResource*>(resource.get());
    }

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