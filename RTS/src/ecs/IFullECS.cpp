#include "stdafx.h"
#include "IFullECS.h"

#include "world/World.h"
#include "world/IHeightmapGrid.h"
#include "world/IChunkGrid.h"
#include "ecs/component/FullEntityBindingComponent.h"
#include "ecs/component/SimEntityTypeComponent.h"
#include "ecs/AttachedEntityUpdater.h"
#include "camera/Camera3D.h"

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
		const Camera3D* camera = RenderContext::getInstance().getCamera();
		if (camera) {
			mPlayerControlSystem.update(mWorld, mRegistry, camera->getYaw());
		}
	}

    mSkillsSystem.update(mWorld, mRegistry, elapsedSec);

	ProjectileSystem::update(mWorld, mRegistry, elapsedSec);

    assert(mDebugEntityUpdater);
    mDebugEntityUpdater->update(mRegistry);
}

void IFullECS::tickPhysics(f32 elapsedSec) {
	mPhysicsSystem.update(mWorld, mRegistry);
	mCharacterControlSystem.update(mRegistry);
}

void IFullECS::addPendingEntitiesToChunk(Chunk& chunk, ChunkFullTransitionData&& data) {

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

void IFullECS::createFullEntitiesFromSimEntities(Chunk& chunk, ChunkFullTransitionData& data) {
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

    // Items
    const f32v2 worldPosChunkWithTileOffset(chunk.getWorldPos().x + 0.5f, chunk.getWorldPos().y + 0.5f);
    for (auto&& [itemID, stacks] : data.itemStacks) {
        for (const TileItemStack& stack : stacks) {
            const f32v2 worldPos(worldPosChunkWithTileOffset + f32v2(stack.tileIndex % CHUNK_WIDTH, stack.tileIndex / CHUNK_WIDTH));
            const f32v3 pos3(worldPos.x, worldPos.y, heightGrid.computeHeightAtPoint<true>(worldPos));
            entt::entity newEntity = EntityFactory::createItemOnGround(mWorld, pos3, stack.toItemStack(itemID), stack.uniqueId);

            // TODO: DELETE ME
            //DebugRenderer::drawWireQuadThreadSafe(pos3, f32v2(1.0f), color::Magenta, 2000);
        }
    }
}

ChunkSimTransitionData IFullECS::deactivateEntitiesForChunk(Chunk& chunk) {
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
                if (TileItemComponent* itemCmp = mRegistry.try_get<TileItemComponent>(e)) {
                    if (itemCmp->getTileItemUID() != INVALID_TILE_ITEM_UID) {
                        mTileItemEntityMap.erase(itemCmp->getTileItemUID());
                    }
                }
                // Destroy everything else, assume it is tracked
                destroyEntity(e);
            }
		}
	}
	chunkEntities.swap(unboundEntities);
	chunkEntities.shrink_to_fit();
	return rv;
}

void IFullECS::onEntityEnterNewChunk(entt::entity entity, ChunkID prevChunk, ChunkID newChunk) {
    ASSERT_GAME_THREAD();

    IChunkGrid& chunkGrid = mWorld.getChunkGrid();
    Chunk& chunk = chunkGrid.getChunk(newChunk);
    if (chunk.isActivated()) {
        mEntitiesByChunk[newChunk].emplace_back(entity);
    }
    else {
        if (FullEntityBindingComponent* bindingCmp = mRegistry.try_get<FullEntityBindingComponent>(entity)) {
            if (chunk.isDeactivated()) {
                // Send to sim thread
                FullECSEvent e;
                e.entity = entity;
                e.chunkId = chunk.getChunkID();
                dispatchEntityDeactivated(e);

                // We will remove from mEntitiesbyChunk in the destroy listener
                destroyEntity(entity);
                return;
            }
            else {
                // If we get here (rare), we are in the process of activating, so pretend we are still in the old chunk and retry next time
                mRegistry.get<PositionComponent>(entity).chunkId = prevChunk;
                return;
            }
        }
        else {
            // Non sim entity such as player, just add to the entities by chunk
            mEntitiesByChunk[newChunk].emplace_back(entity);
            LOG_WARN("Added non sim entity {} to deactivated chunk {}", (int)entity, newChunk);
        }
    }

    // Remove from previous if we did not return early due to setting to previous chunk
    EntityVector& prevEntityList = mEntitiesByChunk[prevChunk];
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

            LOG_DEBUG("remove entity {} from chunk {}", (int)entity, prevChunk);
            found = true;
            break;
        }
    }

    if (!found) [[unlikely]] {
        SimEntityTypeComponent* cmp = mRegistry.try_get<SimEntityTypeComponent>(entity);
        if (!cmp) {
            LOG_CRITICAL("Prev {} state: {} Next {} state: {}", prevChunk, (int)chunkGrid.getChunk(prevChunk).getState(), newChunk, (int)chunkGrid.getChunk(newChunk).getState());
            panic("Failed to find entity for enter new chunk, type player");
        }
        else {
            LOG_CRITICAL("Prev {} state: {} Next {} state: {}", prevChunk, (int)chunkGrid.getChunk(prevChunk).getState(), newChunk, (int)chunkGrid.getChunk(newChunk).getState());
            panic("Failed to find entity for enter new chunk, type {}", (int)cmp->type);
        }
    }
}

entt::entity IFullECS::getLocalPlayerThreadSafe() const {
	std::lock_guard lock(mPlayerEntityMutex);
    return mLocalPlayerEntity;
}

void IFullECS::setLocalPlayer(entt::entity playerEntity)
{
    ASSERT_GAME_THREAD();
	if (mLocalPlayerEntity != entt::null) {
		mRegistry.remove<PlayerControlComponent>(mLocalPlayerEntity);
	}
    mRegistry.emplace<PlayerControlComponent>(playerEntity);

	std::lock_guard lock(mPlayerEntityMutex);
    mLocalPlayerEntity = playerEntity;
}

f32v3 IFullECS::getLocalPlayerPosition() {
	ASSERT_GAME_THREAD();
	entt::entity localPlayer = getLocalPlayer();
	if (localPlayer == entt::null) {
        return f32v3(0.0f);
    }
	return mRegistry.get<PositionComponent>(localPlayer).mPosition;
}

void IFullECS::onItemPickedUp(TileItemUID itemUID, i32 remaining) {
    ASSERT_GAME_THREAD();
    auto&& it = mTileItemEntityMap.find(itemUID);
    assert(it != mTileItemEntityMap.end());
    entt::entity entity = it->second;
    if (remaining) {
        TileItemComponent& itemCmp = mRegistry.get<TileItemComponent>(entity);
        itemCmp.itemStack.count = remaining;
        assert(itemCmp.tileItemUID == itemUID);
    }
    else {
        mTileItemEntityMap.erase(it);
        destroyEntity(entity);
    }
}

void IFullECS::addThreadSafeEntityUpdateForNearestCharacter(AttachedEntityUpdateHandlePtr updateHandle, f32v3 pos) {
    QueuedEntityUpdateAttach update;
    update.target = pos;
    update.type = QueuedEntityUpdateAttach::AttachType::Character;
    update.handle = std::move(updateHandle);
    mDebugEntityUpdater->queueAttachUpdate(std::move(update));
}

void IFullECS::initEvents() {
    IChunkGrid& chunkGrid = mWorld.getChunkGrid();
    chunkGrid.registerChunkGridListeners(mChunkEventListeners);

	// Synchronously activate entities on chunk activated
    chunkGrid.addActivatedListener(mChunkEventListeners, [this](ChunkGridEvent& evnt) {
        ASSERT_GAME_THREAD();
        Chunk& chunk = evnt.chunk;
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
    });
    mWorld.addOnEntityDestroyedListener(mWorldEventListeners,
        [this](const WorldEntityEvent& event) {
        ASSERT_GAME_THREAD();
        mDebugEntityUpdater->unregisterAllHandlesForEntity(mRegistry, event.entity);

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
