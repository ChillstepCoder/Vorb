#pragma once

#include "ecs/IEntityComponentSystem.h"

class SrvEntityComponentSystem : public IEntityComponentSystem
{
public:

	entt::entity createEntity(const f32v3& position, const nString& typeName, bool shouldReplicate) override;
	entt::entity createEntityFromSrv(entt::entity srvEntity, const f32v3& position, const nString& typeName) override;
	void destroyEntity(entt::entity entity) override;
	void destroyEntityFromSrv(entt::entity entity) override { assert(false); }

};

