#pragma once

#include "ecs/IFullECS.h"

class FullAISystem;

class HostFullECS : public IFullECS
{
public:
    HostFullECS(World& world);
    ~HostFullECS();

    void tick(f32 elapsedSec) override;

    // Begin IFullECS interface
	entt::entity createEntity(const f32v3& position, StrToken typeToken, bool shouldReplicate) override;
    void destroyEntity(entt::entity entity) override;
    // End IFullECS interface

    entt::entity createPlayerEntity(int clientIndex, const f32v3& position);

protected:

    std::unique_ptr<FullAISystem> mFullAISystem;
    NavigationSystem mNavigationSystem;

    // City stuff
    BusinessSystem mBusinessSystem;

};

