#include "stdafx.h"
#include "PathToTargetTask.h"

#include "ecs/component/NavigationComponent.h"
#include "ecs/component/PhysicsComponent.h"
#include "ecs/component/CharacterControlComponent.h"

#include <boost/pool/singleton_pool.hpp>

struct ship_pool {};
using singleton_task_pool = boost::singleton_pool<ship_pool, sizeof(PathToTargetTask), boost::default_user_allocator_new_delete, boost::details::pool::null_mutex, 64u>;

PathToTargetTask::PathToTargetTask(TileHandle targetPosition, f32 completionRadius, AgentTaskFinishedFunc finishedFunc)
    : mTargetHandle(targetPosition)
    , mCompletionRadiusSQ(SQ(completionRadius))
    , IAgentTask(finishedFunc) {
    assert(mTargetHandle.isValid());
    mTargetPosition = mTargetHandle.getWorldPos3D();
}

PathToTargetTask::~PathToTargetTask() {

}

void* PathToTargetTask::operator new(size_t count) {
    ASSERT_GAME_THREAD();
    UNUSED(count);
    return singleton_task_pool::malloc();
}

void PathToTargetTask::operator delete(void* pointer, size_t size) {
    ASSERT_GAME_THREAD();
    UNUSED(size);
    return singleton_task_pool::free(pointer);
}

TaskTickResult PathToTargetTask::tick(World& world, entt::registry& registry, entt::entity agent) {
    switch (mState) {
        case TaskState::NEEDS_PATH: {
            const PhysicsComponent& physCmp = registry.get<PhysicsComponent>(agent);
            NavigationComponent& navCmp = registry.get_or_emplace<NavigationComponent>(agent);
            mState = TaskState::PATHING;
            navCmp.requestCoarsePath(physCmp.getPosition(), mTargetPosition, [this](bool success) {
                mState = success ? TaskState::SUCCESS : TaskState::FAIL;
            });
            break;
        }
        case TaskState::PATHING: {
            // TODO: can we just rely on a navcmp callback and give nav cmp a distance check?
            const PhysicsComponent& physCmp = registry.get<PhysicsComponent>(agent);
            const f32 distanceSQ = glm::length2(physCmp.getPosition() - mTargetPosition);
            if (distanceSQ <= mCompletionRadiusSQ) {
                NavigationComponent* navCmp = registry.try_get<NavigationComponent>(agent);
                if (navCmp) {
                    navCmp->abort(registry.get<CharacterControlComponent>(agent));
                    mState = TaskState::SUCCESS;
                }
                return TaskTickResult::SUCCESS;
            }
            break;
        }
        case TaskState::FAIL:
            return TaskTickResult::FAIL;
        case TaskState::SUCCESS:
            return TaskTickResult::SUCCESS;
        default:
            break;
    }
    return TaskTickResult::IN_PROGRESS;
}
