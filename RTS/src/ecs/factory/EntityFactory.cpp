#include "stdafx.h"
#include "EntityFactory.h"

#include "ecs/EntityRepository.h"
#include "ecs/IFullECS.h"
#include "Ecs/component/ThreadSharedComponent.h"
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

#include <Jolt/Physics/Character/Character.h>

#include "math/Random.h"

constexpr f32 ITEM_NAMEPLATE_HEIGHT = 0.25f;
constexpr f32 SACK_NAMEPLATE_HEIGHT = 0.5f;

entt::entity EntityFactory::createEntity(World& world, f32v3 position, StrToken typeToken) {
    ASSERT_GAME_THREAD();

    // We only support creating entities in the game world for now,
    // due to physics component doing singleton lookup
    assert(&world == sGameWorld.get());

    PhysicsWorld& physicsWorld = world.getPhysicsWorld();
    IFullECS& ecs = world.getECS();

    entt::registry& registry = ecs.mRegistry;
    const entt::entity newEntity = registry.create();
    // Copy components over to new entity
    ModelRepository& modelRepo = ModelRepository::get();
    ResourceManager& resourceManager = Services::ResourceManager::ref();
    //todo_use_ryml; // TODO: USE RYML
    const EntityDef& edef = EntityRepository::get().getLoadedAsset(typeToken);

    // Char control needs further initialization post physics load
    CharacterControlComponent* charControlCmp = nullptr;

    // All entities have a position
    auto& positionCmp = registry.get_or_emplace<PositionComponent>(newEntity);
    positionCmp.mPosition = position;
    positionCmp.chunkId = world.getChunkIDAtWorldPos(position);

    bool hasCharacterControl = false;

    // Initialize components
    // TODO: Use groups
    // TODO: WE NEED TO VALIDATE ORDER! PHYSICS MUST BE AFTER CHARACTER CONTROL!
    for (const ComponentDefinitionInstance& defInst : edef.components) {
        switch (defInst.type) {
            case ComponentType::CharacterModel: {
                assert(defInst.componentDef);
                CharacterModelComponentDef& cdef = static_cast<CharacterModelComponentDef&>(*defInst.componentDef);
                // TODO: Select correct model
                assert(cdef.model.isValid());
                ModularHumanoidCharacterModel model;
                model.baseModel = cdef.model.getAssetID();
                const ModelDef& baseDef = modelRepo.getLoadedOrUnloadedAsset(model.baseModel);
                // Default submodels for now
                //assert(baseDef.mSubmeshData.size() == e_count(HumanoidSubmeshPart));
                for (size_t i = 0; i < baseDef.mSubmeshData.size(); ++i) {
                    model.partIds[i] = baseDef.mSubmeshData[i].submeshId;
                }
                registry.emplace<CharacterModelComponent>(newEntity, model, nullptr);
                break;
            }
            case ComponentType::CharacterControl: {
                assert(defInst.componentDef);
                assert(!registry.all_of<PhysicsComponent>(newEntity) && "Character control component must come BEFORE physics!");
                CharacterControlComponentDef& cdef = static_cast<CharacterControlComponentDef&>(*defInst.componentDef);
                auto& cmp = registry.emplace<CharacterControlComponent>(newEntity);
                cmp.mSpeedRun = cdef.mSpeed;
                charControlCmp = &cmp;
                // Character control begets navigation always
                registry.get_or_emplace<NavigationComponent>(newEntity);
                hasCharacterControl = true;
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

                physics.mHalfHeight = cdef.halfExtents.y;
                if (hasCharacterControl) {
                    // TODO: Use?
                    UNUSED(cdef.massKg);
                    UNUSED(cdef.colliderShape);
                    UNUSED(cdef.disableXyzRot);
                    CharacterControlComponent& controlCmp = registry.get<CharacterControlComponent>(newEntity);
                    controlCmp.mCharacterController = physicsWorld.createSimpleCharacter(newEntity, f32v3(position.x, position.y, position.z + cdef.halfExtents.y), cdef.halfExtents);
                    physics.mBodyID = static_cast<JPH::Character&>(*controlCmp.mCharacterController).GetBodyID().GetIndexAndSequenceNumber();
                }
                else {
                    assert(cdef.colliderShape == CollisionShapes::Capsule && "Only capsule physics supported for now");
                    physics.mBodyID = physicsWorld.createCharacterBody(newEntity, position, cdef.halfExtents);
                    assert(false); // Shouldn't happen?
                }
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

AssetID getItemSackSmallID() {
    return ModelRepository::get().getAssetID(CStrToken("item_sack_small"));
}

AssetID getItemModelID(ItemStack stack) {
    ItemRepository& itemRepo = ItemRepository::get();
    const ItemDef& itemDef = itemRepo.getLoadedOrUnloadedAsset(stack.id);
    ModelID modelId;
    if (itemDef.mModelRefs.size()) {
        if (stack.count > 1) {
            modelId = getItemSackSmallID();
        }
        else {
            const i32 modelIndex = Random::getCachedRandom() % itemDef.mModelRefs.size();
            modelId = itemDef.mModelRefs[modelIndex].getAssetID();
        }
    }
    else {
        modelId = getItemSackSmallID();
    }
    return modelId;
}

entt::entity EntityFactory::createItemProjectile(World& world, f32v3 position, f32v3 velocity, ItemStack itemStack) {
    ASSERT_GAME_THREAD();
    assert(itemStack.count);
    IFullECS& ecs = world.getECS();
    entt::registry& registry = ecs.mRegistry;
    const entt::entity newEntity = registry.create();

    const ItemDef& itemDef = ItemRepository::get().getLoadedOrUnloadedAsset(itemStack.id);
    ModelID modelId = getItemModelID(itemStack);

    // TODO: allow no collider and use old projectile component?
    const ModelDef& modelDef = ModelRepository::get().getLoadedOrUnloadedAsset(modelId);
    const ModelCollider& colliderData = modelDef.mColliderData;
    if (colliderData.mSubShapes.size() != 1) {
        panic("Tried to spawn item collider with {} subshapes - {}", colliderData.mSubShapes.size(), ModelRepository::get().getAssetName(modelId).toString());
    }
    const ModelColliderShape& baseShapeData = colliderData.mSubShapes[0];

    // Note that since this is completely random we can ignore the baseShapeData.mOrientation
    glm::quat startOrientation = MathUtil::randomQuaternion();
    registry.emplace<PositionComponent>(newEntity, position, world.getChunkIDAtWorldPos(position));
    registry.emplace<SimpleItemComponent>(newEntity, itemStack);
    registry.emplace<OrientationComponent>(newEntity, startOrientation);
    registry.emplace<SimpleTextNameplateComponent>(newEntity, itemDef.mDisplayName.c_str(), ITEM_NAMEPLATE_HEIGHT, color::White);
    if (colliderData.mHasBaseOrientation || colliderData.mHasBaseOffset) {
        registry.emplace<ColliderInverseTransformComponent>(newEntity, colliderData.mInverseBaseOrientation, colliderData.mBaseOffset);
    }
    PhysicsComponent& physCmp = registry.emplace<PhysicsComponent>(newEntity);

    const f32v3 angularVelocity = MathUtil::randomAngularVelocity(Random::getCachedRandomf() * 4.0f);

    switch (baseShapeData.mShape) {
        case CollisionShapes::Capsule:
            physCmp.mBodyID = world.getPhysicsWorld().createDynamicItemBody(newEntity, position + startOrientation * baseShapeData.mOffset, modelDef.mCollisionShapeID, startOrientation, velocity, angularVelocity);
            break;
        case CollisionShapes::Cylinder:
        case CollisionShapes::Box:
        case CollisionShapes::Sphere:
        default:
            panic("Unhandled collision shape {} in createItemProjectile", (int)baseShapeData.mShape);
    }
    static_assert(e_count(CollisionShapes) == 7);
    
    // TODO: Use this version for sack items
    //registry.emplace<AngularVelocityComponent>(newEntity, MathUtil::randomAngularVelocity(Random::getCachedRandomf() * 4.0f));
    //BitFlags<ProjectileFlags> flags(ProjectileFlags::RemoveOnLand, ProjectileFlags::OrientToTerrainOnLand);
    //ProjectileSystem::addProjectileComponent(registry, newEntity, velocity, flags, [](World& world, entt::registry& registry, entt::entity entity, ProjectileImpactResult result) {
    //    ASSERT_GAME_THREAD();
    //    // Switch to static model on land
    //    if (DynamicModelComponent* dynCmp = registry.try_get<DynamicModelComponent>(entity)) {
    //        StaticModelComponent& staticCmp = registry.emplace<StaticModelComponent>(entity, dynCmp->modelId);
    //        registry.remove<DynamicModelComponent>(entity);
    //        registry.remove<AngularVelocityComponent>(entity);
    //        assert(RenderContext::exists());
    //        InstancedStaticModelManager& modelMgr = RenderContext::getInstance().getRenderDataManagerForWorld(world).getInstancedStaticModelManager();
    //        staticCmp.staticModelInstanceId = modelMgr.addLooseModelInstance(
    //            staticCmp.modelId, registry.get<OrientationComponent>(entity).mOrientation, registry.get<PositionComponent>(entity).mPosition, 0 /*TODO Variant*/
    //        );
    //        // Item is now gounded
    //        world.dispatchOnItemProjectileLand(WorldEntityEvent(world, entity));
    //    }
    //});


    registry.emplace<DynamicModelComponent>(newEntity, modelId, 1.0f /*scale*/);
    world.dispatchOnEntityCreated(WorldEntityEvent(world, newEntity));

    return newEntity;
}

void finalizeItemOnGroundEntity(entt::entity newEntity, World& world, f32v3 position, ChunkID chunkId, ModelID modelId, f32 scale) {
    ASSERT_GAME_THREAD();
    entt::registry& registry = world.getECS().mRegistry;

    PositionComponent& posCmp = registry.emplace<PositionComponent>(newEntity, position, chunkId);
    OrientationComponent& orientCmp = registry.emplace<OrientationComponent>(newEntity, glm::angleAxis(Random::getCachedRandomf() * M_2_PIF, f32v3(0.0f, 0.0f, 1.0f)));

    StaticModelComponent& staticCmp = registry.emplace<StaticModelComponent>(newEntity, modelId, scale);
    InstancedStaticModelManager& modelMgr = world.getRenderDataManager().getInstancedStaticModelManager();
    staticCmp.staticModelInstanceId = modelMgr.addLooseModelInstance(
        staticCmp.modelId, orientCmp.mOrientation, position, 0 /*TODO Variant*/, scale
    );

    const ModelDef& def = ModelRepository::get().getLoadedOrUnloadedAsset(modelId);
    if (def.mCollisionShapeID != INVALID_COLLISION_SHAPE_ID) {
        StaticPhysicsComponent& physCmp = registry.emplace<StaticPhysicsComponent>(newEntity);
        physCmp.mBodyID = world.getPhysicsWorld().createStaticItemBody(newEntity, position + orientCmp.mOrientation * (def.mColliderData.mBaseOffset * scale), def.mCollisionShapeID, orientCmp.mOrientation, scale);
    }

    world.dispatchOnEntityCreated(WorldEntityEvent(world, newEntity));
}

f32 computeItemSackScale(f32 totalWeight) {
    return glm::clamp(powf(totalWeight, 0.34f), 0.1f, 3.0f);
}

entt::entity EntityFactory::createItemOnGround(World& world, f32v3 position, ItemStack itemStack, TileItemUID uid) {
    ASSERT_GAME_THREAD();
    assert(itemStack.count);
    entt::registry& registry = world.getECS().mRegistry;
    const entt::entity newEntity = registry.create();

    const ChunkID chunkId = world.getChunkIDAtWorldPos(position);
    if (world.isChunkDeactivated(chunkId)) [[unlikely]] {
        // This can happen if sim thread just created an item RIGHT before this chunk deactivated
        return entt::null;
    }

    ItemRepository& itemRepo = ItemRepository::get();
    const ItemDef& itemDef = itemRepo.getLoadedOrUnloadedAsset(itemStack.id);

    f32 scale;
    f32 offset;
    if (itemStack.count == 1) {
        registry.emplace<TileItemComponent>(newEntity, itemStack, uid);
        scale = 1.0f;
        offset = ITEM_NAMEPLATE_HEIGHT;
    }
    else {
        registry.emplace<TileItemContainerComponent>(newEntity, itemStack, uid);
        scale = computeItemSackScale(itemDef.getWeight() * itemStack.count);
        offset = SACK_NAMEPLATE_HEIGHT;
    }

    registry.emplace<SimpleTextNameplateComponent>(newEntity, itemDef.mDisplayName.c_str(), offset * scale, color::White);
    
    const ModelID modelId = getItemModelID(itemStack);

    finalizeItemOnGroundEntity(newEntity, world, position, chunkId, modelId, scale);

    return newEntity;
}

f32 buildContainerNameplateAndGetScale(World& world, TileItemContainerComponent& cmp, entt::entity entity) {
    // TODO: loc translate + memory arena?
    IFullECS& ecs = world.getECS();
    entt::registry& registry = ecs.mRegistry;
    ItemRepository& itemRepo = ItemRepository::get();
    std::string stringBuild;
    int strCount = 0;
    f32 totalWeight = 0.0f;
    for (const ItemStackWithUID& stack : cmp.getItemStacks()) {
        totalWeight += itemRepo.getLoadedOrUnloadedAsset(stack.itemStack.id).getWeight() * stack.itemStack.count;

        if (strCount < 3) {
            stringBuild += fmt::format("{} x {}\n", stack.itemStack.count, itemRepo.getLoadedOrUnloadedAsset(stack.itemStack.id).mDisplayName);
            ++strCount;
            if (strCount == 3) {
                stringBuild += "...";
            }
        }
    }
    const f32 scale = computeItemSackScale(totalWeight);
    registry.emplace_or_replace<SimpleTextNameplateComponent>(entity, std::move(stringBuild), scale * SACK_NAMEPLATE_HEIGHT, color::White);
    return scale;
}

entt::entity EntityFactory::createItemContainerOnGround(World& world, f32v3 position, std::span<TileItemStack> itemStacks) {
    ASSERT_GAME_THREAD();
    assert(itemStacks.size());
    IFullECS& ecs = world.getECS();
    ItemRepository& itemRepo = ItemRepository::get();
    entt::registry& registry = ecs.mRegistry;
    const entt::entity newEntity = registry.create();

    const ChunkID chunkId = world.getChunkIDAtWorldPos(position);
    if (world.isChunkDeactivated(chunkId)) [[unlikely]] {
        // This can happen if sim thread just created an item RIGHT before this chunk deactivated
        return entt::null;
    }

    TileItemContainerComponent& containerCmp = registry.emplace<TileItemContainerComponent>(newEntity, itemStacks);

    containerCmp.onChanged += [newEntity, &world](TileItemContainerComponent& cmp) {
        ASSERT_GAME_THREAD();
        const f32 scale = buildContainerNameplateAndGetScale(world, cmp, newEntity);

        InstancedStaticModelManager& modelMgr = world.getRenderDataManager().getInstancedStaticModelManager();
        entt::registry& registry = world.getECS().mRegistry;
        if (StaticModelComponent* staticCmp = registry.try_get<StaticModelComponent>(newEntity)) {
            modelMgr.changeLooseModelInstanceScale(
                staticCmp->modelId,
                staticCmp->staticModelInstanceId,
                registry.get<OrientationComponent>(newEntity).mOrientation,
                registry.get<PositionComponent>(newEntity).mPosition,
                scale
            );
            staticCmp->scale = scale;
        }
        if (StaticPhysicsComponent* physCmp = registry.try_get<StaticPhysicsComponent>(newEntity)) {
            world.getPhysicsWorld().changeStaticItemBodyScale(physCmp->mBodyID, scale);
        }
    };
    const f32 scale = buildContainerNameplateAndGetScale(world, containerCmp, newEntity);

    const ModelID modelId = getItemSackSmallID();
    finalizeItemOnGroundEntity(newEntity, world, position, chunkId, modelId, scale);

    return newEntity;
}

void EntityFactory::destroyEntity(World& world, entt::entity entity) {
    ASSERT_GAME_THREAD();
    IFullECS& ecs = world.getECS();
    entt::registry& registry = ecs.mRegistry;

    LOG_CRITICAL("Destroy entity {}", (ui32)entity);

    if (PhysicsComponent* cmp = registry.try_get<PhysicsComponent>(entity)) {
        // We dont destroy the body for characters because the destructor on JPH::Character does it for us
        if (registry.all_of<CharacterControlComponent>(entity)) {
            world.getPhysicsWorld().removeBody(cmp->mBodyID, false);
        } else {
            world.getPhysicsWorld().removeBody(cmp->mBodyID, true);
            LOG_CRITICAL("  Destroy body ", cmp->mBodyID);
        }
    } else if (StaticPhysicsComponent* cmp = registry.try_get<StaticPhysicsComponent>(entity)) {
        world.getPhysicsWorld().removeBody(cmp->mBodyID, true);
    }

    if (StaticModelComponent* cmp = registry.try_get<StaticModelComponent>(entity)) {
        InstancedStaticModelManager& modelMgr = world.getRenderDataManager().getInstancedStaticModelManager();
        modelMgr.removeLooseModelInstance(cmp->modelId, cmp->staticModelInstanceId);
    }

    if (RenderThreadSharedComponent* cmp = registry.try_get<RenderThreadSharedComponent>(entity)) {
        cmp->mData->setOwnerEntity(entt::null);
        cmp->mData->wasDestroyed = true;
    }

    world.dispatchOnEntityDestroyed(WorldEntityEvent(world, entity));

    registry.destroy(entity);
}
