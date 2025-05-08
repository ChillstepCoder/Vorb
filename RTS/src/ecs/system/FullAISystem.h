#pragma once

class World;
struct DualTaskQueueComponent;

class FullAISystem {
public:
    FullAISystem(World& world, entt::registry& registry);

    void update(f32 elapsedSec);

private:
    void updateCharacter(entt::entity entity);
    void updateTask(entt::entity entity, DualTaskQueueComponent& taskCmp);

    World& mWorld;
    entt::registry& mRegistry;
    f32 mElapsedSec = 0.0f;
};

