#include "stdafx.h"
#include "actor/UndeadActorFactory.h"

#include "ResourceManager.h"

#include <Vorb/graphics/TextureCache.h>

#include "EntityComponentSystem.h"

const float SPRITE_RADIUS = 0.3f; // In meters

UndeadActorFactory::UndeadActorFactory(entt::registry& registry, World& world, ResourceManager& resourceManager)
	: IActorFactory(registry, world, resourceManager) {
}

entt::entity UndeadActorFactory::createActor(const f32v2& position, const vio::Path& texturePath, const vio::Path& definitionFile) {
    UNUSED(definitionFile);
    UNUSED(texturePath);
    //VGTexture texture = mResourceManager.getTextureCache().addTexture(texturePath).id;

    // Create physics entity
    // TODO: Why not make entities object oriented?

    entt::entity newEntity = mRegistry.create();

    // Physics component
    auto& physics = mRegistry.emplace<PhysicsComponent>(newEntity, mWorld, position, false);
    physics.mQueryActorTypes = ACTORTYPE_UNDEAD;
    physics.addCollider(newEntity, ColliderShapes::SPHERE, SPRITE_RADIUS);

    // Sprite Component
    //auto& spriteComp = mRegistry.emplace<SimpleSpriteComponent>(newEntity, texture, f32v2(SPRITE_RADIUS * 2.0f));
    //spriteComp.mColor = color4(1.0f, 0.0f, 0.0f);

    /*auto& combatComp = mEcs.addCombatComponent(newEntity).second;
    combatComp.mWeapon = WeaponRegistry::getWeapon(BuiltinWeapons::IRON_SWORD);
    combatComp.mArmor = ArmorRegistry::getArmor(BuiltinArmors::IRON_ARMOR_MEDIUM);
    combatComp.mShield = ShieldRegistry::getShield(BuiltinShields::IRON_ROUND);*/
    /*mEcs.addSoldierAIComponent(newEntity);
    auto& navCmp = mEcs.addNavigationComponent(newEntity).second;
    navCmp.mSpeed = 0.1f;*/
    //mRegistry.emplace<PlayerControlComponent>(newEntity);
    // Player combat
    //auto& combatComp = mEcs.getCombatComponentFromEntity(newEntity);

    // model
    auto& modelCmp = mRegistry.emplace<CharacterModelComponent>(newEntity);
    modelCmp.mModel.load(mResourceManager.getTextureCache(), "face/female/Female_Average_Wide", "body/thin", "hair/longB");
    return newEntity;
}
