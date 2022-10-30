#include "stdafx.h"
#include "EntityFactory.h"

#include "ecs/EntityDefinitionRepository.h"
#include "ecs/IEntityComponentSystem.h"
#include "ecs/component/EntityDefinition.h"

#include "resources/ModelRepository.h"
#include "resources/SkillRepository.h"

#include "resources/ResourceManager.h"
#include <Vorb/graphics/TextureCache.h>

#include <ozz/animation/runtime/animation.h>
#include "world/IWorld.h"
#include "physics/PhysicsWorld.h"

entt::entity EntityFactory::createEntity(const f32v3& position, StrToken typeToken) {
    assert(IS_GAME_THREAD());
    PhysicsWorld& physWorld = sWorld->getPhysicsWorld();
    IEntityComponentSystem& ecs = sWorld->getECS();

    entt::registry& registry = ecs.mRegistry;
    const entt::entity newEntity = registry.create();
    // Copy components over to new entity
    ResourceManager& resourceManager = Services::ResourceManager::ref();
    const EntityDefinition& edef = resourceManager.getEntityDefinitionRepository().getDefinition(typeToken);

    // Char control needs further initialization post physics load
    CharacterControlComponent* charControlCmp = nullptr;
    // Initialize components
    for (auto&& cdef : edef.components) {
        switch (cdef.type) {
            case ComponentTypes::CharacterModel: {
                // TODO: Select correct model
                LOG_CRITICAL("TODO: EntityFactory::createEntity needs to set the correct modelId");
                registry.emplace<CharacterModelComponent>(newEntity, 0);
                break;
            }
            case ComponentTypes::CharacterControl: {
                auto& cmp = registry.emplace<CharacterControlComponent>(newEntity);
                cmp.mSpeedRun = cdef.characterControl.mSpeed;
                charControlCmp = &cmp;
                // Character control begets navigation always
                registry.get_or_emplace<NavigationComponent>(newEntity);
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
            case ComponentTypes::CharacterDetails: {
                auto& characterDetails = registry.emplace<CharacterDetailsComponent>(newEntity);
                if (cdef.characterDetails.name) {
                    characterDetails.name = cdef.characterDetails.name;
                }
                else {
                    characterDetails.name = "UNNAMED CHARACTER";
                }
                break;
            }
            case ComponentTypes::DynamicLight: {
                registry.emplace<DynamicLightComponent>(newEntity);
                break;
            }
            case ComponentTypes::Navigation: {
                registry.get_or_emplace<NavigationComponent>(newEntity);
                break;
            }
            case ComponentTypes::PersonAI: {
                registry.emplace<PersonAIComponent>(newEntity);
                break;
            }
            case ComponentTypes::Inventory: {
                registry.emplace<InventoryComponent>(newEntity, DEFAULT_CARRY_WEIGHT);
                break;
            }
            case ComponentTypes::Physics: {
                auto& physics = registry.emplace<PhysicsComponent>(newEntity);
                RigidBodyRotationType rotType = RigidBodyRotationType::FULL;
                if (cdef.physics.disableXyzRot) {
                    rotType = RigidBodyRotationType::NO_ROTATE;
                }
                else if (cdef.physics.disableXyRot) {
                    rotType = RigidBodyRotationType::NO_ROTATE_XY;
                }
                RigidBodyPair rbp = physWorld.addRigidBody(newEntity, position, cdef.physics.colliderShape, cdef.physics.massKg, cdef.physics.colliderScale, rotType);
                physics.mRigidBody = rbp.first;
                physics.mZPosOffset = -rbp.second;
                break;
            }
            case ComponentTypes::Profession: {
                registry.emplace<ProfessionComponent>(newEntity);
                break;
            }
            case ComponentTypes::SimpleSprite: {
                VGTexture texture = resourceManager.getTextureCache().addTexture(cdef.simpleSprite.texture).id;
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
            case ComponentTypes::Skills: {
                // TODO: We shouldnt have to do this every single time we create a new entity!
                auto& skillsCmp = registry.emplace<SkillsComponent>(newEntity);
                const SkillRepository& skillRepo = resourceManager.getSkillRepository();
                for (size_t i = 0; i < cdef.skillsFileData.mSkillNames.size(); ++i) {
                    skillsCmp.mSkills.emplace_back(&skillRepo.getSkillDef(cdef.skillsFileData.mSkillNames[i]));
                }   
                break;
            }
            default:
                assert(false); // Missing type
                break;
        }
        static_assert(e_cast(ComponentTypes::COUNT) == 15, "Update component construction");
    }

    // Post load
    if (charControlCmp) {
        charControlCmp->mController = physWorld.addDynamicCharacterController(newEntity, registry.get<PhysicsComponent>(newEntity).mRigidBody, 0.0f);
    }

    return newEntity;
}
