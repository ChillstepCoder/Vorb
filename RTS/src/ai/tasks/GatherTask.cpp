#include "stdafx.h"
#include "GatherTask.h"

#include "World.h"
#include "ecs/component/NavigationComponent.h"
#include "ecs/component/PhysicsComponent.h"
#include "ecs/component/TimedTileInteractComponent.h"

#include "ecs/component/InventoryComponent.h"
#include "services/Services.h"
#include "ResourceManager.h"
#include "item/ItemRepository.h"
#include "world/Tile.h"
#include "Random.h"

GatherTask::GatherTask(LiteTileHandle tileTarget, TileResource resource) :
    mTileTarget(tileTarget),
    mResource(resource) {

}

bool GatherTask::tick(World& world, entt::registry& registry, entt::entity agent) {
    switch (mState) {
        case GatherTaskState::INIT: {
            init(world, registry, agent);
            break;
        }
        case GatherTaskState::PATH_TO_RESOURCE:
            // Awaiting callback
            break;
        case GatherTaskState::BEGIN_HARVEST:
            // Make sure tile still has the resource
            if (!beginHarvest(world, registry, agent)) {
                mState = GatherTaskState::FAIL;
                return true;
            }
            break;
        case GatherTaskState::HARVESTING: {
            // Once the component is destroyed, we are done
            if (!registry.try_get<TimedTileInteractComponent>(agent)) {
                pathToHome(world, registry, agent);
            }
            break;
        }
        case GatherTaskState::PATH_TO_HOME:
            // Awaiting callback
            break;
        case GatherTaskState::SUCCESS:
        case GatherTaskState::FAIL:
            return true;
        default:
            assert(false);
            break;
    }
    return false;
}

void GatherTask::init(World& world, entt::registry& registry, entt::entity agent) {

    NavigationComponent& navCmp = registry.get_or_emplace<NavigationComponent>(agent);
    // If we already have a path, wait for it to finish
    if (navCmp.mPath) {
        return;
    }
    PhysicsComponent& physCmp = registry.get<PhysicsComponent>(agent);

    // Make sure tile still has the resource
    if (!world.tileHasHarvestableResource(mTileTarget.getWorldPos(), mResource, nullptr)) {
        mState = GatherTaskState::FAIL;
        return;
    }

    // TODO: temp    
    mReturnPos = physCmp.getXYPosition();

    navCmp.setPathWithCallback(
        Services::PathFinder::ref().generatePathSynchronous(world, mTileTarget.getWorldPos(), physCmp.getXYPosition()),
        [this](bool success) {
            if (success == true) {
                mState = GatherTaskState::BEGIN_HARVEST;
            }
            else {
                // Failed to path, fail he task
                mState = GatherTaskState::FAIL;
            }
        }
    );
    mState = GatherTaskState::PATH_TO_RESOURCE;
}

bool GatherTask::beginHarvest(World& world, entt::registry& registry, entt::entity agent)
{
    PhysicsComponent& physCmp = registry.get<PhysicsComponent>(agent);
    TileLayer layer;
    if (!world.tileHasHarvestableResource(mTileTarget.getWorldPos(), mResource, &layer)) {
        mState = GatherTaskState::FAIL;
        return false;
    }

    // Interact
    constexpr int INTERACT_TICKS = 60;
    TimedTileInteractComponent& interact = registry.emplace<TimedTileInteractComponent>(
        agent,
        world.getTileHandleAtWorldPos(mTileTarget.getWorldPos()),
        enum_cast(layer),
        INTERACT_TICKS,
        0,
        [&world, &registry, agent, this](bool, TimedTileInteractComponent& cmp) {
            // TODO: Interact lock???
            auto&& tileRef = cmp.mInteractTile;
            //if (tileHandle.tile.layers[cmp.mTileLayer])
            const TileData& tileData = TileRepository::getTileData(tileRef->tile.layers[cmp.mTileLayer]);
            tileRef->tile.layers[cmp.mTileLayer] = TILE_ID_NONE;
            // Award loot
            InventoryComponent& invCmp = registry.get<InventoryComponent>(agent);
            for (size_t i = 0; i < tileData.itemDrops.size(); ++i) {
                const ItemDropDef& dropDef = tileData.itemDrops[i];
                ItemStack stack;
                if (dropDef.countRange.y <= dropDef.countRange.x) {
                    stack.quantity = dropDef.countRange.y;
                }
                else {
                    stack.quantity = Random::getCachedRandom() % (dropDef.countRange.y - dropDef.countRange.x) + dropDef.countRange.x;
                }
                // TODO: More efficient lookup
                stack.id = Services::ResourceManager::ref().getItemRepository().getItem(dropDef.itemName).getID();
                invCmp.addOrDropItemStackToWorkingStorage(stack, enum_cast(WorkStorageID::HAULING));
            }
        }
    );
    
    mState = GatherTaskState::HARVESTING;

    return true;
}

void GatherTask::pathToHome(World& world, entt::registry& registry, entt::entity agent)
{
    PhysicsComponent& physCmp = registry.get<PhysicsComponent>(agent);
    NavigationComponent& navCmp = registry.get_or_emplace<NavigationComponent>(agent);
    assert(!navCmp.mPath);

    navCmp.setPathWithCallback(
        Services::PathFinder::ref().generatePathSynchronous(world, mReturnPos, physCmp.getXYPosition()),
        [this, &registry, agent, &world](bool success) {
            if (success == true) {
                // Drop resources into the stockpile
                InventoryComponent& invCmp = registry.get<InventoryComponent>(agent);
                std::vector<ItemStack> items = invCmp.getMutableWorkingStorage(enum_cast(WorkStorageID::HAULING));
                // TODO: Path to target pos
                for (auto&& it : items) {
                    assert(false);
                }
                invCmp.eraseWorkingStorage(enum_cast(WorkStorageID::HAULING));
                mState = GatherTaskState::SUCCESS;
            }
            else {
                // Failed to path, fail he task
                mState = GatherTaskState::FAIL;
            }
        }
    );

    mState = GatherTaskState::PATH_TO_HOME;
}
