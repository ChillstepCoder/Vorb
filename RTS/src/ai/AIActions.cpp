#include "stdafx.h"
#include "AIActions.h"

#include "world/World.h"
#include "world/simulation/host/component/SimCharacterComponents.h"

#include "world/chunk/SimChunkGrid.h"

void AIActions::dropBundleSim(World& world, entt::registry& simRegistry, entt::entity simAgent) {
    ASSERT_SIM_THREAD();
    if (SimResourceBundleComponent* bundle = simRegistry.try_get<SimResourceBundleComponent>(simAgent)) {
        TileCoord worldPos(i32v2(simRegistry.get<SimPositionComponent>(simAgent).getPosition()));
        
        if (bundle->itemStack.count > 0) {
            const bool success = world.getSimChunkGrid().tryDropItemStackOnGround(ItemStack(bundle->itemStack), worldPos);
            assert(success);
        }

        simRegistry.remove<SimResourceBundleComponent>(simAgent);
    }
}
