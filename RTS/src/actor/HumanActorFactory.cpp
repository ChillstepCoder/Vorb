#include "stdafx.h"
#include "actor/HumanActorFactory.h"

#include <Vorb/graphics/TextureCache.h>
#include "EntityComponentSystem.h"

HumanActorFactory::HumanActorFactory(EntityComponentSystem& ecs, vg::TextureCache& textureCache)
	: IActorFactory(ecs, textureCache) {
}

vecs::EntityID HumanActorFactory::createActor(const f32v2& position, const vio::Path& texturePath, const vio::Path& definitionFile) {
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
	physComp.mFrictionCoef = 0.9f;
	physComp.mFlags = 0;
	physComp.mQueryActorType = ACTORTYPE_HUMAN;

	auto spriteCompPair = static_cast<EntityComponentSystem&>(mEcs).addSimpleSpriteComponent(newEntity);
	auto& spriteComp = spriteCompPair.second;
	spriteComp.physicsComponent = physCompPair.first;
	spriteComp.texture = texture;
	spriteComp.dims = f32v2(30.0f);
	spriteComp.color = color4(1.0f, 0.0f, 0.0f);

	return newEntity;
}
