#pragma once

#include "ecs/IEntityComponentSystem.h"

class CliEntityComponentSystem : public IEntityComponentSystem
{
public:

private:
    std::unordered_map<entt::entity, entt::entity> mSrvToCliEntityLookup;
};

