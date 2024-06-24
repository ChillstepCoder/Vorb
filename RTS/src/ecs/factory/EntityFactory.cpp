#include "stdafx.h"
#include "EntityFactory.h"

#include "ecs/EntityRepository.h"
#include "ecs/IFullECS.h"
#include "definitions/EntityDef.h"

#include "resources/ModelRepository.h"
#include "resources/SkillRepository.h"
#include "resources/ResourceManager.h"
#include "item/ItemRepository.h"

#include "util/RandomQuaternion.h"

// TODO: This is a lot of includes to just add a static model
#include "rendering/RenderContext.h"
#include "rendering/renderdata/WorldRenderDataManager.h"
#include "rendering/model/InstancedStaticModelManager.h"

#include <ozz/animation/runtime/animation.h>
#include "world/World.h"
#include "physics/PhysicsWorld.h"

#include "math/Random.h"

entt::entity EntityFactory::createEntity(World& world, f32v3 position, StrToken typeToken) {
    ASSERT_GAME_THREAD();
    PhysicsWorld& physWorld = world.getPhysicsWorld();
    IFullECS& ecs = world.getECS();

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
                registry.emplace<FullBrainComponent>(newEntity);
                break;
            }
            case ComponentType::Inventory: {
                registry.emplace<DualInventoryComponent>(newEntity);
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
                // TODO: AssetRef
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

    world.dispatchOnEntityCreated(WorldEntityEvent(world, newEntity));

    return newEntity;
}

entt::entity EntityFactory::createItemProjectile(World& world, f32v3 position, f32v3 velocity, ItemStack itemStack) {
    ASSERT_GAME_THREAD();
    IFullECS& ecs = world.getECS();
    entt::registry& registry = ecs.mRegistry;
    const entt::entity newEntity = registry.create();

    registry.emplace<PositionComponent>(newEntity, position, world.getChunkIDAtWorldPos(position));
    registry.emplace<SimpleItemComponent>(newEntity, itemStack);
    registry.emplace<OrientationComponent>(newEntity, MathUtil::randomQuaternion());
    registry.emplace<AngularVelocityComponent>(newEntity, MathUtil::randomAngularVelocity(Random::getCachedRandomf() * 4.0f));
    BitFlags<ProjectileFlags> flags(ProjectileFlags::RemoveOnLand, ProjectileFlags::OrientToTerrainOnLand);
    ProjectileSystem::addProjectileComponent(registry, newEntity, velocity, flags, [](World& world, entt::registry& registry, entt::entity entity, ProjectileImpactResult result) {
        ASSERT_GAME_THREAD();
        // Switch to static model on land
        if (DynamicModelComponent* dynCmp = registry.try_get<DynamicModelComponent>(entity)) {
            StaticModelComponent& staticCmp = registry.emplace<StaticModelComponent>(entity, dynCmp->modelId);
            registry.remove<DynamicModelComponent>(entity);
            registry.remove<AngularVelocityComponent>(entity);
            assert(RenderContext::exists());
            // TODO: This incurs a mutex lock in getRenderDataManagerForWorld, and it also could crash during shutdown if the render data manager is destroyed after we access it
            InstancedStaticModelManager& modelMgr = RenderContext::getInstance().getRenderDataManagerForWorld(world).getInstancedStaticModelManager();
            staticCmp.staticModelInstanceId = modelMgr.addLooseModelInstance(
                staticCmp.modelId, registry.get<OrientationComponent>(entity).mOrientation, registry.get<PositionComponent>(entity).mPosition, 0 /*TODO Variant*/
            );
            // Item is now gounded
            registry.emplace<TileItemComponent>(entity, registry.get<SimpleItemComponent>(entity).itemStack, INVALID_TILE_ITEM_UID);
            registry.remove<SimpleItemComponent>(entity);
            world.dispatchOnItemProjectileLand(WorldEntityEvent(world, entity));
        }
    });

    ItemRepository& itemRepo = ItemRepository::get();
    const ItemDef& itemDef = itemRepo.getLoadedOrUnloadedAsset(itemStack.id);
    if (itemDef.mModelRefs.size()) {
        i32 modelIndex = Random::getCachedRandom() % itemDef.mModelRefs.size();
        registry.emplace<DynamicModelComponent>(newEntity, itemDef.mModelRefs[modelIndex].getAssetID());
    }
    else {
        panic("Need fallback sack model for items without model refs in EntityFactory::createItemProjectile");
    }

    world.dispatchOnEntityCreated(WorldEntityEvent(world, newEntity));

    return newEntity;
}

entt::entity EntityFactory::createItemOnGround(World& world, f32v3 position, ItemStack itemStack, TileItemUID uid) {
    ASSERT_GAME_THREAD();
    IFullECS& ecs = world.getECS();
    entt::registry& registry = ecs.mRegistry;
    const entt::entity newEntity = registry.create();

    PositionComponent& posCmp = registry.emplace<PositionComponent>(newEntity, position, world.getChunkIDAtWorldPos(position));
    registry.emplace<TileItemComponent>(newEntity, itemStack, uid);
    OrientationComponent& orientCmp = registry.emplace<OrientationComponent>(newEntity, glm::angleAxis(Random::getCachedRandomf() * M_2_PIF, f32v3(0.0f, 0.0f, 1.0f)));

    ItemRepository& itemRepo = ItemRepository::get();
    const ItemDef& itemDef = itemRepo.getLoadedOrUnloadedAsset(itemStack.id);
    if (itemDef.mModelRefs.size()) {
        const i32 randomIndex = Random::getCachedRandom() % itemDef.mModelRefs.size();
        StaticModelComponent& staticCmp = registry.emplace<StaticModelComponent>(newEntity, itemDef.mModelRefs[randomIndex].getAssetID());
        assert(RenderContext::exists());
        // TODO: This incurs a mutex lock in getRenderDataManagerForWorld, and it also could crash during shutdown if the render data manager is destroyed after we access it
        InstancedStaticModelManager& modelMgr = RenderContext::getInstance().getRenderDataManagerForWorld(world).getInstancedStaticModelManager();
        staticCmp.staticModelInstanceId = modelMgr.addLooseModelInstance(
            staticCmp.modelId, orientCmp.mOrientation, position, 0 /*TODO Variant*/
        );
    }
    else {
        panic("Need fallback sack model for items without model refs in EntityFactory::createItemOnGroundEntity");
    }

    world.dispatchOnEntityCreated(WorldEntityEvent(world, newEntity));

    return newEntity;
}

void EntityFactory::destroyEntity(World& world, entt::entity entity) {
    IFullECS& ecs = world.getECS();
    entt::registry& registry = ecs.mRegistry;

    if (PhysicsComponent* cmp = registry.try_get<PhysicsComponent>(entity)) {
        world.getPhysicsWorld().deleteRigidBody(cmp->mRigidBody);
    }

    if (StaticModelComponent* cmp = registry.try_get<StaticModelComponent>(entity)) {
        // TODO: This incurs a mutex lock in getRenderDataManagerForWorld, and it also could crash during shutdown if the render data manager is destroyed after we access it
        InstancedStaticModelManager& modelMgr = RenderContext::getInstance().getRenderDataManagerForWorld(world).getInstancedStaticModelManager();
        modelMgr.removeLooseModelInstance(cmp->modelId, cmp->staticModelInstanceId);
    }

    world.dispatchOnEntityDestroyed(WorldEntityEvent(world, entity));

    registry.destroy(entity);
}
