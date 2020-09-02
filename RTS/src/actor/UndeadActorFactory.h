#pragma once
#include "IActorFactory.h"

class UndeadActorFactory : public IActorFactory {
public:
	UndeadActorFactory(EntityComponentSystem& ecs, vg::TextureCache& textureCache);

	vecs::EntityID createActor(const f32v2& position, const vio::Path& texturePath, const vio::Path& definitionFile) override;
};
