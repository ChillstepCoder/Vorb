#include "stdafx.h"
#include "GatherTask.h"

#include "World.h"
#include "ecs/component/NavigationComponent.h"
#include "ecs/component/PhysicsComponent.h"
#include "ecs/component/TimedTileInteractComponent.h"

#include "city/City.h"
#include "city/CityQuartermaster.h"

#include "ecs/component/InventoryComponent.h"
#include "services/Services.h"
#include "ResourceManager.h"
#include "item/ItemRepository.h"
#include "item/ItemStockpile.h"
#include "world/Tile.h"
#include "Random.h"

GatherTask::GatherTask(LiteTileHandle tileTarget, TileResource resource, City* city) :
    mTileTarget(tileTarget),
    mResource(resource),
    mCity(city) {

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
                if (mState == GatherTaskState::FAIL) {
                    return true;
                }
            }
            break;
        case GatherTaskState::HARVESTING: {
            // Once the component is destroyed, we are done
            if (!registry.try_get<TimedTileInteractComponent>(agent)) {
                pathToStockpile(world, registry, agent);
            }
            break;
        }
        case GatherTaskState::PATH_TO_STOCKPILE:
            // Awaiting callback
            break;
        case GatherTaskState::PICK_STOCKPILE_SLOT:
            addItemToStockpile(world, registry, agent);
            break;
        case GatherTaskState::PATH_TO_STOCKPILE_SLOT:
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
    TileHandle tileHandle = world.getTileHandleAtWorldPos(mTileTarget.getWorldPos());
    if (tileHandle.tile.hasFlag(TILE_FLAG_IS_INTERACTING)) {
        // Someone else is using this tile, try again next tick.
        return false;
    }

    constexpr int INTERACT_TICKS = 60;
    TimedTileInteractComponent& interact = registry.emplace<TimedTileInteractComponent>(
        agent,
        tileHandle,
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

// TODO: HaulTask
void GatherTask::pathToStockpile(World& world, entt::registry& registry, entt::entity agent) {
    PhysicsComponent& physCmp = registry.get<PhysicsComponent>(agent);
    NavigationComponent& navCmp = registry.get_or_emplace<NavigationComponent>(agent);
    assert(!navCmp.mPath);

    assert(mCity);
    const f32v2& myPos = physCmp.getXYPosition();
    ItemStockpile* closestStockpile = mCity->getCityQuartermaster().tryGetClosestStockpileToPoint(myPos);
    ui32v2 stockpileCenter = closestStockpile->getAABB().getCenter();

    // Path to the stockpile
    navCmp.setPathWithCallback(
        Services::PathFinder::ref().generatePathSynchronous(world, stockpileCenter, myPos),
        [this, &registry, agent, &world, stockpileCenter](bool success) {
            if (success) {
                mState = GatherTaskState::PICK_STOCKPILE_SLOT;
            }
            else {
                // Failed to path, fail he task
                mState = GatherTaskState::FAIL;
            }
        }
    );

    mState = GatherTaskState::PATH_TO_STOCKPILE;
}

void GatherTask::addItemToStockpile(World& world, entt::registry& registry, entt::entity agent) {
    // Drop resources into the stockpile
    PhysicsComponent& physCmp = registry.get<PhysicsComponent>(agent);
    InventoryComponent& invCmp = registry.get<InventoryComponent>(agent);
    std::vector<ItemStack>& items = invCmp.getMutableWorkingStorage(enum_cast(WorkStorageID::HAULING));
    if (items.empty()) {
        mState = GatherTaskState::SUCCESS;
        return;
    }

    const f32v2& myPos = physCmp.getXYPosition();
    // TODO: Just get at point? This is another lookup
    ItemStockpile* closestStockpile = mCity->getCityQuartermaster().tryGetClosestStockpileToPoint(myPos);
    // TODO: Path to target pos
    // Insert each item into stockpile storage
    // Get stockpile at position
    ItemStack& stack = items[0];
    ui32v2 posToInsert;
    if (closestStockpile->tryGetBestPositionToInsertItemStack(stack, &posToInsert)) {
        // Walk to the point and drop the item, no pathing
        NavigationComponent& navCmp = registry.get_or_emplace<NavigationComponent>(agent);
        navCmp.setSimpleLinearTargetPoint(posToInsert, [this, agent, &registry](bool success) {
            if (success) {
                PhysicsComponent& physCmp = registry.get<PhysicsComponent>(agent);
                const f32v2& myPos = physCmp.getXYPosition();
                ItemStockpile* closestStockpile = mCity->getCityQuartermaster().tryGetClosestStockpileToPoint(myPos);
                mState = GatherTaskState::PICK_STOCKPILE_SLOT;
            }
            mState = GatherTaskState::PICK_STOCKPILE_SLOT;
        });
    }
    invCmp.eraseWorkingStorage(enum_cast(WorkStorageID::HAULING));
}
