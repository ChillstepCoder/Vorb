#include "stdafx.h"
#include "EntityActions.h"

#include "world/World.h"
#include "world/simulation/host/component/SimCharacterComponents.h"

#include "world/chunk/SimChunkGrid.h"

// =====================================================================================================================
// Drop Bundle
// =====================================================================================================================

void EntityActions::dropBundleSim(World& world, entt::registry& simRegistry, entt::entity simAgent) {
    ASSERT_SIM_THREAD();
    if (DualResourceBundleComponent* bundle = simRegistry.try_get<DualResourceBundleComponent>(simAgent)) {
        TileCoord worldPos(i32v2(simRegistry.get<SimPositionComponent>(simAgent).getPosition()));
        
        if (bundle->itemStack.count > 0) {
            assert(bundle->itemStack.itemId != INVALID_ITEM_ID);
            const bool success = world.getSimChunkGrid().tryDropItemStackOnGround(ItemStack(bundle->itemStack), worldPos);
            assert(success);
        }

        simRegistry.remove<DualResourceBundleComponent>(simAgent);
    }
}

void EntityActions::dropBundleFull(World& world, entt::registry& fullRegistry, entt::entity fullAgent) {
    IFullECS& ecs = world.getECS();
    ASSERT_GAME_THREAD();
    if (DualResourceBundleComponent* bundle = fullRegistry.try_get<DualResourceBundleComponent>(fullAgent)) {
        TileCoord worldPos(i32v2(fullRegistry.get<SimPositionComponent>(fullAgent).getPosition()));

        if (bundle->itemStack.count > 0) {
            assert(bundle->itemStack.itemId != INVALID_ITEM_ID);
            const bool success = world.getSimChunkGrid().tryDropItemStackOnGround(ItemStack(bundle->itemStack), worldPos);
            assert(success);
            assert(false); // Need to create the item entity?
        }

        fullRegistry.remove<DualResourceBundleComponent>(fullAgent);
    }
}
