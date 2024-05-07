#include "stdafx.h"
#include "IEntityComponentSystem.h"

#include "world/World.h"
#include "world/IHeightmapGrid.h"
#include "world/IChunkGrid.h"
#include "ecs/component/FullEntityBindingComponent.h"
#include "ecs/component/SimEntityTypeComponent.h"
#include "camera/Camera3D.h"

// TODO: Get rid of this
#include "rendering/RenderContext.h"

const float DEAD_COLOR_MULT = 0.4f;

constexpr ui32 ENTITY_LIST_RESERVE_COUNT = 64;
// Prevent lists getting too out of control
constexpr ui32 ENTITY_LIST_DEALLOCATE_COUNT = 512;

IEntityComponentSystem::IEntityComponentSystem(World& world) : mWorld(world) {
	mEntitiesByChunk.resize(world.getTotalChunks());
	initEvents();

    // Prevent allocations
    for (auto& list : mEntitiesByChunk) {
        list.reserve(ENTITY_LIST_RESERVE_COUNT);
    }
}

IEntityComponentSystem::~IEntityComponentSystem() {

}

void IEntityComponentSystem::tick(f32 elapsedSec) {
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

}

void IEntityComponentSystem::tickPhysics(f32 elapsedSec) {
	mPhysicsSystem.update(mWorld, mRegistry);
	mCharacterControlSystem.update(mRegistry);
}

void IEntityComponentSystem::addPendingEntitiesToChunk(Chunk& chunk, ChunkEntityFullActivateDataList&& entities) {
	ASSERT_GAME_THREAD();
	ChunkID chunkId = chunk.getChunkID();
	auto&& it = mPendingEntities.find(chunkId);
	if (it == mPendingEntities.end()) {
        mPendingEntities.emplace(chunkId, std::move(entities));
	}
	else {
		it->second.insert(it->second.end(), entities.begin(), entities.end());
	}
}

void IEntityComponentSystem::createFullEntitiesFromSimEntities(Chunk& chunk, const ChunkEntityFullActivateDataList& entities) {
    ASSERT_GAME_THREAD();
    const IHeightmapGrid& heightGrid = mWorld.getHeightmapGrid();
    for (const EntityFullActivateData& activateData : entities) {
        // TODO: I dont think we need thread safe here as main thread is only writer?
        const f32 zPos = heightGrid.computeHeightAtPoint<true>(activateData.simPosition);
        entt::entity newEntity = entt::null;
        switch (activateData.entityType) {
            case SimEntityType::Person: {
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
        static_assert(e_count(SimEntityType) == 4);
    }
}

ChunkEntityFullDeactivateDataList IEntityComponentSystem::deactivateEntitiesForChunk(Chunk& chunk) {
    ASSERT_GAME_THREAD();
	EntityVector& chunkEntities = mEntitiesByChunk[chunk.getChunkID()];
	ChunkEntityFullDeactivateDataList rv;
	rv.reserve(chunkEntities.size());

	std::vector<entt::entity> unboundEntities;

	for (entt::entity e : chunkEntities) {
		// Ignore things with no binding, such as players
		if (FullEntityBindingComponent* bindingCmp = mRegistry.try_get<FullEntityBindingComponent>(e)) [[likely]] {
            EntityFullDeactivateData& ddata = rv.emplace_back();
            ddata.simEntity = bindingCmp->binding->simEntity;
			ddata.simPosition = mRegistry.get<PositionComponent>(e).mPosition;
			assert(mRegistry.get<PositionComponent>(e).chunkId == chunk.getChunkID());
			// TODO: Send anything else?
			destroyEntity(e);
		}
		else {
			unboundEntities.emplace_back(e);
		}
	}
	chunkEntities.swap(unboundEntities);
	chunkEntities.shrink_to_fit();
	return rv;
}

void IEntityComponentSystem::onEntityEnterNewChunk(entt::entity entity, ChunkID prevChunk, ChunkID newChunk) {
    ASSERT_GAME_THREAD();

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
            found = true;
            break;
        }
    }
    assert(found);

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
                e.deactivateData.simEntity = bindingCmp->binding->simEntity;
                e.deactivateData.simPosition = mRegistry.get<PositionComponent>(entity).mPosition;
                dispatchEntityDeactivated(e);
            }
            else {
                // If we get here (rare), we are in the process of activating, so immediately push it into pending and destroy it
                EntityFullActivateData rebuildData;
                rebuildData.binding = bindingCmp->binding;
                rebuildData.entityType = mRegistry.get<SimEntityTypeComponent>(entity).type;
                rebuildData.simPosition = mRegistry.get<PositionComponent>(entity).mPosition;
                mPendingEntities[newChunk].emplace_back(rebuildData);
            }
            destroyEntity(entity);
        }
        else {
            // Non sim entity such as player, just add to the entities by chunk
            mEntitiesByChunk[newChunk].emplace_back(entity);
            LOG_WARN("Added non sim entity {} to deactivated chunk {}", (int)entity, newChunk);
        }
    }
}

entt::entity IEntityComponentSystem::getLocalPlayerThreadSafe() const {
	std::lock_guard lock(mPlayerEntityMutex);
    return mLocalPlayerEntity;
}

void IEntityComponentSystem::setLocalPlayer(entt::entity playerEntity)
{
    ASSERT_GAME_THREAD();
	if (mLocalPlayerEntity != entt::null) {
		mRegistry.remove<PlayerControlComponent>(mLocalPlayerEntity);
	}
    mRegistry.emplace<PlayerControlComponent>(playerEntity);

	std::lock_guard lock(mPlayerEntityMutex);
    mLocalPlayerEntity = playerEntity;
}

f32v3 IEntityComponentSystem::getLocalPlayerPosition() {
	ASSERT_GAME_THREAD();
	entt::entity localPlayer = getLocalPlayer();
	if (localPlayer == entt::null) {
        return f32v3(0.0f);
    }
	return mRegistry.get<PositionComponent>(localPlayer).mPosition;
}

void IEntityComponentSystem::initEvents() {
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
}
