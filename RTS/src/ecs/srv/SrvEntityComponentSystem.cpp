#include "stdafx.h"
#include "SrvEntityComponentSystem.h"

#include "ecs/factory/EntityFactory.h"
#include "ecs/component/ReplicationComponent.h"

#include "network/srv/GameServer.h"

entt::entity SrvEntityComponentSystem::createEntity(const f32v3& position, const nString& typeName, bool shouldReplicate)
{
    entt::entity newEntity = EntityFactory::createEntity(position, typeName);
    if (shouldReplicate) {
        mRegistry.emplace<ReplicationComponent>(newEntity);

    }
}

entt::entity SrvEntityComponentSystem::createEntityFromSrv(entt::entity srvEntity, const f32v3& position, const nString& typeName)
{
    throw std::logic_error("The method or operation is not implemented.");
}

void SrvEntityComponentSystem::destroyEntity(entt::entity entity)
{
    
}

