#include "stdafx.h"
#include "actor/UndeadActorFactory.h"

#include <Vorb/graphics/TextureCache.h>
#include "EntityComponentSystem.h"

UndeadActorFactory::UndeadActorFactory(EntityComponentSystem& ecs, vg::TextureCache& textureCache)
	: IActorFactory(ecs, textureCache) {
}

vecs::EntityID UndeadActorFactory::createActor(const f32v2& position, const vio::Path& texturePath, const vio::Path& definitionFile) {
	UNUSED(definitionFile);

	VGTexture texture = mTextureCache.addTexture(texturePath).id;

	// Create physics entity
	vecs::EntityID newEntity = mEcs.addEntity();
	auto physCompPair = mEcs.addPhysicsComponent(newEntity);
	auto& physComp = physCompPair.second;
	physComp.mPosition = position;
	physComp.mVelocity = f32v2(0.0f);
	physComp.mMass = 1.0f;
	physComp.mGravity = 1.0f;
	physComp.mCollisionRadius = 15.0f;
	physComp.mFrictionCoef = 0.9f;
	physComp.mFlags = 0;
	physComp.mQueryActorType = ACTORTYPE_UNDEAD;

	auto spriteCompPair = mEcs.addSimpleSpriteComponent(newEntity);
	auto& spriteComp = spriteCompPair.second;
	spriteComp.physicsComponent = physCompPair.first;
	spriteComp.texture = texture;
	spriteComp.dims = f32v2(30.0f);

	mEcs.addUndeadAIComponent(newEntity);
	auto& navCmp = mEcs.addNavigationComponent(newEntity).second;
	navCmp.speed = 4.0f;

	return newEntity;
}
