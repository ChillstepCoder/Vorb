#pragma once

class World;

class FullAISystem {
public:
    FullAISystem(World& world, entt::registry& registry);

    void update(f32 elapsedSec);

private:
    void updateCharacter(entt::entity entity);

    World& mWorld;
    entt::registry& mRegistry;
};

