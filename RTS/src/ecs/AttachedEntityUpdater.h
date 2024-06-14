#pragma once

#include "ecs/AttachedEntityUpdateHandle.h"

struct EntityAttachedUpdateComponent {
    std::vector<AttachedEntityUpdateHandlePtr> handles;
};

struct QueuedEntityUpdateAttach {
    // If position, chooses closest entity that does not already have the ID
    std::variant<entt::entity, f32v3> target;
    enum class AttachType {
        Character
    } type = AttachType::Character;
    AttachedEntityUpdateHandlePtr handle;
};

// Attaches to an entity and allows for thread safe operations to run on it per
// tick while it is alive. Non owning reference.
// Owned by ECS systems.
// Primarily used for debugging or one off actions, as it is not as efficient as a normal system
// update and requires a lock per entity
class AttachedEntityUpdater {
    friend class IFullECS;
    friend class ISimECS;
public:
    ~AttachedEntityUpdater();
private:
    AttachedEntityUpdater();

    // Called by owner ECS.
    void update(entt::registry& registry);

    void queueAttachUpdate(QueuedEntityUpdateAttach attachUpdate);

    void registerHandleForEntity(entt::registry& registry, entt::entity entity, AttachedEntityUpdateHandlePtr handle);
    void unregisterHandleForEntity(entt::registry& registry, entt::entity entity, AttachedEntityUpdateTypeID type);
    void unregisterAllHandlesForEntity(entt::registry& registry, entt::entity entity);
private:
    void proccessQueuedUpdateAttach(QueuedEntityUpdateAttach& queuedUpdate, entt::registry& registry);

    moodycamel::ConcurrentQueue<QueuedEntityUpdateAttach> mQueuedUpdates;

    std::thread::id mOwnerThreadID;
    std::map<entt::entity, std::vector<AttachedEntityUpdateHandlePtr>> mEntityHandleLists;
};