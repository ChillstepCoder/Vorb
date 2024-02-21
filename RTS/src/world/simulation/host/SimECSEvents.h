#pragma once

#include "SimEntityType.h"

enum class SimECSEventType {
    EntityCreated,
    EntityDestroyed
};
struct SimECSEvent {
    entt::entity entity;
    SimEntityType type = SimEntityType::INVALID;
};
EVENT_DISPATCHER_TYPE(SimECS, SimECSEventType, SimECSEvent e);