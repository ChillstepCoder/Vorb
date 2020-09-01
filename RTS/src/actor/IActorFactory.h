#pragma once
#include <Vorb/ecs/Entity.h>

DECL_VIO(class Path);

class ResourceManager;
class EntityComponentSystem;

class IActorFactory {
public:
	IActorFactory() = delete;
	IActorFactory(EntityComponentSystem& ecs, ResourceManager& resourceManager) : mEcs(ecs), mResourceManager(resourceManager) { };
	virtual ~IActorFactory() = default;

	// TODO??
	virtual vecs::EntityID createActor(const f32v2& position, const vio::Path& texturePath, const vio::Path& definitionFile) = 0;

protected:
	EntityComponentSystem& mEcs;
	ResourceManager& mResourceManager;
};
