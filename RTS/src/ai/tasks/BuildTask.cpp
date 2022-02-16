#include "stdafx.h"

#include "BuildTask.h"

#include <boost/pool/singleton_pool.hpp>

struct build_pool {};
using singleton_task_pool = boost::singleton_pool<build_pool, sizeof(BuildTask)>;

BuildTask::BuildTask(BuildingBlueprint& blueprint, std::vector<std::unique_ptr<ItemReservation>>&& sourceItems, std::vector<ui16>&& targetTiles) : mSourceItems(std::move(sourceItems)), mTargetTiles(std::move(targetTiles)), mBlueprint(blueprint) {
    assert(mSourceItems.size());
}

BuildTask::~BuildTask()
{

}

bool BuildTask::tick(World& world, entt::registry& registry, entt::entity agent) {
    switch (mState) {
        case BuildTaskState::FULFILL_RESERVATIONS: {
            //ItemStockpile* targetStockpile = mSourceItems[0]->mStockpile
        }break;
        case BuildTaskState::PATH_TO_STOCKPILE:
            break;
        case BuildTaskState::GRAB_RESOURCES:
            break;
        case BuildTaskState::PATH_TO_BLUEPRINT:
            break;
        case BuildTaskState::BUILD:
            break;
        case BuildTaskState::SUCCESS:
            break;
        case BuildTaskState::FAIL:
            break;
        default:
            break;

    }
    return false;
}

void* BuildTask::operator new(size_t count) {
    UNUSED(count);
    return singleton_task_pool::malloc();
}

void BuildTask::operator delete(void* pointer, size_t size) {
    UNUSED(size);
    return singleton_task_pool::free(pointer);
}