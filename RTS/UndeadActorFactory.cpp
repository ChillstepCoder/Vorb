#include "stdafx.h"
#include "actor/UndeadActorFactory.h"

#include "EntityComponentSystem.h"

#include <Vorb/graphics/TextureCache.h>
#include <Vorb/ecs/ECS.h>

UndeadActorFactory::UndeadActorFactory(EntityComponentSystem& ecs, vg::TextureCache& textureCache)
	: IActorFactory(ecs)
	, mTextureCache(textureCache) {
}

vecs::EntityID UndeadActorFactory::createActor(const f32v2& position, const vio::Path& texturePath, const vio::Path& definitionFile) {
	UNUSED(definitionFile);

	VGTexture texture = mTextureCache.addTexture(texturePath).id;

	// Create physics entity
	vecs::EntityID newEntity = mEcs.addEntity();
	auto physCompPair = static_cast<EntityComponentSystem&>(mEcs).addPhysicsComponent(newEntity);
	auto& physComp = physCompPair.second;
	physComp.mPosition = position;
	physComp.mVelocity = f32v2(0.0f);
	physComp.mMass = 1.0f;
	physComp.mGravity = 1.0f;
	physComp.mCollisionRadius = 15.0f;
	physComp.mFrictionCoef = 0.97f;
	physComp.mFlags = 0;

	auto spriteCompPair = static_cast<EntityComponentSystem&>(mEcs).addSimpleSpriteComponent(newEntity);
	auto& spriteComp = spriteCompPair.second;
	spriteComp.physicsComponent = physCompPair.first;
	spriteComp.texture = texture;
	spriteComp.dims = f32v2(30.0f);

	return newEntity;
}

vecs::EntityID UndeadActorFactory::createActorWithVelocity(const f32v2& position, const f32v2& velocity, const vio::Path& texturePath, const vio::Path& definitionFile) {
	vecs::EntityID id = createActor(position, texturePath, definitionFile);

	auto& physComp = static_cast<EntityComponentSystem&>(mEcs).getPhysicsComponent(id);
	physComp.mVelocity = velocity;

	return id;
}
