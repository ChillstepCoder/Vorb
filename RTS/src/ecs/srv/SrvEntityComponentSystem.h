#pragma once

#include "ecs/IEntityComponentSystem.h"

class SrvEntityComponentSystem : public IEntityComponentSystem
{
public:

    void tick() override;

    // Begin IEntityComponentSystem interface
	entt::entity createEntity(const f32v3& position, StrToken typeToken, bool shouldReplicate) override;
    void destroyEntity(entt::entity entity) override;
    // End IEntityComponentSystem interface

    entt::entity createPlayerEntity(int clientIndex, const f32v3& position);

protected:

    PersonAISystem mPersonAISystem;
    NavigationComponentSystem mNavigationSystem;

    // City stuff
    BusinessSystem mBusinessSystem;

};

