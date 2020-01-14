#pragma once
#include "HumanActorFactory.h"

class PlayerActorFactory : public HumanActorFactory {
public:
	PlayerActorFactory(EntityComponentSystem& ecs, vg::TextureCache& textureCache);
	vecs::EntityID createActor(const f32v2& position, const vio::Path& texturePath, const vio::Path& definitionFile) override;
};

