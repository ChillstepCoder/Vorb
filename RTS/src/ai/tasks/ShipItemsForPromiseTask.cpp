#include "stdafx.h"
#include "ShipItemsForPromiseTask.h"

#include <boost/pool/singleton_pool.hpp>

struct ship_pool {};
using singleton_task_pool = boost::singleton_pool<ship_pool, sizeof(ShipItemsForPromiseTask), boost::default_user_allocator_new_delete, boost::details::pool::null_mutex, 64u>;

ShipItemsForPromiseTask::ShipItemsForPromiseTask(ItemPromiseWeakPtr&& itemPromise, TileHandle targetPosition, f32 completionRadius)
    : mItemPromise(std::move(itemPromise))
    , mTargetPosition(targetPosition)
    , mCompletionRadiusSQ(SQ(completionRadius)) {

}

ShipItemsForPromiseTask::~ShipItemsForPromiseTask() {

}

void* ShipItemsForPromiseTask::operator new(size_t count) {
    assert(IS_GAME_THREAD());
    UNUSED(count);
    return singleton_task_pool::malloc();
}

void ShipItemsForPromiseTask::operator delete(void* pointer, size_t size) {
    assert(IS_GAME_THREAD());
    UNUSED(size);
    return singleton_task_pool::free(pointer);
}

bool ShipItemsForPromiseTask::tick(entt::registry& registry, entt::entity agent) {
    throw std::logic_error("The method or operation is not implemented.");
}
