#pragma once

#include "ecs/IEntityComponentSystem.h"


class CliEntityComponentSystem : public IEntityComponentSystem
{
public:
    CliEntityComponentSystem(IWorld& world) : IEntityComponentSystem(world) {};

    // Begin IEntityComponentSystem interface
    entt::entity createEntity(const f32v3& position, StrToken typeToken, bool shouldReplicate) override;
    void destroyEntity(entt::entity entity) override;
    // End IEntityComponentSystem interface

    entt::entity createEntityFromSrv(entt::entity srvEntity, const f32v3& position, StrToken typeToken);
    void destroyEntityFromSrv(entt::entity srvEntity);

    entt::entity getEntityFromSrvEntity(entt::entity srvEntity);

private:
    std::unordered_map<entt::entity, entt::entity> mSrvToCliEntityLookup;
};

