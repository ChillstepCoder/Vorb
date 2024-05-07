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

    // All entities have a position
    auto& positionCmp = registry.get_or_emplace<PositionComponent>(newEntity);
    positionCmp.mPosition = position;
    positionCmp.chunkId = world.getChunkIDAtWorldPos(position);

    // Initialize components
    // TODO: Use groups
    for (const ComponentDefinitionInstance& defInst : edef.components) {
        switch (defInst.type) {
            case ComponentType::CharacterModel: {
                assert(defInst.componentDef);
                CharacterModelComponentDef& cdef = static_cast<CharacterModelComponentDef&>(*defInst.componentDef);
                // TODO: Select correct model
                assert(cdef.model.isValid());
                registry.emplace<CharacterModelComponent>(newEntity, cdef.model.getAssetID());
                break;
            }
            case ComponentType::CharacterControl: {
                assert(defInst.componentDef);
                CharacterControlComponentDef& cdef = static_cast<CharacterControlComponentDef&>(*defInst.componentDef);
                auto& cmp = registry.emplace<CharacterControlComponent>(newEntity);
                cmp.mSpeedRun = cdef.mSpeed;
                charControlCmp = &cmp;
                // Character control begets navigation always
                registry.get_or_emplace<NavigationComponent>(newEntity);
                break;
            }
            case ComponentType::Combat: {
                registry.emplace<CombatComponent>(newEntity);
                break;
            }
            case ComponentType::Corpse: {
                registry.emplace<CorpseComponent>(newEntity);
                break;
            }
            case ComponentType::CharacterDetails: {
                assert(defInst.componentDef);
                CharacterDetailsComponentDef& cdef = static_cast<CharacterDetailsComponentDef&>(*defInst.componentDef);
                auto& characterDetails = registry.emplace<CharacterDetailsComponent>(newEntity);
                if (characterDetails.name.size()) {
                    characterDetails.name = cdef.name;
                }
                else {
                    characterDetails.name = "UNNAMED CHARACTER";
                }
                break;
            }
            case ComponentType::DynamicLight: {
                registry.emplace<DynamicLightComponent>(newEntity);
                break;
            }
            case ComponentType::Navigation: {
                registry.get_or_emplace<NavigationComponent>(newEntity);
                break;
            }
            case ComponentType::PersonAI: {
                registry.emplace<PersonAIComponent>(newEntity);
                break;
            }
            case ComponentType::Inventory: {
                registry.emplace<InventoryComponent>(newEntity);
                break;
            }
            case ComponentType::Physics: {
                assert(defInst.componentDef);
                PhysicsComponentDef& cdef = static_cast<PhysicsComponentDef&>(*defInst.componentDef);
                auto& physics = registry.emplace<PhysicsComponent>(newEntity);
                RigidBodyRotationType rotType = RigidBodyRotationType::FULL;
                if (cdef.disableXyzRot) {
                    rotType = RigidBodyRotationType::NO_ROTATE;
                }
                else if (cdef.disableXyRot) {
                    rotType = RigidBodyRotationType::NO_ROTATE_XY;
                }
                // TODO: allow collision group specify
                RigidBodyPair rbp = physWorld.addRigidBody(newEntity, position, cdef.colliderShape, cdef.halfExtents, cdef.massKg, CollisionGroup::CHARACTER, rotType);
                physics.mRigidBody = rbp.first;
                physics.mZPosOffset = -rbp.second;
                break;
            }
            case ComponentType::Profession: {
                registry.emplace<ProfessionComponent>(newEntity);
                break;
            }
            case ComponentType::Skills: {
                assert(defInst.componentDef);
                SkillsComponentDef& cdef = static_cast<SkillsComponentDef&>(*defInst.componentDef);
                // TODO: We shouldnt have to do this every single time we create a new entity!
                auto& skillsCmp = registry.emplace<SkillsComponent>(newEntity);
                SkillRepository& skillRepo = SkillRepository::get();
                skillsCmp.mSkills.reserve(cdef.mSkillNames.size());
                // TODO: SoftAssetReference
                for (size_t i = 0; i < cdef.mSkillNames.size(); ++i) {
                    skillsCmp.mSkills.emplace_back(skillRepo.getAssetHandle(StrToken(cdef.mSkillNames[i])));
                }   
                break;
            }
            default:
                panic("Missing component type");
                break;
        }
        static_assert(e_cast(ComponentType::COUNT) == 12, "Update component construction");
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
