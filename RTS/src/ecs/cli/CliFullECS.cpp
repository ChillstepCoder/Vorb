#include "stdafx.h"
#include "ClIFullECS.h"

#include "ecs/factory/EntityFactory.h"

entt::entity CliFullECS::createEntity(const f32v3& position, StrToken typeToken, bool shouldReplicate) {
    ASSERT_GAME_THREAD();
    entt::entity newEntity = EntityFactory::createEntity(mWorld, position, typeToken);
    mEntitiesByChunk[mRegistry.get<PositionComponent>(newEntity).chunkId].emplace_back(newEntity);
    return newEntity;
}

entt::entity CliFullECS::createEntityFromSrv(entt::entity srvEntity, const f32v3& position, StrToken typeToken) {
    entt::entity newEntity = EntityFactory::createEntity(mWorld, position, typeToken);
    assert(mSrvToCliEntityLookup.find(srvEntity) == mSrvToCliEntityLookup.end());
    mEntitiesByChunk[mRegistry.get<PositionComponent>(newEntity).chunkId].emplace_back(newEntity);
    mSrvToCliEntityLookup[srvEntity] = newEntity;
    return newEntity;
}

void CliFullECS::destroyEntity(entt::entity entity) {
    // Client cannot destroy server entities
    assert(mSrvToCliEntityLookup.find(entity) == mSrvToCliEntityLookup.end());
    mRegistry.destroy(entity);

    // TODO: FIX ALL THIS SHIT, NEED TO CALL THIS FROM Srv LIKE EFFECT CONTEXT
    assert(false);
}

void CliFullECS::destroyEntityFromSrv(entt::entity srvEntity) {
    auto&& it = mSrvToCliEntityLookup.find(srvEntity);
    if (it != mSrvToCliEntityLookup.end()) {
        mRegistry.destroy(it->second);
        mSrvToCliEntityLookup.erase(it);
    }
    else {
        std::cout << "Failed to destroy a server entity\n";
    }
}

entt::entity CliFullECS::getEntityFromSrvEntity(entt::entity srvEntity) {
    auto&& it = mSrvToCliEntityLookup.find(srvEntity);
    if (it != mSrvToCliEntityLookup.end()) {
        return it->second;
    }
    return entt::null;
}
