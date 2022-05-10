#include "stdafx.h"

#include "BuildTask.h"
#include "ecs/component/NavigationComponent.h"
#include "ecs/component/PhysicsComponent.h"
#include "ecs/component/InventoryComponent.h"
#include "ecs/component/TimedTileInteractComponent.h"

#include "resources/TileRepository.h"
#include "World.h"

#include "city/BuildingBlueprint.h"

#include <boost/pool/singleton_pool.hpp>

struct build_pool {};
using singleton_task_pool = boost::singleton_pool<build_pool, sizeof(BuildTask), boost::default_user_allocator_new_delete, boost::details::pool::null_mutex, 64u>;

BuildTask::BuildTask(BuildingBlueprint& blueprint, std::vector<std::unique_ptr<ItemReservation>>&& sourceItems, std::vector<ui16>&& targetTiles) : mSourceItems(std::move(sourceItems)), mTargetTiles(std::move(targetTiles)), mBlueprint(blueprint) {
    assert(mSourceItems.size());
}

BuildTask::~BuildTask()
{
    // TODO Check if this runs
}

bool BuildTask::tick(World& world, entt::registry& registry, entt::entity agent) {
    switch (mState) {
        case BuildTaskState::FULFILL_RESERVATIONS: {
            pathToStockpileSlot(world, registry, agent);
            break;
        }
        case BuildTaskState::PATH_TO_STOCKPILE_SLOT:
        case BuildTaskState::PATH_TO_BLUEPRINT_TILE:
            // Awaiting callback
            break;
        case BuildTaskState::PULL_ITEM_FROM_STOCKPILE_SLOT:
            pullItemFromStockpile(world, registry, agent);
            break;
        case BuildTaskState::BUILD_TILE:
            buildTile(world, registry, agent);
            break;
        case BuildTaskState::SUCCESS:
        case BuildTaskState::FAIL:
            return true;
        default:
            assert(false);
            break;

    }
    return false;
}

void* BuildTask::operator new(size_t count) {
    assert(IS_MAIN_THREAD());
    UNUSED(count);
    return singleton_task_pool::malloc();
}

void BuildTask::operator delete(void* pointer, size_t size) {
    assert(IS_MAIN_THREAD());
    UNUSED(size);
    return singleton_task_pool::free(pointer);
}

void BuildTask::pathToStockpileSlot(World& world, entt::registry& registry, entt::entity agent) {
    PhysicsComponent& physCmp = registry.get<PhysicsComponent>(agent);
    NavigationComponent& navCmp = registry.get_or_emplace<NavigationComponent>(agent);
    assert(!navCmp.mCoarsePath);

    const f32v2& myPos = physCmp.getXYPosition();

    // TODO: Make sure the stockpile didnt die
    assert(mSourceItems.size());
    PathPoint targetPos(mSourceItems.back()->getCurrentTargetWorldPosition());

    // Path to the stockpile
    navCmp.requestCoarsePathWithCallback(PathPoint(myPos), targetPos, [this](bool success) {
        if (success) {
            mState = BuildTaskState::PULL_ITEM_FROM_STOCKPILE_SLOT;
        }
        else {
            // Failed to path, fail he task
            failTask();
        }
    });
    mState = BuildTaskState::PATH_TO_STOCKPILE_SLOT;
}

void BuildTask::pullItemFromStockpile(World& world, entt::registry& registry, entt::entity agent) {
    // Drop resources into the stockpile
    PhysicsComponent& physCmp = registry.get<PhysicsComponent>(agent);
    InventoryComponent& invCmp = registry.get<InventoryComponent>(agent);
    std::vector<ItemStack>& inventory = invCmp.getMutableWorkingStorage(e_cast(WorkStorageID::HAULING));
    ItemStack* targetStack = nullptr;
    ItemReservation& reservation = *mSourceItems.back();

    for (auto&& stack : inventory) {
        if (stack.id == reservation.getItemID()) {
            targetStack = &stack;
            break;
        }
    }
    if (!targetStack) {
        inventory.push_back(ItemStack{ reservation.getItemID(), 0u });
        targetStack = &inventory.back();
    }

    if (reservation.fulfillCurrentTarget(*targetStack)) {
        mSourceItems.pop_back();
    }

    if (mSourceItems.empty()) {
        // We picked up everything, go to the blueprint
        pathToBlueprint(world, registry, agent);
    }
    else {
        // Need to grab more items
        pathToStockpileSlot(world, registry, agent);
    }
}

void BuildTask::pathToBlueprint(World& world, entt::registry& registry, entt::entity agent) {
    PhysicsComponent& physCmp = registry.get<PhysicsComponent>(agent);
    NavigationComponent& navCmp = registry.get_or_emplace<NavigationComponent>(agent);
    assert(!navCmp.mCoarsePath);

    const f32v2& myPos = physCmp.getXYPosition();

    PathPoint targetPos(mBlueprint.getWorldPositionOfTile(mTargetTiles.back()));

    navCmp.requestCoarsePathWithCallback(PathPoint(myPos), targetPos, [this](bool success) {
        if (success) {
            mState = BuildTaskState::BUILD_TILE;
        }
        else {
            // Failed to path, fail he task
            failTask();
        }
    });
    mState = BuildTaskState::PATH_TO_BLUEPRINT_TILE;
}

void BuildTask::buildTile(World& world, entt::registry& registry, entt::entity agent) {
    ui16 tileIndex = mTargetTiles.back();
    mTargetTiles.pop_back();

    BlueprintTile& bpTile = mBlueprint.tiles[tileIndex];
    const TileID tileId = mBlueprint.tileIDs[e_cast(bpTile.type)];
    const TileData& tileData = TileRepository::getTileData(tileId);
    const auto& recipe = *mBlueprint.tileRecipes[e_cast(bpTile.type)];

    InventoryComponent& invCmp = registry.get<InventoryComponent>(agent);
    std::vector<ItemStack>& inventory = invCmp.getMutableWorkingStorage(e_cast(WorkStorageID::HAULING));

    // Consume items (TODO: Utility function)
    for (auto&& itemStack : recipe) {
        bool found = false;
        for (size_t i = 0; i < inventory.size(); ++i) {
            ItemStack& invStack = inventory[i];
            if (itemStack.id == invStack.id) {
                found = true;
                assert(invStack.quantity >= itemStack.quantity);
                invStack.quantity -= itemStack.quantity;
                if (invStack.quantity == 0) {
                    inventory[i] = inventory.back();
                    inventory.pop_back();
                }
                break;
            }
        }
        assert(found);
    }

    // Build tile
    const ui32v2 worldPos = mBlueprint.getWorldPositionOfTile(tileIndex);
    TileHandle tileHandle = world.getTileHandleAtWorldPos(worldPos);
    TileContainer& tiles = *tileHandle.getMutableContainer();
    tiles.setTileLayer(tileHandle.index, (TileLayer)tileData.layer, tileId);
    // Walls have higher base Z position
    if (bpTile.type == BlueprintTileType::WALL) {
        tiles.setTileBaseZPosition(tileHandle.index, tileHandle.tile->getBaseZPositionUncompressedMainThread() + 3.0f);
    }

    // Notify blueprint
    bpTile.isBuilt = true;
    ++mBlueprint.tilesBuilt;

    if (mTargetTiles.empty()) {
        mState = BuildTaskState::SUCCESS;
    }
    else {
        pathToBlueprint(world, registry, agent);
    }
}

void BuildTask::failTask() {
    /* if (mState <= GatherTaskState::HARVESTING) {
         mTileTarget.getMutableChunk()->clearTileFlag(mTileTarget.index, TILE_FLAG_IS_RESOURCE_RESERVED);
         mState = GatherTaskState::FAIL;
     }*/
    mSourceItems.clear();
}
