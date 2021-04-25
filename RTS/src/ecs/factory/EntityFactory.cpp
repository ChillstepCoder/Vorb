#include "stdafx.h"
#include "EntityFactory.h"

#include "ecs/EntityDefinitionRepository.h"

#include "ecs/EntityComponentSystem.h"

#include "ecs/component/EntityDefinition.h"

#include "ResourceManager.h"
#include <Vorb/graphics/TextureCache.h>

EntityFactory::EntityFactory(EntityComponentSystem& ecs, ResourceManager& resourceManager) :
    mEcs(ecs),
    mResourceManager(resourceManager)
{
}

EntityFactory::~EntityFactory()
{

}

entt::entity EntityFactory::createEntity(const f32v2& position, const nString& typeName) {

    entt::registry& registry = mEcs.mRegistry;
    const entt::entity newEntity = registry.create();
    // Copy components over to new entity
    const EntityDefinition& edef = mResourceManager.getEntityDefinitionRepository().getDefinition(typeName);
    // Initialize components
    for (auto&& cdef : edef.components) {
        switch (cdef.type) {
            case ComponentTypes::CharacterModel: {
                auto& modelCmp = registry.emplace<CharacterModelComponent>(newEntity);
                modelCmp.mModel.load(mResourceManager.getTextureCache(), "face/female/Female_Average_Wide", "body/thin", "hair/longB");
                break;
            }
            case ComponentTypes::Combat: {
                registry.emplace<CombatComponent>(newEntity);
                break;
            }
            case ComponentTypes::Corpse: {
                registry.emplace<CorpseComponent>(newEntity);
                break;
            }
            case ComponentTypes::DynamicLight: {
                registry.emplace<DynamicLightComponent>(newEntity);
                break;
            }
            case ComponentTypes::Navigation: {
                registry.emplace<NavigationComponent>(newEntity);
                break;
            }
            case ComponentTypes::Physics: {
                auto& physics = registry.emplace<PhysicsComponent>(newEntity, mEcs.mWorld, position, false);
                physics.mQueryActorTypes = ACTORTYPE_HUMAN;
                physics.addCollider(newEntity, cdef.physics.colliderShape, cdef.physics.colliderRadius);
                break;
            }
            case ComponentTypes::PlayerControl: {
                registry.emplace<PlayerControlComponent>(newEntity);
                break;
            }
            case ComponentTypes::Profession: {
                registry.emplace<ProfessionComponent>(newEntity);
                break;
            }
            case ComponentTypes::SimpleSprite: {
                VGTexture texture = mResourceManager.getTextureCache().addTexture(cdef.simpleSprite.texture).id;
                auto& spriteComp = registry.emplace<SimpleSpriteComponent>(newEntity, texture, cdef.simpleSprite.dims);
                spriteComp.mColor = cdef.simpleSprite.color;
                break;
            }
            case ComponentTypes::SoldierAI: {
                registry.emplace<SoldierAIComponent>(newEntity);
                break;
            }
            case ComponentTypes::UndeadAI: {
                registry.emplace<UndeadAIComponent>(newEntity);
                break;
            }
            default:
                assert(false); // Missing type
                break;
        }
        static_assert(enum_cast(ComponentTypes::COUNT) == 11, "Update component construction");
    }

    return newEntity;
}
