#pragma once

class World;

struct ObjectPickupComponent {
    entt::entity picker;
    f32 speed = 5.0f; // Start with some velocity
};

class ObjectPickupSystem {
public:
    static void update(World& world, entt::registry& registry, f32 elapsedSec);
};