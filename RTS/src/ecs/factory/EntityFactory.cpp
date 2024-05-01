#include "stdafx.h"
#include "EntityFactory.h"

#include "ecs/EntityRepository.h"
#include "ecs/IEntityComponentSystem.h"
#include "definitions/EntityDef.h"

#include "resources/ModelRepository.h"
#include "resources/SkillRepository.h"
#include "resources/ResourceManager.h"
#include "item/ItemRepository.h"

#include <ozz/animation/runtime/animation.h>
#include "world/World.h"
#include "physics/PhysicsWorld.h"

#include "math/Random.h"

entt::entity EntityFactory::createEntity(World& world, f32v3 position, StrToken typeToken) {
    ASSERT_GAME_THREAD();
    PhysicsWorld& physWorld = world.getPhysicsWorld();
    IEntityComponentSystem& ecs = world.getECS();

    entt::registry& registry = ecs.mRegistry;
    const entt::entity newEntity = registry.create();
    // Copy components over to new entity
    ResourceManager& resourceManager = Services::ResourceManager::ref();
    //todo_use_ryml; // TODO: USE RYML
    const EntityDef& edef = EntityRepository::get().getLoadedAsset(typeToken);

    // Char control needs further initialization post physics load
    CharacterControlComponent* charControlCmp = nullptr;
    // Initialize components
    // TODO: Use groups
    for (auto&& cdef : edef.components) {
        switch (cdef.type) {
            case ComponentTypes::CharacterModel: {
                // TODO: Select correct model
                assert(cdef.characterModel.model.isValid());
                registry.emplace<CharacterModelComponent>(newEntity, cdef.characterModel.model.getAssetID());
                if (cdef.characterModel.animMachine.isValid()) {
                    assert(false); // DO THIS
                }
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
                if (characterDetails.name.size()) {
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
                registry.emplace<InventoryComponent>(newEntity);
                break;
            }
            case ComponentTypes::Physics: {
                auto& physics = registry.emplace<PhysicsComponent>(newEntity);
                auto& positionCmp = registry.get_or_emplace<PositionComponent>(newEntity);
                RigidBodyRotationType rotType = RigidBodyRotationType::FULL;
                if (cdef.physics.disableXyzRot) {
                    rotType = RigidBodyRotationType::NO_ROTATE;
                }
                else if (cdef.physics.disableXyRot) {
                    rotType = RigidBodyRotationType::NO_ROTATE_XY;
                }
                // TODO: allow collision group specify
                RigidBodyPair rbp = physWorld.addRigidBody(newEntity, position, cdef.physics.colliderShape, cdef.physics.halfExtents, cdef.physics.massKg, CollisionGroup::CHARACTER, rotType);
                physics.mRigidBody = rbp.first;
                physics.mZPosOffset = -rbp.second;
                positionCmp.mPosition = position;
                break;
            }
            case ComponentTypes::Profession: {
                registry.emplace<ProfessionComponent>(newEntity);
                break;
            }
            case ComponentTypes::Skills: {
                // TODO: We shouldnt have to do this every single time we create a new entity!
                auto& skillsCmp = registry.emplace<SkillsComponent>(newEntity);
                SkillRepository& skillRepo = SkillRepository::get();
                skillsCmp.mSkills.reserve(cdef.skillsFileData.mSkillNames.size());
                for (size_t i = 0; i < cdef.skillsFileData.mSkillNames.size(); ++i) {
                    skillsCmp.mSkills.emplace_back(skillRepo.getAssetHandle(StrToken(cdef.skillsFileData.mSkillNames[i])));
                }   
                break;
            }
            default:
                assert(false); // Missing type
                break;
        }
        static_assert(e_cast(ComponentTypes::COUNT) == 12, "Update component construction");
    }

    // Post load
     // OLD BULLET CONTROLLER
    // if (charControlCmp) {
   //     charControlCmp->mController = physWorld.addDynamicCharacterController(newEntity, registry.get<PhysicsComponent>(newEntity).mRigidBody, 0.0f);
   // }

    return newEntity;
}

entt::entity EntityFactory::createItemProjectile(World& world, f32v3 position, f32v3 velocity, ItemStack itemStack) {
    IEntityComponentSystem& ecs = world.getECS();
    entt::registry& registry = ecs.mRegistry;
    const entt::entity newEntity = registry.create();

    registry.emplace<PositionComponent>(newEntity, position);
    registry.emplace<ItemComponent>(newEntity, itemStack);
    registry.emplace<OrientationComponent>(newEntity, glm::angleAxis(Random::getCachedRandomf() * M_2_PIF, f32v3(0.0f, 0.0f, 1.0f)));
    BitFlags<ProjectileFlags> flags(ProjectileFlags::RemoveOnLand, ProjectileFlags::OrientToTerrainOnLand);
    ProjectileSystem::addProjectileComponent(registry, newEntity, velocity, flags);

    ItemRepository& itemRepo = ItemRepository::get();
    const ItemDef& itemDef = itemRepo.getLoadedOrUnloadedAsset(itemStack.id);
    if (itemDef.mModelRef.isValid()) {
        registry.emplace<DynamicModelComponent>(newEntity, ModelRepository::get().getAssetID(itemDef.mModelRef.name));
    }
    return newEntity;
}
