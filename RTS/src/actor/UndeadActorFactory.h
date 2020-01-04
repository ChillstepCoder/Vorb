#pragma once
#include "IActorFactory.h"

DECL_VG(class TextureCache);
class EntityComponentSystem;

class UndeadActorFactory : public IActorFactory {
public:
	UndeadActorFactory(EntityComponentSystem& ecs, vg::TextureCache& textureCache);

	vecs::EntityID createActor(const f32v2& position, const vio::Path& texturePath, const vio::Path& definitionFile) /*override*/;

	vecs::EntityID createActorWithVelocity(const f32v2& position, const f32v2& velocity, const vio::Path& texturePath, const vio::Path& definitionFile);

private:
	vg::TextureCache& mTextureCache;
};

