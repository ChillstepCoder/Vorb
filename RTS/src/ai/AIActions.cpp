#include "stdafx.h"
#include "AIActions.h"

#include "world/World.h"
#include "world/simulation/host/component/SimCharacterComponents.h"

#include "world/chunk/SimChunkGrid.h"

// =====================================================================================================================
// Drop Bundle
// =====================================================================================================================

void AIActions::dropBundleSim(World& world, entt::registry& simRegistry, entt::entity simAgent) {
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

void AIActions::dropBundleFull(World& world, entt::registry& fullRegistry, entt::entity fullAgent, IFullECS& ecs) {
    ASSERT_GAME_THREAD();
    if (DualResourceBundleComponent* bundle = fullRegistry.try_get<DualResourceBundleComponent>(fullAgent)) {
        TileCoord worldPos(i32v2(fullRegistry.get<SimPositionComponent>(fullAgent).getPosition()));

        if (bundle->itemStack.count > 0) {
            assert(bundle->itemStack.itemId != INVALID_ITEM_ID);
            const bool success = world.getSimChunkGrid().tryDropItemStackOnGround(ItemStack(bundle->itemStack), worldPos);
            assert(success);
        }

        fullRegistry.remove<DualResourceBundleComponent>(fullAgent);
    }
}
