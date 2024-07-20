#include "stdafx.h"
#include "ThreadSharedComponent.h"

#include "ecs/component/TileItemComponent.h"
#include "ecs/component/EntityUidComponent.h"

RenderThreadSharedComponent& ThreadSharedComponentFactory::addItemSackUISharedComponent(entt::registry& registry, entt::entity entity) {
    ASSERT_GAME_THREAD();

    // Close any existing shared item sack UI
    auto view = registry.view<RenderThreadSharedComponent>();
    for (entt::entity e : view) {
        RenderThreadSharedComponent& cmp = view.get<RenderThreadSharedComponent>(e);
        if (cmp.mData->type == RenderThreadSharedComponentType::ItemSack) {
            cmp.mData->wasDestroyed = true;
        }
        registry.remove<RenderThreadSharedComponent>(e);
    }

    const EntityUid uid = registry.get_or_emplace<EntityUidComponent>(entity).mUid;

    RenderThreadSharedComponent& newCmp = registry.emplace<RenderThreadSharedComponent>(entity);
    newCmp.mData = std::make_shared<RenderThreadSharedComponentData>(RenderThreadSharedComponentType::ItemSack, uid);
    newCmp.mData->resource = std::make_shared<RenderThreadSharedComponentItemSackResource>();
    return newCmp;
}

void RenderThreadSharedComponentSystem::update(World& world, entt::registry& registry) {
    ASSERT_GAME_THREAD();

    auto view = registry.view<RenderThreadSharedComponent>();
    for (entt::entity e : view) {
        RenderThreadSharedComponent& cmp = view.get<RenderThreadSharedComponent>(e);
        RenderThreadSharedComponentData& data = *cmp.mData;
        assert(!data.wasDestroyed);
        switch (data.type) {
            case RenderThreadSharedComponentType::ItemSack: {
                data.resourceMutex.lock(); // LOCK
                RenderThreadSharedComponentItemSackResource& resource = *std::static_pointer_cast<RenderThreadSharedComponentItemSackResource>(data.resource);
                if (const TileItemContainerComponent* containerCmp = registry.try_get<TileItemContainerComponent>(e)) {
                    resource.itemStacks = containerCmp->getItemStacks();
                }
                else {
                    resource.itemStacks.clear();
                }
                // Remove when the container is empty
                if (resource.itemStacks.empty()) {
                    data.resourceMutex.unlock(); // UNLOCK
                    data.wasDestroyed = true; // Atomic
                    registry.remove<RenderThreadSharedComponent>(e);
                }
                else {
                    data.resourceMutex.unlock(); // UNLOCK
                }
                break;
            }
            default:
                assert(false);
                break;

        }
        static_assert(e_count(RenderThreadSharedComponentType) == 1, "Update RenderThreadSharedComponentSystem");
    }

}
