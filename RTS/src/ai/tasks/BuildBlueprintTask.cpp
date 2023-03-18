#include "stdafx.h"

#include "BuildBlueprintTask.h"
#include "ecs/component/NavigationComponent.h"
#include "ecs/component/PhysicsComponent.h"
#include "ecs/component/InventoryComponent.h"
#include "ecs/component/TimedTileInteractComponent.h"

#include "resources/TileRepository.h"
#include "world/IWorld.h"
#include "world/IHeightmapGrid.h"

#include <boost/pool/singleton_pool.hpp>

constexpr int MAX_ERROR_COUNT_BEFORE_FAIL = 4;

struct build_pool {};
using singleton_task_pool = boost::singleton_pool<build_pool, sizeof(BuildBlueprintTask), boost::default_user_allocator_new_delete, boost::details::pool::null_mutex, 64u>;

//
//TaskTickResult BuildTask::tick(entt::registry& registry, entt::entity agent) {
//    switch (mState) {
//        case BuildTaskState::FULFILL_RESERVATIONS: {
//            pathToStockpileSlot(registry, agent);
//            break;
//        }
//        case BuildTaskState::PATH_TO_STOCKPILE_SLOT:
//        case BuildTaskState::PATH_TO_BLUEPRINT_TILE:
//            // Awaiting callback
//            break;
//        case BuildTaskState::PULL_ITEM_FROM_STOCKPILE_SLOT:
//            pullItemFromStockpile(registry, agent);
//            break;
//        case BuildTaskState::BUILD_TILE:
//            buildTile(registry, agent);
//            break;
//        case BuildTaskState::SUCCESS:
//            return TaskTickResult::SUCCESS;
//        case BuildTaskState::FAIL:
//            return TaskTickResult::FAIL;
//        default:
//            assert(false);
//            break;
//
//    }
//    return TaskTickResult::IN_PROGRESS;
//}

void* BuildBlueprintTask::operator new(size_t count) {
    assert(IS_GAME_THREAD());
    UNUSED(count);
    return singleton_task_pool::malloc();
}

void BuildBlueprintTask::operator delete(void* pointer, size_t size) {
    assert(IS_GAME_THREAD());
    UNUSED(size);
    return singleton_task_pool::free(pointer);
}

//void BuildTask::pathToStockpileSlot(entt::registry& registry, entt::entity agent) {
//    PhysicsComponent& physCmp = registry.get<PhysicsComponent>(agent);
//    NavigationComponent& navCmp = registry.get_or_emplace<NavigationComponent>(agent);
//    assert(!navCmp.mCoarsePath);
//
//    const f32v3 myPos = physCmp.getPosition();
//
//    // TODO: Make sure the stockpile didnt die
//    assert(mSourceItems.size());
//    // TODO: THIS SHOULD BE 3D!
//    f32v2 targetPos(mSourceItems.back()->getCurrentTargetWorldPosition());
//    //assert(false); // itemreservation should use TileHandle or most probably, TileRef ACKSUALLY we dont want it to be tileRef or busy chunks will never LOD ai. Instead it should handle LOD transition
//    // Path to the stockpile
//    navCmp.requestCoarsePath(myPos, f32v3(targetPos.x, targetPos.y, 0.0f), [this](bool success) {
//        if (success) {
//            mState = BuildTaskState::PULL_ITEM_FROM_STOCKPILE_SLOT;
//        }
//        else {
//            // Failed to path, fail he task
//            failTask();
//        }
//    });
//    mState = BuildTaskState::PATH_TO_STOCKPILE_SLOT;
//}

//void BuildTask::pullItemFromStockpile(entt::registry& registry, entt::entity agent) {
//    // Drop resources into the stockpile
//    PhysicsComponent& physCmp = registry.get<PhysicsComponent>(agent);
//    InventoryComponent& invCmp = registry.get<InventoryComponent>(agent);
//    std::vector<ItemStack>& inventory = invCmp.getMutableWorkingStorage(e_cast(WorkStorageID::HAULING));
//    ItemStack* targetStack = nullptr;
//    ItemReservation& reservation = *mSourceItems.back();
//
//    for (auto&& stack : inventory) {
//        if (stack.id == reservation.getItemID()) {
//            targetStack = &stack;
//            break;
//        }
//    }
//    if (!targetStack) {
//        inventory.push_back(ItemStack{ reservation.getItemID(), 0u });
//        targetStack = &inventory.back();
//    }
//
//    if (reservation.fulfillCurrentTarget(*targetStack)) {
//        mSourceItems.pop_back();
//    }
//
//    if (mSourceItems.empty()) {
//        // We picked up everything, go to the blueprint
//        pathToBlueprint(registry, agent);
//    }
//    else {
//        // Need to grab more items
//        pathToStockpileSlot(registry, agent);
//    }
//}

//void BuildTask::pathToBlueprint(entt::registry& registry, entt::entity agent) {
//    PhysicsComponent& physCmp = registry.get<PhysicsComponent>(agent);
//    NavigationComponent& navCmp = registry.get_or_emplace<NavigationComponent>(agent);
//    assert(!navCmp.mCoarsePath);
//
//    const f32v3 myPos = physCmp.getPosition();
//    navCmp.requestCoarsePath(myPos, mBlueprint.getTileHandle(mTargetTiles.back()).getWorldPos3D(), [this](bool success) {
//        if (success) {
//            mState = BuildTaskState::BUILD_TILE;
//        }
//        else {
//            // Failed to path, fail he task
//            failTask();
//        }
//    });
//    mState = BuildTaskState::PATH_TO_BLUEPRINT_TILE;
//}

//void BuildTask::buildTile(entt::registry& registry, entt::entity agent) {
//    TileIndex tileIndex = mTargetTiles.back();
//    mTargetTiles.pop_back();
//
//    BlueprintTile& bpTile = mBlueprint.tiles[tileIndex];
//    const TileID tileId = mBlueprint.tileIDs[e_cast(bpTile.type)];
//    assert(tileId != TILE_ID_NONE);
//    const TileData& tileData = TileRepository::getTileData(tileId);
//    const auto& recipe = *mBlueprint.tileRecipes[e_cast(bpTile.type)];
//
//    InventoryComponent& invCmp = registry.get<InventoryComponent>(agent);
//    std::vector<ItemStack>& inventory = invCmp.getMutableWorkingStorage(e_cast(WorkStorageID::HAULING));
//
//    // Consume items (TODO: Utility function)
//    for (auto&& itemStack : recipe) {
//        bool found = false;
//        for (size_t i = 0; i < inventory.size(); ++i) {
//            ItemStack& invStack = inventory[i];
//            if (itemStack.id == invStack.id) {
//                found = true;
//                assert(invStack.quantity >= itemStack.quantity);
//                invStack.quantity -= itemStack.quantity;
//                if (invStack.quantity == 0) {
//                    inventory[i] = inventory.back();
//                    inventory.pop_back();
//                }
//                break;
//            }
//        }
//        assert(found);
//    }
//
//    // Build tile
//    TileHandle tileHandle = mBlueprint.getTileHandle(tileIndex);
//    TileContainer& tileContainer = *tileHandle.getMutableContainer();
//    const i32v3 xyzOffset = tileContainer.getTileXYZOffset(tileIndex);
//
//    if (bpTile.type != BlueprintTileType::NONE) {
//        // Flatten heightmap
//        if (xyzOffset.z == 0) {
//            IHeightmapGrid& grid = sWorld->getHeightmapGrid();
//            // Epsilon to prevent z fighting
//            grid.setHeightAt(tileHandle.getWorldPos2D(), (f32)tileContainer.getWorldPos3D().z - 0.005f);
//        }
//
//        tileContainer.setOwnedTile(tileIndex);
//        if (!mBlueprint.walls[tileIndex].isEmpty()) {
//            tileContainer.setWallsAt(tileIndex, mBlueprint.walls[tileIndex]);
//        }
//
//        // TODO: Stairs
//        if (bpTile.type != BlueprintTileType::STAIRS) {
//            tileContainer.setTileLayer(tileHandle.tileIndex, (TileLayer)tileData.layer, tileId);
//        }
//    }
//
//    // Notify blueprint
//    bpTile.isBuilt = true;
//    ++mBlueprint.tilesBuilt;
//
//    if (mTargetTiles.empty()) {
//        mState = BuildTaskState::SUCCESS;
//    }
//    else {
//        pathToBlueprint(registry, agent);
//    }
//}

BuildBlueprintTask::BuildBlueprintTask(BuildingBlueprint& blueprint, AgentTaskFinishedFunc finishedFunc)
    : mBlueprint(blueprint)
    , IAgentTask(finishedFunc) {

}

BuildBlueprintTask::~BuildBlueprintTask() {

}

TaskTickResult BuildBlueprintTask::tick(entt::registry& registry, entt::entity agent) {
    constexpr f32 BUILD_PER_TICK = 1.0f / 100.0f;
    switch (mState) {
        case TaskState::SELECT_TILE_TO_FILL:
            if (!selectTileToFill(registry, agent)) {
                mState = TaskState::SELECT_TILE_TO_BUILD;
            }
            break;
        case TaskState::SELECT_TILE_TO_BUILD:
            if (!selectTileToBuild(registry, agent)) {
                return TaskTickResult::SUCCESS;
            }
            break;
        case TaskState::PATH_TO_TILE:
            // Waiting callback
            break;
        case TaskState::PLACE_ITEMS:
            placeItemsOnTile(registry, agent);
            break;
        case TaskState::BUILD_TILE: {
            assert(mBuildTilesTarget);
            if (mBuildTilesTarget->tick(BUILD_PER_TICK)) {
                mBuildTilesTarget.reset();
                mState = TaskState::SELECT_TILE_TO_BUILD;
            }
            break;
        }
        case TaskState::FAIL:
            return TaskTickResult::FAIL;
        default:
            break;
    }
    return TaskTickResult::IN_PROGRESS;
}

bool BuildBlueprintTask::selectTileToFill(entt::registry& registry, entt::entity agent) {
    assert(!mPlaceTilesTarget);

    InventoryComponent& invCmp = registry.get<InventoryComponent>(agent);
    std::vector<ItemStack>& workingStorage = invCmp.getMutableWorkingStorage(WorkStorageID::HAULING);
    // Find items in our inventory to build with
    for (ItemStack& s : workingStorage) {
        if (mPlaceTilesTarget = mBlueprint.reserveTileToPlaceItems(s.id, s.quantity)) {
            break;
        }
    }

    if (!mPlaceTilesTarget) {
        // Nothing to build!
        return false;
    }
    assert(mPlaceTilesTarget->isValid());

    PhysicsComponent& physCmp = registry.get<PhysicsComponent>(agent);
    NavigationComponent& cmp = registry.get_or_emplace<NavigationComponent>(agent);
    const f32v3 targetWorldPos = mPlaceTilesTarget->mBlueprint->getTileWorldPos(mPlaceTilesTarget->mTileIndex);
    cmp.requestCoarsePath(physCmp.getPosition(), targetWorldPos, [this](bool success) {
        if (success) {
            mState = TaskState::PLACE_ITEMS;
        }
        else {
            // Path failed, lets try again
            ++mErrorCount;
            if (mErrorCount >= MAX_ERROR_COUNT_BEFORE_FAIL) {
                LOG_DEBUG("BuildBlueprintTask path to place items failed, error count {} - RESULT FAILURE");
                mState = TaskState::FAIL;
            }
            else {
                LOG_DEBUG("BuildBlueprintTask path to place items failed, error count {} - RESULT RETRY");
                mState = TaskState::SELECT_TILE_TO_FILL;
                mPlaceTilesTarget.reset();
            }
        }
    });
    mState = TaskState::PATH_TO_TILE;

    return true;
}

bool BuildBlueprintTask::selectTileToBuild(entt::registry& registry, entt::entity agent) {

    PhysicsComponent& physCmp = registry.get<PhysicsComponent>(agent);
    const f32v3 position = physCmp.getPosition();
    mBuildTilesTarget = mBlueprint.reserveTileToBuild(agent, position);
    if (!mBuildTilesTarget) {
        return false;
    }

    NavigationComponent& cmp = registry.get_or_emplace<NavigationComponent>(agent);
    const f32v3 targetWorldPos = mBuildTilesTarget->mBlueprint->getTileWorldPos(mBuildTilesTarget->mTileIndex);
    cmp.requestCoarsePath(position, targetWorldPos, [this](bool success) {
        if (success) {
            mState = TaskState::BUILD_TILE;
        }
        else {
            // Path failed, lets try again
            ++mErrorCount;
            if (mErrorCount >= MAX_ERROR_COUNT_BEFORE_FAIL) {
                LOG_DEBUG("BuildBlueprintTask path to build tile failed, error count {} - RESULT FAILURE");
                mState = TaskState::FAIL;
            }
            else {
                LOG_DEBUG("BuildBlueprintTask path to build tile failed, error count {} - RESULT RETRY");
                mState = TaskState::SELECT_TILE_TO_BUILD;
                mPlaceTilesTarget.reset();
            }
        }
    });
    mState = TaskState::PATH_TO_TILE;

    return true;
}

void BuildBlueprintTask::placeItemsOnTile(entt::registry& registry, entt::entity agent) {
    assert(mPlaceTilesTarget);
    assert(mPlaceTilesTarget->isValid());

    InventoryComponent& invCmp = registry.get<InventoryComponent>(agent);
    std::vector<ItemStack>& workingStorage = invCmp.getMutableWorkingStorage(WorkStorageID::HAULING);
    size_t selectedStackIndex = UINT32_MAX;
    for (size_t i = 0; i < workingStorage.size(); ++i) {
        if (workingStorage[i].id == mPlaceTilesTarget->mItemId) {
            selectedStackIndex = i;
            break;
        }
    }
    // We got pickpocketed???
    assert(selectedStackIndex != UINT32_MAX);
    ItemStack& sourceStack = workingStorage[selectedStackIndex];
    mPlaceTilesTarget->fulfillFromItemStack(sourceStack);
    mPlaceTilesTarget.reset();
    if (sourceStack.quantity == 0) {
        workingStorage[selectedStackIndex] = workingStorage.back();
        workingStorage.pop_back();
    }

    if (workingStorage.size()) {
        // We may still have items to place
        mState = TaskState::SELECT_TILE_TO_FILL;
    }
    else {
        // No items to place
        mState = TaskState::SELECT_TILE_TO_BUILD;
    }
}
