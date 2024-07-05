#pragma once

enum class PhysicsWorldEventType {
    INVALID,
    ItemAtRest,
    ItemMoved
};
struct PhysicsWorldEvent {
    entt::entity entity;
};
EVENT_DISPATCHER_TYPE(PhysicsWorld, PhysicsWorldEventType, PhysicsWorldEvent e);