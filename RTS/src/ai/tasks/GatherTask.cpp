#include "stdafx.h"
#include "GatherTask.h"

#include "world/World.h"
#include "ecs/component/NavigationComponent.h"
#include "ecs/component/PositionComponent.h"
#include "ecs/component/TimedTileInteractComponent.h"

#include "city/City.h"
#include "city/CityQuartermaster.h"

#include "ecs/component/InventoryComponent.h"
#include "resources/ResourceManager.h"
#include "item/ItemRepository.h"
#include "item/ItemStockpile.h"
#include "resources/TileRepository.h"
#include "math/Random.h"



HarvestItemsTask::HarvestItemsTask(ItemID itemId, ui16 itemCount, AgentTaskFinishedFunc finishedFunc) : mItemId(itemId), mTargetCount(itemCount), IAgentTask(finishedFunc) {
    mTargetHarvestable = ItemRepository::get().getLoadedOrUnloadedAsset(mItemId).getSourceHarvestable();
    assert(mTargetHarvestable != TileHarvestable::NONE);
}

HarvestItemsTask::~HarvestItemsTask() {

}

POOLED_ALLOC_DEF_NOT_THREADSAFE(HarvestItemsTask, 64u, ASSERT_GAME_THREAD());

TaskTickResult HarvestItemsTask::tick(World& world, entt::registry& registry, entt::entity agent) {
    switch (mState) {
        case TaskState::FIND_ITEM:
            findItem(registry, agent);
            break;
        case TaskState::PATH_TO_ITEM: {
            // NavigationStatusComponent? Simple 1 byte lookup?
            NavigationComponent& navCmp = registry.get<NavigationComponent>(agent);
            const NavigationStatus navStatus = navCmp.getStatus();
            if (navStatus == NavigationStatus::SUCCESS) {
                harvestItem(world, registry, agent, navCmp.mTargetHandle);
            }
            else if (navStatus == NavigationStatus::FAIL) {
                if (++mFailCount >= 4) {
                    LOG_CRITICAL("HarvestItemsTask failed after 4 attempts");
                    return TaskTickResult::FAIL;
                }
                else {
                    mState = TaskState::FIND_ITEM;
                }
            }
            break;
        }
        case TaskState::HARVESTING:
            // TODO: TimedInteractComponent needs to handle interact cancel!
            break;
        case TaskState::SUCCESS:
            return TaskTickResult::SUCCESS;
        case TaskState::FAIL:
            return TaskTickResult::FAIL;
        default:
            assert(false);
      
    }
    return TaskTickResult::IN_PROGRESS;
}

void HarvestItemsTask::findItem(entt::registry& registry, entt::entity agent) {
    PositionComponent& posCmp = registry.get<PositionComponent>(agent);
    NavigationComponent& cmp = registry.get_or_emplace<NavigationComponent>(agent);
    cmp.requestCoarsePathToHarvestable(posCmp.mPosition, TileHarvestable::WOOD, 1024.0f, nullptr);
    mState = TaskState::PATH_TO_ITEM;
}

void HarvestItemsTask::harvestItem(World& world, entt::registry& registry, entt::entity agent, TileHandle targetTileHandle) {
    //ASSERT_GAME_THREAD();

    //assert(targetTileHandle.isValid());
    //PositionComponent& posCmp = registry.get<PositionComponent>(agent);
    //TileLayer layer;
    //if (!world.terrainTileHasHarvestable(targetTileHandle.getWorldPos2D(), mTargetHarvestable, &layer)) {
    //    // Try again
    //    findItem(registry, agent);
    //    return;
    //}

    //// Interact
    //if (targetTileHandle.getTile().hasFlagsMaskAny(e_cast(TileFlags::IS_INTERACTING) | e_cast(TileFlags::IS_RESOURCE_RESERVED))) {
    //    // Someone else is using this tile
    //    // Try again
    //    findItem(registry, agent);
    //    return;
    //}

    //constexpr int INTERACT_TICKS = 60;
    //TimedTileInteractComponent& interact = registry.emplace<TimedTileInteractComponent>(
    //    agent,
    //    targetTileHandle,
    //    e_cast(layer),
    //    INTERACT_TICKS,
    //    0,
    //    [&registry, agent, this](bool success, TimedTileInteractComponent& cmp) {
    //    ASSERT_GAME_THREAD();

    //    // TODO: Handle failure
    //    assert(success);

    //    // TODO: Interact lock???
    //    auto&& tileRef = cmp.mInteractTile;
    //    //if (tileHandle.tile.layers[cmp.mTileLayer])
    //    TileID tileId = tileRef->tile->getLayers()[cmp.mTileLayer];
    //    const TileDef& tileData = TileRepository::get().getLoadedOrUnloadedAsset(tileId);
    //    // Destroy tile
    //    tileRef->container->clearTileFlag(tileRef->index, TileFlags::IS_RESOURCE_RESERVED); // Possible race condition? We could doubitemPromisele clear this in failTask()
    //    tileRef->container->setTileLayer(tileRef->index, (TileLayer)cmp.mTileLayer, TILE_ID_NONE);

    //    // Award loot
    //    InventoryComponent& invCmp = registry.get<InventoryComponent>(agent);
    //    for (size_t i = 0; i < tileData.itemDrops.size(); ++i) {
    //        const ItemDrop& drop = tileData.itemDrops[i];
    //        ItemStack stack;
    //        if (drop.countRange.y <= drop.countRange.x) {
    //            stack.quantity = drop.countRange.y;
    //        }
    //        else {
    //            stack.quantity = Random::getCachedRandom() % (drop.countRange.y - drop.countRange.x) + drop.countRange.x;
    //        }
    //        stack.id = drop.id;
    //        invCmp.tryAddItemStackToWorkingStorage(stack, WorkStorageID::HAULING);

    //        if (stack.id == mItemId) {
    //            mCurrentCount += stack.quantity;
    //            if (mCurrentCount >= mTargetCount) {
    //                mState = TaskState::SUCCESS;
    //            }
    //        }
    //        
    //    }
    //    // Find next
    //    if (mState != TaskState::SUCCESS) {
    //        findItem(registry, agent);
    //    }
    //});

    //mState = TaskState::HARVESTING;

}

void HarvestItemsTask::failTask()
{
    mState = TaskState::FAIL;
}
