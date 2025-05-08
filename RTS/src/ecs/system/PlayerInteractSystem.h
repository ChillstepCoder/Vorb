#pragma once

class World;

// Tracks interactables for the player and 
// spatially partitions them. Handles marking
// entities for display and such
class PlayerInteractSystem {
public:
    PlayerInteractSystem(World& world);

    void update(entt::registry& registry, f32 elapsedSec, entt::entity localPlayer);

private:
    World& mWorld;
};

