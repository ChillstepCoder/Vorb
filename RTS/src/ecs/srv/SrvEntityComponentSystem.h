#pragma once

#include "ecs/IEntityComponentSystem.h"

class FullAISystem;

class SrvEntityComponentSystem : public IEntityComponentSystem
{
public:
    SrvEntityComponentSystem(World& world);
    ~SrvEntityComponentSystem();

    void tick(f32 elapsedSec) override;

    // Begin IEntityComponentSystem interface
	entt::entity createEntity(const f32v3& position, StrToken typeToken, bool shouldReplicate) override;
    void destroyEntity(entt::entity entity) override;
    // End IEntityComponentSystem interface

    entt::entity createPlayerEntity(int clientIndex, const f32v3& position);

protected:

    std::unique_ptr<FullAISystem> mFullAISystem;
    NavigationSystem mNavigationSystem;

    // City stuff
    BusinessSystem mBusinessSystem;

};

