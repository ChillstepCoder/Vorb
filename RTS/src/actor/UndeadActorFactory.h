#pragma once
#include "IActorFactory.h"

class UndeadActorFactory : public IActorFactory {
public:
	UndeadActorFactory(EntityComponentSystem& ecs, ResourceManager& resourceManager);

	vecs::EntityID createActor(const f32v2& position, const vio::Path& texturePath, const vio::Path& definitionFile) override;
};

