#include "stdafx.h"
#include "SrvEntityComponentSystem.h"

#include "ecs/factory/EntityFactory.h"
#include "ecs/component/ReplicationComponent.h"
#include "ecs/component/EntityDetailsComponent.h"

#include "network/srv/GameServer.h"
#include "network/srv/SrvMessage.h"

void SrvEntityComponentSystem::tick()
{
    PROFILE_FUNCTION();
    IEntityComponentSystem::tick();
    mBusinessSystem.update(mRegistry);
    mPersonAISystem.update(mRegistry);
    mNavigationSystem.update(mRegistry);
}

entt::entity SrvEntityComponentSystem::createEntity(const f32v3& position, StrToken typeToken, bool shouldReplicate) {
    assert(IS_GAME_THREAD());
    entt::entity newEntity = EntityFactory::createEntity(position, typeToken);
    if (shouldReplicate && GameServer::exists()) {
        mRegistry.emplace<ReplicationComponent>(newEntity);
        SrvMessage::sendEntityCreateMessageToAll(newEntity, typeToken, position, 0.0f);
        // Details are only used for replication right now
        EntityDetailsComponent& detailsCmp = mRegistry.emplace<EntityDetailsComponent>(newEntity);
        detailsCmp.mEntityToken = typeToken;
    }
    return newEntity;
}

entt::entity SrvEntityComponentSystem::createPlayerEntity(int clientIndex, const f32v3& position) {
    assert(IS_GAME_THREAD());
    entt::entity entity = EntityFactory::createEntity(position, StrToken("player"));

    if (GameServer::exists()) {
        ReplicationComponent& repCmp = mRegistry.emplace<ReplicationComponent>(entity);
        SrvMessage::sendClientBeginMessageToAll(clientIndex, entity, position, 0.0f);

        // Don't replicate to the owning player
        if (clientIndex != CLIENT_INDEX_HOST) {
            repCmp.setReplicateToClient(clientIndex, false);
        }

        // Details are only used for replication right now
        EntityDetailsComponent& detailsCmp = mRegistry.emplace<EntityDetailsComponent>(entity);
        detailsCmp.mEntityToken = StrToken("player");
    }
    return entity;
}


void SrvEntityComponentSystem::destroyEntity(entt::entity entity) {
    mRegistry.destroy(entity);
}

