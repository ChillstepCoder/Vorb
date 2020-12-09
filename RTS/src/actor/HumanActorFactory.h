#pragma once
#include "IActorFactory.h"

class HumanActorFactory : public IActorFactory {
public:
	
	HumanActorFactory(entt::registry& registry, World& world, ResourceManager& resourceManager);
	entt::entity createActor(const f32v2& position, const vio::Path& texturePath, const vio::Path& definitionFile) override;
};

