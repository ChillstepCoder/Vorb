#include "stdafx.h"
#include "actor/HumanActorFactory.h"

#include <Vorb/graphics/TextureCache.h>
#include "EntityComponentSystem.h"

const float SPRITE_RADIUS = 0.3f; // In meters

HumanActorFactory::HumanActorFactory(EntityComponentSystem& ecs, vg::TextureCache& textureCache)
	: IActorFactory(ecs, textureCache) {
}

vecs::EntityID HumanActorFactory::createActor(const f32v2& position, const vio::Path& texturePath, const vio::Path& definitionFile) {
	UNUSED(definitionFile);

	VGTexture texture = mTextureCache.addTexture(texturePath).id;

	// Create physics entity
	// TODO: Why not make entities object oriented?
	vecs::EntityID newEntity = mEcs.addEntity();
	
	// RAII in order to initialize the components resources. We should give it access to the ECS so it can retrieve resources from it.
	auto physCompPair = static_cast<EntityComponentSystem&>(mEcs).addPhysicsComponent(newEntity);
	PhysicsComponent& physComp = physCompPair.second;
	// TODO: Do we have an initialization issue here with recycled resources?
	assert(physComp.mQueryActorTypes == ACTORTYPE_NONE);
	physComp.mQueryActorTypes = ACTORTYPE_HUMAN;

	// Setup the physics component
	physComp.initBody(mEcs, position, false /*isStatic*/);
	physComp.addCollider(newEntity, ColliderShapes::SPHERE, SPRITE_RADIUS);

	auto spriteCompPair = mEcs.addSimpleSpriteComponent(newEntity);
	SimpleSpriteComponent& spriteComp = spriteCompPair.second;
	spriteComp.init(physCompPair.first, texture, f32v2(SPRITE_RADIUS * 2.0f));
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
