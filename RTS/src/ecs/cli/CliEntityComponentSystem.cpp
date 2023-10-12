#include "stdafx.h"
#include "CliEntityComponentSystem.h"

#include "ecs/factory/EntityFactory.h"

entt::entity CliEntityComponentSystem::createEntity(const f32v3& position, StrToken typeToken, bool shouldReplicate) {
    ASSERT_GAME_THREAD();
    entt::entity newEntity = EntityFactory::createEntity(mWorld, position, typeToken);
    return newEntity;
}

entt::entity CliEntityComponentSystem::createEntityFromSrv(entt::entity srvEntity, const f32v3& position, StrToken typeToken) {
    entt::entity newEntity = EntityFactory::createEntity(mWorld, position, typeToken);
    assert(mSrvToCliEntityLookup.find(srvEntity) == mSrvToCliEntityLookup.end());
    mSrvToCliEntityLookup[srvEntity] = newEntity;
    return newEntity;
}

void CliEntityComponentSystem::destroyEntity(entt::entity entity) {
    // Client cannot destroy server entities
    assert(mSrvToCliEntityLookup.find(entity) == mSrvToCliEntityLookup.end());
    mRegistry.destroy(entity);
}

void CliEntityComponentSystem::destroyEntityFromSrv(entt::entity srvEntity) {
    auto&& it = mSrvToCliEntityLookup.find(srvEntity);
    if (it != mSrvToCliEntityLookup.end()) {
        mRegistry.destroy(it->second);
        mSrvToCliEntityLookup.erase(it);
    }
    else {
        std::cout << "Failed to destroy a server entity\n";
    }
}

entt::entity CliEntityComponentSystem::getEntityFromSrvEntity(entt::entity srvEntity) {
    auto&& it = mSrvToCliEntityLookup.find(srvEntity);
    if (it != mSrvToCliEntityLookup.end()) {
        return it->second;
    }
    return entt::null;
}
