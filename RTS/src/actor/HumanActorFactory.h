#pragma once
#include "IActorFactory.h"

class HumanActorFactory : public IActorFactory {
public:
	
	HumanActorFactory(EntityComponentSystem& ecs, vg::TextureCache& textureCache);
	vecs::EntityID createActor(const f32v2& position, const vio::Path& texturePath, const vio::Path& definitionFile) override;
};

