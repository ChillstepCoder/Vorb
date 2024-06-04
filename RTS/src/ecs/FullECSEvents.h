#pragma once

#include "ecs/ChunkFullTransitionData.h"

enum class FullECSEventType {
    EntityDeactivated
};
struct FullECSEvent {
    entt::entity entity;
    ChunkID chunkId;
};
EVENT_DISPATCHER_TYPE(IEntityComponentSystem, FullECSEventType, FullECSEvent& e);