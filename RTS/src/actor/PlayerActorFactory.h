#pragma once
#include "HumanActorFactory.h"

class World;

class PlayerActorFactory : public HumanActorFactory {
public:
	PlayerActorFactory(entt::registry& registry, World& world, ResourceManager& resourceManager);
	entt::entity createActor(const f32v2& position, const vio::Path& texturePath, const vio::Path& definitionFile) override;
};

