#pragma once

#include "ecs/ChunkFullActivateData.h"

enum class FullECSEventType {
    EntityDeactivated
};
struct FullECSEvent {
    entt::entity entity;
    ChunkID chunkId;
    union {
        EntityFullDeactivateData deactivateData;
    };
};
EVENT_DISPATCHER_TYPE(IEntityComponentSystem, FullECSEventType, FullECSEvent e);