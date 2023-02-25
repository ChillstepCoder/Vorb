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
using singleton_task_pool = boost::singleton_pool<gather_pool, sizeof(GatherTask), boost::default_user_allocator_new_delete, boost::details::pool::null_mutex, 64u>;

GatherTask::GatherTask(TileHandle tileTarget, TileResource resource, std::unique_ptr<ItemReservation> itemPromise) :
    mTileTarget(tileTarget),
    mResource(resource),
    mItemPromise(std::move(itemPromise)) {
    // Gather task requires target tile to be reserved already
    assert(tileTarget.tile->hasFlag(TileFlags::IS_RESOURCE_RESERVED));
    assert(mItemPromise->isPromise());
}

GatherTask::~GatherTask() {
    // Clear tile flag on abort
    if (!IS_SHUTTING_DOWN) {
        if (mState <= GatherTaskState::HARVESTING) {
            mTileTarget.getMutableContainer()->clearTileFlag(mTileTarget.tileIndex, TileFlags::IS_RESOURCE_RESERVED);
        }
    }
}

bool GatherTask::tick(entt::registry& registry, entt::entity agent) {
    switch (mState) {
        case GatherTaskState::INIT: {
            init(registry, agent);
            break;
        }
        case GatherTaskState::PATH_TO_RESOURCE:
            // Awaiting callback
            break;
        case GatherTaskState::BEGIN_HARVEST:
            // Make sure tile still has the resource
            if (!beginHarvest(registry, agent)) {
                if (mState == GatherTaskState::FAIL) {
                    return true;
                }
            }
            break;
        case GatherTaskState::HARVESTING: {
            // Once the component is destroyed, we are done
            if (!registry.try_get<TimedTileInteractComponent>(agent)) {
                pathToStockpileSlot(registry, agent);
            }
            break;
        }
        case GatherTaskState::PATH_TO_STOCKPILE_SLOT:
            // Awaiting callback
            break;
        case GatherTaskState::ADD_ITEM_TO_STOCKPILE_SLOT:
            addItemToStockpile(registry, agent);
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

void* GatherTask::operator new(size_t count) {
    assert(IS_GAME_THREAD());
    UNUSED(count);
    return singleton_task_pool::malloc();
}

void GatherTask::operator delete(void* pointer, size_t size) {
    assert(IS_GAME_THREAD());
    UNUSED(size);
    return singleton_task_pool::free(pointer);
}

void GatherTask::init(entt::registry& registry, entt::entity agent) {

    NavigationComponent& navCmp = registry.get<NavigationComponent>(agent);
    // If we already have a path, wait for it to finish
    if (navCmp.mFinePath || navCmp.mCoarsePath) {
        return;
    }
    PhysicsComponent& physCmp = registry.get<PhysicsComponent>(agent);

    // Make sure tile still has the resource
    // TODO: Handle target tile being invalid!
    // TODO: TileRefWeak?
    if (!mTileTarget.tile->hasHarvestableResource(mResource, nullptr)) {
        failTask();
        return;
    }

    navCmp.requestCoarsePathWithCallback(physCmp.getPosition(), mTileTarget.getWorldPos3D(), [this](bool success) {
        if (success == true) {
            mState = GatherTaskState::BEGIN_HARVEST;
        }
        else {
            // Failed to path, fail he task
            failTask();
        }
    });
    mState = GatherTaskState::PATH_TO_RESOURCE;
}

bool GatherTask::beginHarvest(entt::registry& registry, entt::entity agent)
{
    PhysicsComponent& physCmp = registry.get<PhysicsComponent>(agent);
    TileLayer layer;
    if (!sWorld->terrainTileHasHarvestableResource(mTileTarget.getWorldPos2D(), mResource, &layer)) {
        failTask();
        return false;
    }

    // Interact
    if (mTileTarget.tile->hasFlag(TileFlags::IS_INTERACTING)) {
        // Someone else is using this tile, try again next tick.
        return false;
    }

    constexpr int INTERACT_TICKS = 60;
    TimedTileInteractComponent& interact = registry.emplace<TimedTileInteractComponent>(
        agent,
        mTileTarget,
        e_cast(layer),
        INTERACT_TICKS,
        0,
        [&registry, agent, this](bool, TimedTileInteractComponent& cmp) {
            // TODO: Interact lock???
            auto&& tileRef = cmp.mInteractTile;
            //if (tileHandle.tile.layers[cmp.mTileLayer])
            TileID tileId = tileRef->tile->getLayers()[cmp.mTileLayer];
            const TileData& tileData = TileRepository::getTileData(tileId);
            tileRef->container->setTileLayer(tileRef->index, (TileLayer)cmp.mTileLayer, TILE_ID_NONE);
            tileRef->container->clearTileFlag(mTileTarget.tileIndex, TileFlags::IS_RESOURCE_RESERVED); // Possible race condition? We could doubitemPromisele clear this in failTask()
            // TODO: Play animation of tree falling

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
                invCmp.addOrDropItemStackToWorkingStorage(stack, e_cast(WorkStorageID::HAULING));
            }
        }
    );

    mState = GatherTaskState::HARVESTING;

    return true;
}

// TODO: HaulTask
void GatherTask::pathToStockpileSlot(entt::registry& registry, entt::entity agent) {
    PhysicsComponent& physCmp = registry.get<PhysicsComponent>(agent);
    NavigationComponent& navCmp = registry.get<NavigationComponent>(agent);
    assert(!navCmp.mCoarsePath);

    const f32v3 myPos = physCmp.getPosition();

    // TODO: Make sure the stockpile didnt die
    assert(mItemPromise->isValid());
    // TODO: THIS SHOULD BE 3D!
    PathPoint targetPos(mItemPromise->getCurrentTargetWorldPosition());

    // Path to the stockpile
    navCmp.requestCoarsePathWithCallback(myPos, f32v3(targetPos.x, targetPos.y, 0.0f), [this](bool success) {
        if (success) {
            mState = GatherTaskState::ADD_ITEM_TO_STOCKPILE_SLOT;
        }
        else {
            // Failed to path, fail he task
            failTask();
        }
    });
    mState = GatherTaskState::PATH_TO_STOCKPILE_SLOT;
}

void GatherTask::addItemToStockpile(entt::registry& registry, entt::entity agent) {
    // Drop resources into the stockpile
    PhysicsComponent& physCmp = registry.get<PhysicsComponent>(agent);
    InventoryComponent& invCmp = registry.get<InventoryComponent>(agent);
    std::vector<ItemStack>& items = invCmp.getMutableWorkingStorage(e_cast(WorkStorageID::HAULING));
    if (items.empty()) {
        failTask();
        return;
    }

    bool didFulfill = false;
    bool finished = false;
    bool isOutOfItems = false;
    for (auto&& itemStack : items) {
        if (itemStack.id == mItemPromise->getItemID()) {
            finished = mItemPromise->fulfillCurrentTarget(itemStack);
            didFulfill = true;
            if (itemStack.quantity == 0) {
                isOutOfItems = true;
            }
            break;
        }
    }
    assert(didFulfill);

    if (finished) {
        mState = GatherTaskState::SUCCESS;
        mItemPromise = nullptr;
    }
    else {
        if (isOutOfItems) {
            // We are finished dumping, consider it success and cancel remaining promise
            mState = GatherTaskState::SUCCESS;
            mItemPromise = nullptr;
        }
        else {
            pathToStockpileSlot(registry, agent);
        }
    }
}

void GatherTask::failTask() {
    if (mState <= GatherTaskState::HARVESTING) {
        mTileTarget.getMutableContainer()->clearTileFlag(mTileTarget.tileIndex, TileFlags::IS_RESOURCE_RESERVED);
        mState = GatherTaskState::FAIL;
    }
    mItemPromise = nullptr;
}
