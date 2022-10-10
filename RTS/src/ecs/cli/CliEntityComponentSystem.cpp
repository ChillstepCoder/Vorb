#include "stdafx.h"
#include "CliEntityComponentSystem.h"

entt::entity CliEntityComponentSystem::createEntity(const f32v3& position, StrToken typeToken, bool shouldReplicate)
{
    throw std::logic_error("The method or operation is not implemented.");
    return entt::entity();
}

entt::entity CliEntityComponentSystem::createEntityFromSrv(entt::entity srvEntity, const f32v3& position, StrToken typeToken)
{
    throw std::logic_error("The method or operation is not implemented.");
    return entt::entity();
}

void CliEntityComponentSystem::destroyEntity(entt::entity entity)
{
    throw std::logic_error("The method or operation is not implemented.");
}

void CliEntityComponentSystem::destroyEntityFromSrv(entt::entity entity)
{
    throw std::logic_error("The method or operation is not implemented.");
}
