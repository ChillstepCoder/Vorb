#pragma once
#include "IActorFactory.h"

class HumanActorFactory : public IActorFactory {
public:
	
	HumanActorFactory(EntityComponentSystem& ecs, ResourceManager& resourceManager);
	vecs::EntityID createActor(const f32v2& position, const vio::Path& texturePath, const vio::Path& definitionFile) override;
};

