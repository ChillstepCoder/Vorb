#pragma once
#include <Vorb/ecs/Entity.h>

DECL_VIO(class Path);
DECL_VG(class TextureCache);

class EntityComponentSystem;

class IActorFactory {
public:
	IActorFactory() = delete;
	IActorFactory(EntityComponentSystem& ecs, vg::TextureCache& textureCache) : mEcs(ecs), mTextureCache(textureCache) { };
	virtual ~IActorFactory() = default;

	// TODO??
	virtual vecs::EntityID createActor(const f32v2& position, const vio::Path& texturePath, const vio::Path& definitionFile) = 0;

protected:
	EntityComponentSystem& mEcs;
	vg::TextureCache& mTextureCache;
};
