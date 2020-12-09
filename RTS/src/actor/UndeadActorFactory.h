#pragma once
#include "IActorFactory.h"

class UndeadActorFactory : public IActorFactory {
public:
	UndeadActorFactory(entt::registry& registry, World& world, ResourceManager& resourceManager);

	entt::entity createActor(const f32v2& position, const vio::Path& texturePath, const vio::Path& definitionFile) override;
};

