#pragma once

DECL_VIO(class Path);

class ResourceManager;
class World;

class IActorFactory {
public:
	IActorFactory() = delete;
	IActorFactory(entt::registry& registry, World& world, ResourceManager& resourceManager) : mRegistry(registry), mWorld(world), mResourceManager(resourceManager) { };
	virtual ~IActorFactory() = default;

	// TODO??
	virtual entt::entity createActor(const f32v2& position, const vio::Path& texturePath, const vio::Path& definitionFile) = 0;

protected:
    entt::registry& mRegistry;
    World& mWorld;
	ResourceManager& mResourceManager;
};
