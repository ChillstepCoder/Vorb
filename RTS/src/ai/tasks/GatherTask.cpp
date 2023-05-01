#include "stdafx.h"
#include "GatherTask.h"

#include "world/IWorld.h"
#include "ecs/component/NavigationComponent.h"
#include "ecs/component/PhysicsComponent.h"
#include "ecs/component/TimedTileInteractComponent.h"

#include "city/City.h"
#include "city/CityQuartermaster.h"

#include "ecs/component/InventoryComponent.h"
#include "resources/ResourceManager.h"
#include "item/ItemRepository.h"
#include "item/ItemStockpile.h"
#include "resources/TileRepository.h"
#include "math/Random.h"

#include <boost/pool/singleton_pool.hpp>

struct gather_pool {};
using singleton_task_pool = boost::singleton_pool<gather_pool, sizeof(HarvestItemsTask), boost::default_user_allocator_new_delete, boost::details::pool::null_mutex, 64u>;

HarvestItemsTask::HarvestItemsTask(ItemID itemId, ui16 itemCount, AgentTaskFinishedFunc finishedFunc) : mItemId(itemId), mTargetCount(itemCount), IAgentTask(finishedFunc) {
    mTargetHarvestable = sItemRepository->getItem(mItemId).getSourceHarvestable();
    assert(mTargetHarvestable != TileHarvestable::NONE);
}

HarvestItemsTask::~HarvestItemsTask() {

}

void* HarvestItemsTask::operator new(size_t count) {
    assert(IS_GAME_THREAD());
    UNUSED(count);
    return singleton_task_pool::malloc();
}

void HarvestItemsTask::operator delete(void* pointer, size_t size) {
    assert(IS_GAME_THREAD());
    UNUSED(size);
    return singleton_task_pool::free(pointer);
}

TaskTickResult HarvestItemsTask::tick(IWorld& world, entt::registry& registry, entt::entity agent) {
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
    PhysicsComponent& physCmp = registry.get<PhysicsComponent>(agent);
    NavigationComponent& cmp = registry.get_or_emplace<NavigationComponent>(agent);
    cmp.requestCoarsePathToHarvestable(physCmp.getPosition(), TileHarvestable::WOOD, 1024.0f, nullptr);
    mState = TaskState::PATH_TO_ITEM;
}

void HarvestItemsTask::harvestItem(IWorld& world, entt::registry& registry, entt::entity agent, TileHandle targetTileHandle) {
    assert(IS_GAME_THREAD());

    assert(targetTileHandle.isValid());
    PhysicsComponent& physCmp = registry.get<PhysicsComponent>(agent);
    TileLayer layer;
    if (!world.terrainTileHasHarvestable(targetTileHandle.getWorldPos2D(), mTargetHarvestable, &layer)) {
        // Try again
        findItem(registry, agent);
        return;
    }

    // Interact
    if (targetTileHandle.getTile().hasFlagsMaskAny(e_cast(TileFlags::IS_INTERACTING) | e_cast(TileFlags::IS_RESOURCE_RESERVED))) {
        // Someone else is using this tile
        // Try again
        findItem(registry, agent);
        return;
    }

    constexpr int INTERACT_TICKS = 60;
    TimedTileInteractComponent& interact = registry.emplace<TimedTileInteractComponent>(
        agent,
        targetTileHandle,
        e_cast(layer),
        INTERACT_TICKS,
        0,
        [&registry, agent, this](bool success, TimedTileInteractComponent& cmp) {
        assert(IS_GAME_THREAD());

        // TODO: Handle failure
        assert(success);

        // TODO: Interact lock???
        auto&& tileRef = cmp.mInteractTile;
        //if (tileHandle.tile.layers[cmp.mTileLayer])
        TileID tileId = tileRef->tile->getLayers()[cmp.mTileLayer];
        const TileData& tileData = TileRepository::getTileData(tileId);
        // Destroy tile
        tileRef->container->clearTileFlag(tileRef->index, TileFlags::IS_RESOURCE_RESERVED); // Possible race condition? We could doubitemPromisele clear this in failTask()
        tileRef->container->setTileLayer(tileRef->index, (TileLayer)cmp.mTileLayer, TILE_ID_NONE);

        // Award loot
        InventoryComponent& invCmp = registry.get<InventoryComponent>(agent);
        for (size_t i = 0; i < tileData.itemDrops.size(); ++i) {
            const ItemDrop& drop = tileData.itemDrops[i];
            ItemStack stack;
            if (drop.countRange.y <= drop.countRange.x) {
                stack.quantity = drop.countRange.y;
            }
            else {
                stack.quantity = Random::getCachedRandom() % (drop.countRange.y - drop.countRange.x) + drop.countRange.x;
            }
            stack.id = drop.id;
            invCmp.addItemStackToWorkingStorage(stack, WorkStorageID::HAULING);

            if (stack.id == mItemId) {
                mCurrentCount += stack.quantity;
                if (mCurrentCount >= mTargetCount) {
                    mState = TaskState::SUCCESS;
                }
            }
            
        }
        // Find next
        if (mState != TaskState::SUCCESS) {
            findItem(registry, agent);
        }
    });

    mState = TaskState::HARVESTING;

}

void HarvestItemsTask::failTask()
{
    mState = TaskState::FAIL;
}
