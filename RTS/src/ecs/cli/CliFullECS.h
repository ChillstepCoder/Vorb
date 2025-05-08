#pragma once

#include "ecs/IFullECS.h"


class CliFullECS : public IFullECS
{
public:
    CliFullECS(World& world) : IFullECS(world) {};

    // Begin IFullECS interface
    entt::entity createEntity(const f32v3& position, StrToken typeToken, bool shouldReplicate) override;
    void destroyEntity(entt::entity entity) override;
    // End IFullECS interface

    entt::entity createEntityFromSrv(entt::entity srvEntity, const f32v3& position, StrToken typeToken);
    void destroyEntityFromSrv(entt::entity srvEntity);

    entt::entity getEntityFromSrvEntity(entt::entity srvEntity);

private:
    UnorderedFlatMap<entt::entity, entt::entity> mSrvToCliEntityLookup;
};

