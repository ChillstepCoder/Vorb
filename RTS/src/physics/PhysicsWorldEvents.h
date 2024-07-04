#pragma once

enum class PhysicsWorldEventType {
    INVALID,
    ItemAtRest
};
struct PhysicsWorldEvent {
    entt::entity entity;
    PhysicsWorldEventType type = PhysicsWorldEventType::INVALID;
};
EVENT_DISPATCHER_TYPE(PhysicsWorld, PhysicsWorldEventType, PhysicsWorldEvent e);