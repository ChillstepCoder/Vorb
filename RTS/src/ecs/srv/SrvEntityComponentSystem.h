#pragma once

#include "ecs/IEntityComponentSystem.h"

class SrvEntityComponentSystem : public IEntityComponentSystem
{
public:

	entt::entity createEntity(const f32v3& position, StrToken typeToken, bool shouldReplicate) override;
	entt::entity createEntityFromSrv(entt::entity srvEntity, const f32v3& position, StrToken typeToken) override;
	void destroyEntity(entt::entity entity) override;
	void destroyEntityFromSrv(entt::entity entity) override { assert(false); }

};

