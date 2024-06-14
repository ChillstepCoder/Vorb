#include "stdafx.h"
#include "AttachedEntityUpdater.h"

#include "ecs/component/PositionComponent.h"
#include "world/simulation/host/component/SimCharacterComponents.h"

AttachedEntityUpdater::AttachedEntityUpdater() {
    mOwnerThreadID = std::this_thread::get_id();
}

AttachedEntityUpdater::~AttachedEntityUpdater() {

}

void AttachedEntityUpdater::update(entt::registry& registry) {
    assert(mOwnerThreadID == std::this_thread::get_id());

    // Update tasks queued from other threads
    constexpr size_t BULK_DEQUEUE_SIZE = 256;
    QueuedEntityUpdateAttach queuedUpdates[BULK_DEQUEUE_SIZE];
    size_t numHandles = mQueuedUpdates.try_dequeue_bulk(queuedUpdates, BULK_DEQUEUE_SIZE);
    for (size_t i = 0; i < numHandles; i++) {
        QueuedEntityUpdateAttach& queuedUpdate = queuedUpdates[i];
        proccessQueuedUpdateAttach(queuedUpdate, registry);
    }

    // Update all entities
    auto view = registry.view<EntityAttachedUpdateComponent>();
    std::vector<std::pair<entt::entity, AttachedEntityUpdateTypeID>> toRemove;
    for (auto entity : view) {
        EntityAttachedUpdateComponent& cmp = view.get<EntityAttachedUpdateComponent>(entity);
        for (auto&& handle : cmp.handles) {
            assert(handle);
            if (handle->isValid()) {
                if (handle.use_count() == 2) {
                    // We are only referenced by AttachedEntityUpdater, so lets remove
                    toRemove.emplace_back(entity, handle->getUpdateTypeID());
                }
                else {
                    handle->mFunc(entity, registry, handle.get());
                }
            }
        }
    }

    // Remove all handles that are no longer needed
    for (auto&& [entity, type] : toRemove) {
        unregisterHandleForEntity(registry, entity, type);
    }
}

void AttachedEntityUpdater::queueAttachUpdate(QueuedEntityUpdateAttach attachUpdate) {
    assert(mOwnerThreadID != std::this_thread::get_id());
    assert(attachUpdate.handle);
    mQueuedUpdates.enqueue(std::move(attachUpdate));
}

void AttachedEntityUpdater::registerHandleForEntity(entt::registry& registry, entt::entity entity, AttachedEntityUpdateHandlePtr handle) {
    assert(mOwnerThreadID == std::this_thread::get_id());
    auto&& it = mEntityHandleLists.find(entity);
    if (it == mEntityHandleLists.end()) {
        assert(handle);
        registry.emplace<EntityAttachedUpdateComponent>(entity).handles.emplace_back(handle);
        mEntityHandleLists.emplace(entity, std::vector<AttachedEntityUpdateHandlePtr>{std::move(handle)});
    } else {
        it->second.emplace_back(std::move(handle));
    }
}

void AttachedEntityUpdater::unregisterHandleForEntity(entt::registry& registry, entt::entity entity, AttachedEntityUpdateTypeID type) {
    assert(mOwnerThreadID == std::this_thread::get_id());
    auto&& it = mEntityHandleLists.find(entity);
    if (it != mEntityHandleLists.end()) {
        auto&& handles = it->second;
        for (auto&& handleIt = handles.begin(); handleIt != handles.end(); ++handleIt) {
            if ((*handleIt)->getUpdateTypeID() == type) {
                EntityAttachedUpdateComponent& cmp = registry.get<EntityAttachedUpdateComponent>(entity);
                for (size_t i = 0; i < cmp.handles.size(); ++i) {
                    if (cmp.handles[i].get() == handleIt->get()) {
                        cmp.handles[i] = std::move(cmp.handles.back());
                        cmp.handles.pop_back();
                        if (cmp.handles.empty()) {
                            registry.remove<EntityAttachedUpdateComponent>(entity);
                        }
                        break;
                    }
                }
                handles.erase(handleIt);
                if (handles.empty()) {
                    mEntityHandleLists.erase(it);
                }
                return;
            }
        }
    }
}

void AttachedEntityUpdater::unregisterAllHandlesForEntity(entt::registry& registry, entt::entity entity) {
    assert(mOwnerThreadID == std::this_thread::get_id());
    if (registry.remove<EntityAttachedUpdateComponent>(entity)) {
        mEntityHandleLists.erase(entity);
    }
}


void AttachedEntityUpdater::proccessQueuedUpdateAttach(QueuedEntityUpdateAttach& queuedUpdate, entt::registry& registry) {
    if (std::holds_alternative<entt::entity>(queuedUpdate.target)) {
        registerHandleForEntity(registry, std::get<entt::entity>(queuedUpdate.target), std::move(queuedUpdate.handle));
    }
    else {
        // Find closest entity to position
        assert(queuedUpdate.type == QueuedEntityUpdateAttach::AttachType::Character); // TODO: allow others
        const f32v3 pos = std::get<f32v3>(queuedUpdate.target);
        entt::entity closestEntity = entt::null;
        f32 closestDistSQ = std::numeric_limits<f32>::max();

        auto canApplyToEntity = [&](entt::entity entity) {
            if (EntityAttachedUpdateComponent* updateCmp = registry.try_get<EntityAttachedUpdateComponent>(entity)) {
                // Ensure we only add one of each type
                for (auto&& handle : updateCmp->handles) {
                    if (handle->getUpdateTypeID() == queuedUpdate.handle->getUpdateTypeID()) {
                        return false;
                    }
                }
                return true;
            }
            return true;
        };

        if (IS_SIM_THREAD()) {
            const f32v2 pos2D(pos);
            registry.view<SimPositionComponent, DualCharacterComponent>().each([&](entt::entity entity, SimPositionComponent& posCmp, DualCharacterComponent&) {
                const f32 distSQ = glm::distance2(pos2D, posCmp.getPosition());
                if (distSQ < closestDistSQ) {
                    if (canApplyToEntity(entity)) {
                        closestDistSQ = distSQ;
                        closestEntity = entity;
                    }
                }
            });
        }
        else if (IS_GAME_THREAD()) {
            registry.view<PositionComponent, DualCharacterComponent>().each([&](entt::entity entity, PositionComponent& posCmp, DualCharacterComponent&) {
                const f32 distSQ = glm::distance2(pos, posCmp.mPosition);
                if (distSQ < closestDistSQ) {
                    if (canApplyToEntity(entity)) {
                        closestDistSQ = distSQ;
                        closestEntity = entity;
                    }
                }
            });
        }

        if (closestEntity != entt::null) {
            registerHandleForEntity(registry, closestEntity, std::move(queuedUpdate.handle));
        }
    }
}
