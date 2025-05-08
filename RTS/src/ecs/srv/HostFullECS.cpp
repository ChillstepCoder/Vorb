#include "stdafx.h"
#include "HostFullECS.h"

#include "ecs/factory/EntityFactory.h"
#include "ecs/component/ReplicationComponent.h"
#include "ecs/component/EntityDetailsComponent.h"

#include "network/srv/GameServerOLD.h"
#include "network/srv/SrvMessage.h"

#include "ecs/system/FullAISystem.h"

HostFullECS::HostFullECS(World& world) : IFullECS(world) {
    mFullAISystem = std::make_unique<FullAISystem>(world, mRegistry);
}

HostFullECS::~HostFullECS() {

}

void HostFullECS::tick(f32 elapsedSec) {
    PROFILE_FUNCTION();
    IFullECS::tick(elapsedSec);
    mBusinessSystem.update(mRegistry);
    mFullAISystem->update(elapsedSec);
    mNavigationSystem.update(mWorld, mRegistry);
}

entt::entity HostFullECS::createEntity(const f32v3& position, StrToken typeToken, bool shouldReplicate) {
    ASSERT_GAME_THREAD();
    entt::entity newEntity = EntityFactory::createEntity(mWorld, position, typeToken);
    LOG_CRITICAL("Create Entity {} in {}", (int)newEntity, mRegistry.get<PositionComponent>(newEntity).chunkId);

    if (shouldReplicate && GameServerOLD::exists()) {
        mRegistry.emplace<ReplicationComponent>(newEntity);
        SrvMessage::sendEntityCreateMessageToAll(newEntity, typeToken, position, 0.0f);
        // Details are only used for replication right now
        EntityDetailsComponent& detailsCmp = mRegistry.emplace<EntityDetailsComponent>(newEntity);
        detailsCmp.mEntityToken = typeToken;
    }
    return newEntity;
}

entt::entity HostFullECS::createPlayerEntity(int clientIndex, const f32v3& position) {
    ASSERT_GAME_THREAD();
    entt::entity entity = EntityFactory::createEntity(mWorld, position, CStrToken("player"));

    if (GameServerOLD::exists()) {
        ReplicationComponent& repCmp = mRegistry.emplace<ReplicationComponent>(entity);
        SrvMessage::sendClientBeginMessageToAll(clientIndex, entity, position, 0.0f);

        // Don't replicate to the owning player
        if (clientIndex != CLIENT_INDEX_HOST) {
            repCmp.setReplicateToClient(clientIndex, false);
        }

        // Details are only used for replication right now
        EntityDetailsComponent& detailsCmp = mRegistry.emplace<EntityDetailsComponent>(entity);
        detailsCmp.mEntityToken = CStrToken("player");
    }
    return entity;
}


void HostFullECS::destroyEntity(entt::entity entity) {
    EntityFactory::destroyEntity(mWorld, entity);
}

