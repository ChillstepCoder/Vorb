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
        case TaskState::FLATTEN_TERRAIN:
            if (tryFlattenTerrain(registry, agent)) {
                mState = TaskState::PLACE_ITEMS;
                break; // Delay one tick on success
            }
            mState = TaskState::PLACE_ITEMS;
            [[fallthrough]];
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
            f32v3 pos = mPlaceTilesTarget->mBlueprint->getTileWorldPos(mPlaceTilesTarget->mTileIndex);
            //LOG_DEBUG("FOUND TILE {} AT {}, {}, {}", mPlaceTilesTarget->mTileIndex, pos.x, pos.y, pos.z);
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
    // TODO: Fallback to coarse path?
    cmp.requestFinePath(physCmp.getPosition(), targetWorldPos, [this](bool success) {
        if (success) {
            // We always check for flatten terrain first
            mState = TaskState::FLATTEN_TERRAIN;
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

bool BuildBlueprintTask::tryFlattenTerrain(entt::registry& registry, entt::entity agent) {
    assert(mPlaceTilesTarget);
    assert(mPlaceTilesTarget->isValid());

    // Only first floor
    if (mPlaceTilesTarget->mTileIndex >= mBlueprint.aabb.dims.x * mBlueprint.aabb.dims.y) {
        return false;
    }

    if (mBlueprint.tilesNeedingTerrainFlatten.getBit(mPlaceTilesTarget->mTileIndex)) {
        mBlueprint.tilesNeedingTerrainFlatten.clearBit(mPlaceTilesTarget->mTileIndex);

        IHeightmapGrid& grid = sWorld->getHeightmapGrid();
        f32v3 tileWorldPos = mBlueprint.getTileWorldPos(mPlaceTilesTarget->mTileIndex);
        grid.setHeightAt(tileWorldPos, mBlueprint.mDesiredTerrainFlattenHeight);
        return true;
    }
    return false;
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
