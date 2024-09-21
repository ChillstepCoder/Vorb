#include "stdafx.h"
#include "IFullECS.h"

#include "world/World.h"
#include "world/IHeightmapGrid.h"
#include "world/LocalChunkGrid.h"
#include "world/chunk/SimChunkGrid.h"
#include "ecs/component/FullEntityBindingComponent.h"
#include "ecs/component/SimEntityTypeComponent.h"
#include "ecs/component/ThreadSharedComponent.h"
#include "ecs/AttachedEntityUpdater.h"
#include "ecs/system/PlayerInteractSystem.h"
#include "camera/Camera3D.h"

#include "physics/PhysicsWorld.h"

#include "ecs/factory/EntityFactory.h"

// TODO: Get rid of this
#include "rendering/RenderContext.h"

#include "debugging/DebugRenderer.h"

const float DEAD_COLOR_MULT = 0.4f;

constexpr ui32 ENTITY_LIST_RESERVE_COUNT = 64;
// Prevent lists getting too out of control
constexpr ui32 ENTITY_LIST_DEALLOCATE_COUNT = 512;

IFullECS::IFullECS(World& world) : mWorld(world) {
	mEntitiesByChunk.resize(world.getTotalChunks());
	initEvents();

    // Prevent allocations
    for (auto& list : mEntitiesByChunk) {
        list.reserve(ENTITY_LIST_RESERVE_COUNT);
    }

    mPlayerControlSystem = std::make_unique<PlayerControlSystem>(mWorld, mRegistry);
    mPlayerInteractSystem = std::make_unique<PlayerInteractSystem>(mWorld);
}

IFullECS::~IFullECS() {

}

void IFullECS::onWorldBeginGameThread() {
    mDebugEntityUpdater = std::unique_ptr<AttachedEntityUpdater>(new AttachedEntityUpdater);
}

void IFullECS::tick(f32 elapsedSec) {
    PROFILE_FUNCTION();
    ASSERT_GAME_THREAD();
	
    //mPlayerControlSystem.update(mRegistry, playerCamera);
    // TODO: Move 
  
	mTimedTileInteractSystem.update(mRegistry);

	mFishingSystem.update(mWorld, mRegistry, elapsedSec);

    //mCorpseTable.update();
    
	// TODO: Client ECS
	if (RenderContext::exists()) {
		mPlayerControlSystem->update(RenderContext::getInstance().getGameThreadCameraData(), elapsedSec);
	}

    mSkillsSystem.update(mWorld, mRegistry, elapsedSec);

    if (mLocalPlayerEntity != entt::null) {
        mPlayerInteractSystem->update(mRegistry, elapsedSec, mLocalPlayerEntity);
    }

	ProjectileSystem::update(mWorld, mRegistry, elapsedSec);

    ObjectPickupSystem::update(mWorld, mRegistry, elapsedSec);

    RenderThreadSharedComponentSystem::update(mWorld, mRegistry);

    assert(mDebugEntityUpdater);
    mDebugEntityUpdater->update(mRegistry);
}

void IFullECS::tickPhysics(f32 elapsedSec) {
	mPhysicsSystem.update(mWorld, mRegistry, elapsedSec);
	mCharacterControlSystem.update(mWorld, mRegistry, elapsedSec);
}

void IFullECS::addPendingEntitiesToChunk(LocalChunk& chunk, ChunkFullTransitionData&& data) {

	ASSERT_GAME_THREAD();
	ChunkID chunkId = chunk.getChunkID();
	auto&& it = mPendingEntities.find(chunkId);
	if (it == mPendingEntities.end()) {
        mPendingEntities.emplace(chunkId, std::move(data));
	}
	else {
        ChunkFullTransitionData& existingData = it->second;
        // Merge entities
        data.entities.reserve(data.entities.size() + existingData.entities.size());
        for (auto&& entityData : data.entities) {
            existingData.entities.emplace_back(std::move(entityData));
        }
        //existingData.entities.insert(it->second.entities.end(), data.entities.begin(), data.entities.end());
        // Merge item maps
        for (auto&& sit : data.itemStacks) { 
            // Target
            auto&& tit = existingData.itemStacks.find(sit.first);
            if (tit == existingData.itemStacks.end()) {
                existingData.itemStacks.emplace(sit.first, std::move(sit.second));
            }
            else {
                tit->second.insert(tit->second.end(), sit.second.begin(), sit.second.end());
            }
        }
	}
}

void IFullECS::createFullEntitiesFromSimEntities(LocalChunk& chunk, ChunkFullTransitionData& data) {
    ASSERT_GAME_THREAD();

    const IHeightmapGrid& heightGrid = mWorld.getHeightmapGrid();
    for (EntityFullTransitionData& activateData : data.entities) {
        // TODO: I dont think we need thread safe here as main thread is only writer?
        const f32 zPos = heightGrid.computeHeightAtPoint<true>(activateData.simPosition);
        entt::entity newEntity = entt::null;
        switch (activateData.entityType) {
            case SimEntityType::Character: {
                newEntity = createEntity(f32v3(activateData.simPosition.x, activateData.simPosition.y, zPos), CStrToken("villager"), true /*shouldReplicate*/);
                break;
            }
            case SimEntityType::Group:
            case SimEntityType::Settlement:
            default:
                panic("Tried to create invalid entity type {}", (int)activateData.entityType);
                break;

        }
        assert(activateData.binding);
        //activateData.binding->fullEntity = newEntity;
        mRegistry.emplace<FullEntityBindingComponent>(newEntity).binding = activateData.binding;
        mRegistry.emplace<SimEntityTypeComponent>(newEntity).type = activateData.entityType;
        activateData.characterData->moveToEntity(mWorld, mRegistry, newEntity, true /*isFull*/);
        static_assert(e_count(SimEntityType) == 4);
    }

    // Re-sort by tiles so we can combine into container entities per tile
    // TODO: Scratch allocator?
    static UnorderedFlatMap<TileIndex, std::vector<TileItemStack>> tileCollapsedStacks;
    for (auto&& [itemID, stacks] : data.itemStacks) {
        for (const TileItemStack& stack : stacks) {
            tileCollapsedStacks[stack.tileIndex].emplace_back(stack);
        }
    }

    // Items
    const f32v2 worldPosChunkWithTileOffset(chunk.getWorldPos().x + 0.5f, chunk.getWorldPos().y + 0.5f);
    for (auto&& [tileIndex, stacks] : tileCollapsedStacks) {
        f32v2 worldPos(worldPosChunkWithTileOffset + f32v2(tileIndex % CHUNK_WIDTH, tileIndex / CHUNK_WIDTH));
        worldPos.x += Random::getCachedRandomfInRange(-0.4f, 0.4f);
        worldPos.y += Random::getCachedRandomfInRange(-0.4f, 0.4f);
        const f32v3 pos3(worldPos.x, worldPos.y, heightGrid.computeHeightAtPoint<true>(worldPos));
        if (stacks.size() == 1) {
            const TileItemStack& stack = stacks[0];
            entt::entity newEntity = EntityFactory::createItemOnGround(mWorld, pos3, stack.toItemStack(), stack.uniqueId);
            if (newEntity != entt::null) {
                mTileItemEntityMap[stack.uniqueId] = newEntity;
                // TODO: DELETE ME
                AM::DebugRenderer::drawWireQuadThreadSafe(pos3, f32v2(1.0f), color::Magenta, 300);
            }
        }
        else {
            entt::entity newEntity = EntityFactory::createItemContainerOnGround(mWorld, pos3, stacks);
            if (newEntity != entt::null) {
                for (const TileItemStack& stack : stacks) {
                    mTileItemEntityMap[stack.uniqueId] = newEntity;
                }
                // TODO: DELETE ME
                AM::DebugRenderer::drawWireQuadThreadSafe(pos3, f32v2(1.0f), color::Red, 300);
            }
        }
    }

    tileCollapsedStacks.clear();
}

ChunkSimTransitionData IFullECS::deactivateEntitiesForChunk(LocalChunk& chunk) {
    ASSERT_GAME_THREAD();
	EntityVector& chunkEntities = mEntitiesByChunk[chunk.getChunkID()];
    ChunkSimTransitionData rv;
	rv.entities.reserve(chunkEntities.size());

	std::vector<entt::entity> unboundEntities;

	for (entt::entity e : chunkEntities) {
        assert(mRegistry.valid(e));
		if (FullEntityBindingComponent* bindingCmp = mRegistry.try_get<FullEntityBindingComponent>(e)) [[likely]] {
            EntitySimTransitionData& ddata = rv.entities.emplace_back();
            ddata.moveFromFullEntity(mRegistry, e);
			destroyEntity(e);
		}
		else {
            // Keep players
            if (mRegistry.all_of<PlayerControlComponent>(e)) {
                unboundEntities.emplace_back(e);
            }
            else {

                // If we are a loose item, we need to connect it to the world
                if (SimpleItemComponent* itemCmp = mRegistry.try_get<SimpleItemComponent>(e)) {
                    if (mRegistry.all_of<PhysicsComponent>(e)) {
                        // Note that we do not call connectItemToChunk because we do not need to set up this entity, it is about
                        // to be destroyed. Instead we simply notify the sim chunk grid and forget the UID
                        mWorld.getSimChunkGrid().connectItemEntityToGroundGameThreadNoMerge(itemCmp->itemStack, mRegistry.get<PositionComponent>(e).mPosition);
                    }
                }

                // TODO: Item projectiles probably should snap to the ground rather than delete
                // Destroy everything else, assume it is tracked
                destroyEntity(e);
            }
		}
	}
	chunkEntities.swap(unboundEntities);
	chunkEntities.shrink_to_fit();
	return rv;
}

bool IFullECS::onEntityEnterNewChunk(entt::entity entity, ChunkID prevChunkID, ChunkID newChunkID) {
    ASSERT_GAME_THREAD();

    LocalChunkGrid& chunkGrid = mWorld.getLocalChunkGrid();
    LocalChunk& newChunk = chunkGrid.getChunk(newChunkID);
    if (newChunk.isActivated()) {
        mEntitiesByChunk[newChunkID].emplace_back(entity);
    }
    else {
        if (FullEntityBindingComponent* bindingCmp = mRegistry.try_get<FullEntityBindingComponent>(entity)) {
            if (newChunk.isDeactivated()) {
                // Send to sim thread
                FullECSEvent e;
                e.entity = entity;
                e.chunkId = newChunk.getChunkID();
                dispatchEntityDeactivated(e);

                // We will remove from mEntitiesbyChunk in the destroy listener
                destroyEntity(entity);
                return true;
            }
            else {
                // If we get here (rare), we are in the process of activating, so pretend we are still in the old chunk and retry next time
                mRegistry.get<PositionComponent>(entity).chunkId = prevChunkID;
                return false;
            }
        }
        else if (mRegistry.any_of<TileItemComponent, TileItemContainerComponent>(entity)) {
            // Items destroy, will remove from mEntitiesbyChunk in the destroy listener
            destroyEntity(entity);
            return true;
        } else {
            // Non sim entity such as player, just add to the entities by chunk
            mEntitiesByChunk[newChunkID].emplace_back(entity);
            LOG_WARN("Added non sim entity {} to deactivated chunk {}", (int)entity, newChunkID);
        }
    }

    // Remove from previous if we did not return early due to setting to previous chunk
    EntityVector& prevEntityList = mEntitiesByChunk[prevChunkID];
    bool found = false;
    // We amortize this by reverse iterating as
    // more dynamic entities are likely to be at the end of the list
    for (int i = (int)prevEntityList.size() - 1; i >= 0; --i) {
        if (prevEntityList[i] == entity) {
            prevEntityList[i] = prevEntityList.back();
            prevEntityList.pop_back();
            // Free memory when needed
            if (prevEntityList.capacity() > prevEntityList.size() + ENTITY_LIST_RESERVE_COUNT) [[unlikely]] {
                prevEntityList.shrink_to_fit();
                prevEntityList.reserve(ENTITY_LIST_RESERVE_COUNT);
            }

            LOG_DEBUG("remove entity {} from chunk {}", (int)entity, prevChunkID);
            found = true;
            break;
        }
    }

    if (!found) [[unlikely]] {
        SimEntityTypeComponent* cmp = mRegistry.try_get<SimEntityTypeComponent>(entity);
        if (!cmp) {
            LOG_CRITICAL("Prev {} state: {} Next {} state: {}", prevChunkID, (int)chunkGrid.getChunk(prevChunkID).getState(), newChunkID, (int)chunkGrid.getChunk(newChunkID).getState());
            panic("Failed to find entity for enter new chunk, type player");
        }
        else {
            LOG_CRITICAL("Prev {} state: {} Next {} state: {}", prevChunkID, (int)chunkGrid.getChunk(prevChunkID).getState(), newChunkID, (int)chunkGrid.getChunk(newChunkID).getState());
            panic("Failed to find entity for enter new chunk, type {}", (int)cmp->type);
        }
    }
    return false;
}

entt::entity IFullECS::createLocalPlayer(f32v3 position, ServerPlayerID playerId) {
    ASSERT_GAME_THREAD();
    entt::entity entity = createEntity(mWorld.getDefaultSpawn(), CStrToken("player"), true);

    mRegistry.emplace<PlayerControlComponent>(entity);
    mRegistry.emplace<ServerPlayerID>(entity, playerId);
    mRegistry.emplace<LocalPlayerComponent>(entity);

    mLocalPlayerEntity = entity;
    return entity;
}

f32v3 IFullECS::getLocalPlayerPosition() {
	ASSERT_GAME_THREAD();
	entt::entity localPlayer = getLocalPlayer();
	if (localPlayer == entt::null) {
        return f32v3(0.0f);
    }
	return mRegistry.get<PositionComponent>(localPlayer).mPosition;
}

void IFullECS::addThreadSafeEntityUpdateForNearestCharacter(AttachedEntityUpdateHandlePtr updateHandle, f32v3 pos) {
    QueuedEntityUpdateAttach update;
    update.target = pos;
    update.type = QueuedEntityUpdateAttach::AttachType::Character;
    update.handle = std::move(updateHandle);
    mDebugEntityUpdater->queueAttachUpdate(std::move(update));
}

i32 IFullECS::pickupTileItem(entt::entity picker, TileItemUID itemUID, i32 quantity) {
    ASSERT_GAME_THREAD();
    assert(quantity > 0);

    auto&& it = mTileItemEntityMap.find(itemUID);
    assert(it != mTileItemEntityMap.end());
    entt::entity entity = it->second;

    DualInventoryComponent& inventoryCmp = mRegistry.get<DualInventoryComponent>(picker);

    if (TileItemComponent* itemCmp = mRegistry.try_get<TileItemComponent>(entity)) {
        assert(quantity <= itemCmp->itemStack.count);
        assert(itemCmp->tileItemUID == itemUID);

        // Add to inventory
        ItemStack newItems = itemCmp->itemStack;
        newItems.count = quantity;
        if (!inventoryCmp.tryAddItemStack(newItems)) {
            return quantity;
        }

        itemCmp->itemStack.count -= quantity;
        const i32 remaining = itemCmp->itemStack.count;

        if (remaining == 0) {
            mTileItemEntityMap.erase(it);
            mRegistry.remove<TileItemComponent>(entity);
            if (StaticModelComponent* staticModel = mRegistry.try_get<StaticModelComponent>(entity)) {
                // Transform static to dynamic pickup object with no physics
                // TODO: RECYCLE BODY
                mRegistry.emplace<DynamicModelComponent>(entity, staticModel->modelId, staticModel->scale);
                // TODO: Put this in a component destroy listener?
                RenderContext::getInstance().removeLooseModelInstance(mWorld, staticModel->modelId, staticModel->staticModelInstanceId);

                mRegistry.remove<StaticModelComponent>(entity);
                StaticPhysicsComponent& staticPhysics = mRegistry.get<StaticPhysicsComponent>(entity);
                mWorld.getPhysicsWorld().removeBody(staticPhysics.mBodyID, true);
                mRegistry.remove<StaticPhysicsComponent>(entity);
            }
            else {
                // Transform dynamic physics object to no physics
                PhysicsComponent& dynamicPhysics = mRegistry.get<PhysicsComponent>(entity);
                mWorld.getPhysicsWorld().removeBody(dynamicPhysics.mBodyID, true);
                mRegistry.remove<PhysicsComponent>(entity);
            }
            mRegistry.emplace<ObjectPickupComponent>(entity, picker);
        }
        return remaining;
    }
    else {
        TileItemContainerComponent& containerCmp = mRegistry.get<TileItemContainerComponent>(entity);
        auto [takenStack, remaining] = containerCmp.takeCount(itemUID, quantity);
        if (remaining == 0) {
            mTileItemEntityMap.erase(it);
            if (containerCmp.isEmpty()) {
                // TODO: PICKUP ANIMATION
                LOG_CRITICAL("TODO: SACK INTERACT");
                destroyEntity(entity);
            }
        }
        if (takenStack.count > 0) {
            inventoryCmp.tryAddItemStack(takenStack);
        }
        return remaining;
    }
}

i32 IFullECS::pickupDynamicItem(entt::entity picker, entt::entity itemEntity, i32 quantity) {
    assert(mRegistry.all_of<DynamicModelComponent>(itemEntity));
    assert(!mRegistry.all_of<TileItemComponent>(itemEntity) && !mRegistry.all_of<TileItemContainerComponent>(itemEntity));

    DualInventoryComponent& inventoryCmp = mRegistry.get<DualInventoryComponent>(picker);

    SimpleItemComponent& itemCmp = mRegistry.get<SimpleItemComponent>(itemEntity);

    assert(quantity <= itemCmp.itemStack.count);
    itemCmp.itemStack.count -= quantity;
    i32 remaining = itemCmp.itemStack.count;

    // Add to inventory
    ItemStack newItems = itemCmp.itemStack;
    newItems.count = quantity;
    inventoryCmp.tryAddItemStack(newItems);

    if (remaining == 0) {
        mRegistry.remove<SimpleItemComponent>(itemEntity);
        // Transform dynamic physics object to no physics
        PhysicsComponent& dynamicPhysics = mRegistry.get<PhysicsComponent>(itemEntity);
        mWorld.getPhysicsWorld().removeBody(dynamicPhysics.mBodyID, true);
        mRegistry.remove<PhysicsComponent>(itemEntity);
        mRegistry.emplace<ObjectPickupComponent>(itemEntity, picker);
    }
    return remaining;
}

void IFullECS::initEvents() {
    LocalChunkGrid& chunkGrid = mWorld.getLocalChunkGrid();
    chunkGrid.registerChunkGridListeners(mChunkEventListeners);

	// Synchronously activate entities on chunk activated
    chunkGrid.addActivatedListener(mChunkEventListeners, [this](ChunkGridEvent& evnt) {
        ASSERT_GAME_THREAD();
        LocalChunk& chunk = evnt.chunk;
        auto&& it = mPendingEntities.find(chunk.getChunkID());
		if (it != mPendingEntities.end()) {
			createFullEntitiesFromSimEntities(chunk, it->second);
			mPendingEntities.erase(it);
		}
    });

    mWorld.registerWorldListeners(mWorldEventListeners);
    mWorld.addOnEntityCreatedListener(mWorldEventListeners,
        [this](const WorldEntityEvent& event) {
        ASSERT_GAME_THREAD();
        ChunkID chunkId = mRegistry.get<PositionComponent>(event.entity).chunkId;
        assert(chunkId != INVALID_CHUNK_ID);
        mEntitiesByChunk[chunkId].emplace_back(event.entity);
        if (TileItemComponent* itemCmp = mRegistry.try_get<TileItemComponent>(event.entity)) {
            assert(itemCmp->tileItemUID != INVALID_TILE_ITEM_UID);
            mTileItemEntityMap[itemCmp->tileItemUID] = event.entity;
        }
        else if (TileItemContainerComponent* itemCmp = mRegistry.try_get<TileItemContainerComponent>(event.entity)) {
            for (const ItemStackWithUID& stack : itemCmp->mItemStacks) {
                mTileItemEntityMap[stack.tileItemUID] = event.entity;
            }
        }
    });

    // Item events
    if (mWorld.isHostWorld()) {

        mWorld.addOnItemProjectileLandListener(mWorldEventListeners,
            [this](const WorldEntityEvent& event) {
            assert(false);
            //ASSERT_GAME_THREAD();
            //PositionComponent& posCmp = mRegistry.get<PositionComponent>(event.entity);
            //assert(posCmp.chunkId != INVALID_CHUNK_ID);

            //SimChunkGrid& simChunkGrid = mWorld.getSimChunkGrid();
            //SimpleItemComponent& itemCmp = mRegistry.get<SimpleItemComponent>(event.entity);

            //TileItemComponent& tileItemCmp = mRegistry.emplace<TileItemComponent>(
            //    event.entity,
            //    itemCmp.itemStack,
            //    simChunkGrid.onItemProjectileLandGameThread(itemCmp.itemStack, posCmp.mPosition
            //));
            //mTileItemEntityMap[tileItemCmp.tileItemUID] = event.entity;

            //// Check if we landed in a new chunk and update accordingly
            //const ChunkID landedChunk = mWorld.getChunkIDAtWorldPos(posCmp.mPosition);
            //if (landedChunk != posCmp.chunkId) {
            //    posCmp.chunkId = landedChunk;
            //    // May end up destroying the item entity if we landed on sim chunk
            //    onEntityEnterNewChunk(event.entity, posCmp.chunkId, landedChunk);
            //}
        });

        mWorld.getPhysicsWorld().registerPhysicsWorldListeners(mPhysicsWorldListeners);
        mWorld.getPhysicsWorld().addItemAtRestListener(mPhysicsWorldListeners, [this](const PhysicsWorldEvent& event) {
            connectItemToChunk(event.entity);
        });
        mWorld.getPhysicsWorld().addItemMovedListener(mPhysicsWorldListeners, [this](const PhysicsWorldEvent& event) {
            ASSERT_GAME_THREAD();
            // First move will not have item component
            if (TileItemComponent* tileItemCmp = mRegistry.try_get<TileItemComponent>(event.entity)) {
                PositionComponent& posCmp = mRegistry.get<PositionComponent>(event.entity);
                assert(posCmp.chunkId != INVALID_CHUNK_ID);

                SimChunkGrid& simChunkGrid = mWorld.getSimChunkGrid();

                mTileItemEntityMap.erase(tileItemCmp->tileItemUID);
                mWorld.getSimChunkGrid().untrackItem(tileItemCmp->getTileItemUID(), tileItemCmp->getItemStack().id, posCmp.mPosition);

                mRegistry.remove<TileItemComponent>(event.entity);
            } else if (TileItemContainerComponent* containerCmp = mRegistry.try_get<TileItemContainerComponent>(event.entity)) {
                for (const ItemStackWithUID& stack : containerCmp->mItemStacks) {
                    mTileItemEntityMap.erase(stack.tileItemUID);
                    mWorld.getSimChunkGrid().untrackItem(stack.tileItemUID, stack.itemStack.id, mRegistry.get<PositionComponent>(event.entity).mPosition);
                }
                mRegistry.remove<TileItemContainerComponent>(event.entity);
            }
        });
    }

    mWorld.addOnEntityDestroyedListener(mWorldEventListeners,
        [this](const WorldEntityEvent& event) {
        ASSERT_GAME_THREAD();
        mDebugEntityUpdater->unregisterAllHandlesForEntity(mRegistry, event.entity);

        if (TileItemComponent* itemCmp = mRegistry.try_get<TileItemComponent>(event.entity)) {
            if (itemCmp->getTileItemUID() != INVALID_TILE_ITEM_UID) {
                mTileItemEntityMap.erase(itemCmp->getTileItemUID());
            }
        } else if (TileItemContainerComponent* itemCmp = mRegistry.try_get<TileItemContainerComponent>(event.entity)) {
            for (const ItemStackWithUID& stack : itemCmp->mItemStacks) {
                mTileItemEntityMap.erase(stack.tileItemUID);
            }
        }

        ChunkID chunkId = mRegistry.get<PositionComponent>(event.entity).chunkId;
        if (chunkId != INVALID_CHUNK_ID) {
            EntityVector& entitiesInChunk = mEntitiesByChunk[chunkId];
            for (size_t i = 0; i < entitiesInChunk.size(); ++i) {
                if (entitiesInChunk[i] == event.entity) {
                    entitiesInChunk[i] = entitiesInChunk.back();
                    entitiesInChunk.pop_back();
                    return;
                }
            }
        }
    });
}

void IFullECS::connectItemToChunk(entt::entity entity) {
    ASSERT_GAME_THREAD();
    PositionComponent& posCmp = mRegistry.get<PositionComponent>(entity);
    assert(posCmp.chunkId != INVALID_CHUNK_ID);

    SimpleItemComponent& itemCmp = mRegistry.get<SimpleItemComponent>(entity);
    if (itemCmp.itemStack.count == 1) {
        // Single item
        TileItemComponent& tileItemCmp = mRegistry.emplace<TileItemComponent>(
            entity,
            itemCmp.itemStack,
            mWorld.getSimChunkGrid().connectItemEntityToGroundGameThreadNoMerge(itemCmp.itemStack, posCmp.mPosition
        ));
        mTileItemEntityMap[tileItemCmp.tileItemUID] = entity;
    }
    else {
        // Container
        TileItemContainerComponent& newContainer = mRegistry.emplace<TileItemContainerComponent>(
            entity,
            itemCmp.itemStack,
            mWorld.getSimChunkGrid().connectItemEntityToGroundGameThreadNoMerge(itemCmp.itemStack, posCmp.mPosition
        ));
        mTileItemEntityMap[newContainer.mItemStacks[0].tileItemUID] = entity;
    }

    // Check if we landed in a new chunk and update accordingly
    const ChunkID landedChunk = mWorld.getChunkIDAtWorldPos(posCmp.mPosition);
    if (landedChunk != posCmp.chunkId) {
        posCmp.chunkId = landedChunk;
        // May end up destroying the item entity if we landed on sim chunk
        onEntityEnterNewChunk(entity, posCmp.chunkId, landedChunk);
    }
}