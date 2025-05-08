#include "stdafx.h"
#include "EntityActions.h"

#include "world/World.h"
#include "world/simulation/host/component/SimCharacterComponents.h"
#include "ecs/component/PositionComponent.h"
#include "ecs/factory/EntityFactory.h"

#include "ecs/IFullECS.h"

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
            const TileItemUID uid = world.getSimChunkGrid().tryDropItemStackOnGroundSimThread(ItemStack(bundle->itemStack), worldPos);
            assert(uid != INVALID_TILE_ITEM_UID);
        }

        simRegistry.remove<DualResourceBundleComponent>(simAgent);
    }
}

void EntityActions::dropBundleFull(World& world, entt::registry& fullRegistry, entt::entity fullAgent) {
    ASSERT_GAME_THREAD();
    IFullECS& ecs = world.getECS();
    if (DualResourceBundleComponent* bundle = fullRegistry.try_get<DualResourceBundleComponent>(fullAgent)) {
        const f32v3 worldPos(fullRegistry.get<PositionComponent>(fullAgent).mPosition);

        if (bundle->itemStack.count > 0) {
            assert(bundle->itemStack.itemId != INVALID_ITEM_ID);
            const TileItemUID uid = world.getSimChunkGrid().tryDropItemStackOnGroundGameThread(
                ItemStack(bundle->itemStack), worldPos
            );
            assert(uid != INVALID_TILE_ITEM_UID);
        }

        fullRegistry.remove<DualResourceBundleComponent>(fullAgent);
    }
}
