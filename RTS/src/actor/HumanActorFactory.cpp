#include "stdafx.h"
#include "actor/HumanActorFactory.h"

#include <Vorb/graphics/TextureCache.h>
#include <box2d/b2_body.h>
#include <box2d/b2_circle_shape.h>
#include <box2d/b2_fixture.h>
#include "EntityComponentSystem.h"

const float SPRITE_RADIUS = 0.3f; // In meters

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
	physComp.mFlags = 0;
	physComp.mQueryActorTypes = ACTORTYPE_HUMAN;

	{ // box2d
		b2BodyDef bodyDef;
		bodyDef.type = b2_dynamicBody;
		bodyDef.position.Set(position.x, position.y);
		b2Body* body = mEcs.getPhysWorld().CreateBody(&bodyDef);
		body->SetLinearDamping(0.1f);

		b2CircleShape dynamicCircle;
		dynamicCircle.m_radius = SPRITE_RADIUS;
		physComp.mCollisionRadius = dynamicCircle.m_radius;

		b2FixtureDef fixtureDef;
		fixtureDef.shape = &dynamicCircle;
		fixtureDef.density = 1.0f;
		fixtureDef.userData = reinterpret_cast<void*>(newEntity);

		body->CreateFixture(&fixtureDef);
		physComp.mBody = body;
	}

	auto spriteCompPair = mEcs.addSimpleSpriteComponent(newEntity);
	auto& spriteComp = spriteCompPair.second;
	spriteComp.physicsComponent = physCompPair.first;
	spriteComp.texture = texture;
	spriteComp.dims = f32v2(SPRITE_RADIUS * 2.0f);
	spriteComp.color = color4(1.0f, 0.0f, 0.0f);

	auto& combatComp = mEcs.addCombatComponent(newEntity).second;
	combatComp.mWeapon = WeaponRegistry::getWeapon(BuiltinWeapons::IRON_SWORD);
	combatComp.mArmor = ArmorRegistry::getArmor(BuiltinArmors::IRON_ARMOR_MEDIUM);
	combatComp.mShield = ShieldRegistry::getShield(BuiltinShields::IRON_ROUND);

	mEcs.addSoldierAIComponent(newEntity);
	auto& navCmp = mEcs.addNavigationComponent(newEntity).second;
	navCmp.mSpeed = 0.1f;

	return newEntity;
}
