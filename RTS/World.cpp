#include "stdafx.h"
#include "World.h"

#include "ecs/EntityComponentSystem.h"
#include "DebugRenderer.h"
#include "world/ChunkGenerator.h"
#include "world/HeightmapTerrainQuadtree.h"
#include "world/TileRepository.h"
#include "weather/CloudManager.h"
#include "physics/ContactListener.h"
#include "physics/ContactFilter.h"
#include "item/ItemStockpileRegistry.h"

#include "ecs/factory/EntityFactory.h"

#include <Vorb/ui/InputDispatcher.h>
#include <Vorb/graphics/SpriteBatch.h>
#include <Vorb/graphics/TextureCache.h>
#include <Vorb/math/VectorMath.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/transform.hpp>

#include "rendering/ChunkMesher.h"
#include "rendering/ChunkGrassQuadtree.h"

#include "ui/UIContext.h"

#include "services/Services.h"

#include <box2d/b2_world.h>
#include <box2d/b2_fixture.h>

#include "physics/PhysQueryCallback.h"
#include "Utils.h"

#include "city/City.h"
#include "camera/Camera3D.h"

#include "generation/WorldGeneration.h"
#include "pathfinding/NavGraph.h"
#include "pathfinding/NavThread.h"

// TODO: remove?
#include "ResourceManager.h"
#include "particles/ParticleSystemManager.h"

#include "util/TileUtil.h"

#include "options/DebugOptions.h"

const float CHUNK_UNLOAD_TOLERANCE = -10.0f; // How many extra blocks we add when checking unload distance


World::World(ResourceManager& resourceManager) :
	mWorldGrid(*this),
	mResourceManager(resourceManager)
{
    // Init generation
    mChunkGenerator = std::make_unique<ChunkGenerator>();

    // Init ECS
    mEcs = std::make_unique<EntityComponentSystem>(*this);

    // Init factories
	mEntityFactory = std::make_unique<EntityFactory>(*mEcs, mResourceManager);

    // Init physics
    mPhysWorld = std::make_unique<b2World>(b2Vec2(0.0f, 0.0f));
    mContactListener = std::make_unique<ContactListener>(*mEcs);
    mPhysWorld->SetContactListener(mContactListener.get());
    mContactFilter = std::make_unique<ContactFilter>(*mEcs);
    mPhysWorld->SetContactFilter(mContactFilter.get());

	// Cities
	mCities = std::make_unique<CityGraph>();

	// Stockpiles
	mItemStockpileRegistry = std::make_unique<ItemStockpileRegistry>(*this);

	// Nav graph
	mNavGraph = std::make_unique<NavGraph>(*this);

    // Weather (Init post load because it contains rendering and requires render context to be initialized, TODO: Fix this)
    mCloudManager = std::make_unique<CloudManager>(*this);

	// Activate the nav thread
	Services::NavThread::ref().init(*this);
}

World::~World() {
	IS_SHUTTING_DOWN = true;
}

void World::initPostLoad(ChunkMesher& chunkMesher) {
	mChunkMesher = &chunkMesher;

	// Init terrain
	mTerrainTrees.resize(WORLD_SIZE_TERRAIN_QUADTREES);
	for (size_t i = 0; i < mTerrainTrees.size(); ++i) {
		f32v2 pos((i % WORLD_WIDTH_TERRAIN_QUADTREES) * TERRAIN_QUADTREE_WIDTH, (i / WORLD_WIDTH_TERRAIN_QUADTREES) * TERRAIN_QUADTREE_WIDTH);
		mTerrainTrees[i].init(pos, mWorldGrid);
	}
}

void World::updateTaskQueues() {
    Services::Threadpool::ref().mainThreadUpdate();
    Services::NavThread::ref().mainThreadUpdate();

	// Update any pending updates if pathfinding is idle
	if (!Services::NavThread::ref().isRunningPathfind()) {
		for (auto&& chunk : mActiveChunks) {
			if (chunk->mTilesNeedingThreadSafeCopy.size() && chunk->mReadLockCount == 0) {
				for (TileIndex& id : chunk->mTilesNeedingThreadSafeCopy) {
					chunk->mTiles[id].updateThreadSafeLayers();
				}
				chunk->mTilesNeedingThreadSafeCopy.clear();
				chunk->dirtyMesh();
				chunk->dirtyNavGraph(); // TODO: Make this smarter
			}
		}
	}
}

void World::tick(const f32v2& playerPos) {
	assert(mEcs);

	if (sWorldGen.mIsDirty) {
		sWorldGen.mIsDirty = false;
		debugRefreshWorldGeneration();
	}

	updateSun();

	mLoadCenter = playerPos;

	// TODO: More explicit initialization?
	if (mNeedsLazyInit) {
		lazyInit();
	}
	
	// TODO: This now asserts out of bounds
	Chunk& playerChunk = getChunkAtPosition(playerPos);
	if (playerChunk.isInvalid()) {
		initChunk(playerChunk);
	}

    for (size_t i = 0; i < mActiveChunks.size();) {
        Chunk& chunk = *mActiveChunks[i];
        if (updateChunk(chunk)) {
			mWorldGrid.releaseHeightDataAt(chunk.getChunkID());
            chunk.dispose();
			mActiveChunks[i] = mActiveChunks.back();
			mActiveChunks.pop_back();
			continue;
        }
        ++i;
    }

	// Update cities
	// TODO: Amortized
	for (auto&& it : mCities->mNodes) {
		it->update();
	}

	// TODO: Give physworld ownership to physicssystem?
	mEcs->mPhysicsSystem.updateFrameBegin(mEcs->mRegistry);

	// Update physics
	mPhysWorld->Step(1.0f /*deltaTime*/, 1, 1);

	// Update particles (TODO: Ecs?)
	mResourceManager.getParticleSystemManager().update(playerPos);

	// Update weather
	mCloudManager->update();

	// Update terrain
	for (auto&& terrainQuadtree : mTerrainTrees) {
		terrainQuadtree.update(playerPos);
	}

	// Update ECS
	// TODO: Move out
    mEcs->tick();
}

void World::frameUpdate(const Camera3D& camera) {
	mEcs->frameUpdate(camera);

	// Client only, rendering stuff
    mVisibleChunks.clear();
	for (Chunk* chunk : mActiveChunks) {
        // Determine visibility
        if (chunk->isDataReady()) {
            const f32v2& worldPos = chunk->getWorldPos();
            if (camera.sphereIsVisible(f32v3(worldPos.x + HALF_CHUNK_WIDTH, worldPos.y + HALF_CHUNK_WIDTH, 0.0f), CHUNK_DIAGONAL_RADIUS + 30.0f /*padding for camera pan fix :C WHY*/)) { // TODO: Broken + AABB Test?
                mVisibleChunks.push_back(chunk);
                chunk->mChunkRenderData.mIsVisible = true;
            }
            else {
                chunk->mChunkRenderData.mIsVisible = false;
            }
        }
	}
}

void World::lazyInit() {
    mCloudManager->init();
	mNeedsLazyInit = false;
}

Chunk& World::getChunkAtPosition(const f32v2& worldPos) {
	return getChunk(ChunkID(worldPos));
}

const Chunk& World::getChunkAtPosition(const f32v2& worldPos) const {
    return getChunk(ChunkID(worldPos));
}

Chunk& World::getChunkAtPosition(const ui32v2& worldPos) {
    return getChunk(ChunkID::fromWorldUI32v2(worldPos));
}

const Chunk& World::getChunkAtPosition(const ui32v2& worldPos) const {
    return getChunk(ChunkID::fromWorldUI32v2(worldPos));
}

Chunk& World::getChunkAtPosition(const ui16v2& worldPos) {
    return getChunk(ChunkID::fromWorldUI16v2(worldPos));
}

const Chunk& World::getChunkAtPosition(const ui16v2& worldPos) const {
    return getChunk(ChunkID::fromWorldUI16v2(worldPos));
}

Chunk& World::getChunk(ChunkID chunkId) {
	assert(chunkId.id < WorldData::WORLD_SIZE_CHUNKS);
	return mWorldGrid.getChunk(chunkId.id);
}

const Chunk& World::getChunk(ChunkID chunkId) const {
	assert(chunkId.id < WorldData::WORLD_SIZE_CHUNKS);
    return mWorldGrid.getChunk(chunkId.id);
}

Chunk& World::getChunk(ui32 chunkId) {
    assert(chunkId < WorldData::WORLD_SIZE_CHUNKS);
    return mWorldGrid.getChunk(chunkId);
}

const Chunk& World::getChunk(ui32 chunkId) const {

    assert(chunkId < WorldData::WORLD_SIZE_CHUNKS);
    return mWorldGrid.getChunk(chunkId);
}

Chunk& World::getChunkAtChunkCoords(const ui32v2& worldPos) {
    return getChunk(ChunkID(worldPos));
}

inline f32 fastFloorf(f32 x) {
    return FastConversion<f32, f32>::floor(x);
}
inline f32 fastCeilf(f32 x) {
    return FastConversion<f32, f32>::ceiling(x);
}

TileHandle World::getTileFromCameraPickVector(const Camera3D& camera, const f32v3& rayDir) const {
	assert(false); // NO LONGER IMPLEMENTED
	return TileHandle();
}

TileHandle World::getTileHandleAtWorldPos(const f32v2& worldPos) const {
	TileHandle handle;
	const Chunk* chunk = &getChunkAtPosition(worldPos);
	if (chunk->isDataReady()) {
		ui32 x = (ui32)worldPos.x & (CHUNK_WIDTH - 1); // Fast modulus
		ui32 y = (ui32)worldPos.y & (CHUNK_WIDTH - 1); // Fast modulus
		return chunk->getTileHandleAt(TileIndex(x, y));
	}
	return TileHandle();
}

TileHandle World::getTileHandleAtWorldPos(const ui32v2& worldPos) const {
    TileHandle handle;
    const Chunk* chunk = &getChunkAtPosition(worldPos);
    if (chunk->isDataReady()) {
        ui32 x = (ui32)worldPos.x & (CHUNK_WIDTH - 1); // Fast modulus
        ui32 y = (ui32)worldPos.y & (CHUNK_WIDTH - 1); // Fast modulus
        return chunk->getTileHandleAt(TileIndex(x, y));
    }
    return TileHandle();
}

TileHandle World::getTileHandle(ui32 chunkId, TileIndex tileIndex) const {
    TileHandle handle;
    const Chunk* chunk = &getChunk(chunkId);
    if (chunk->isDataReady()) {
        return chunk->getTileHandleAt(tileIndex);
    }
    return TileHandle();
}

const Tile& World::getTileAtWorldPos(const f32v2& worldPos) const {
    const Chunk* chunk = &getChunkAtPosition(worldPos);
	assert(chunk->isDataReady());
    ui32 x = (ui32)worldPos.x & (CHUNK_WIDTH - 1); // Fast modulus
    ui32 y = (ui32)worldPos.y & (CHUNK_WIDTH - 1); // Fast modulus
    return chunk->getTileAt(TileIndex(x, y));
}

const Tile* World::tryGetTileAtWorldPos(const f32v2& worldPos) const {
    const Chunk* chunk = &getChunkAtPosition(worldPos);
    if (chunk->isDataReady()) {
        ui32 x = (ui32)worldPos.x & (CHUNK_WIDTH - 1); // Fast modulus
        ui32 y = (ui32)worldPos.y & (CHUNK_WIDTH - 1); // Fast modulus
        return &chunk->getTileAt(TileIndex(x, y));
    }
    return nullptr;
}

const Tile* World::tryGetTileAtWorldPos(const ui32v2& worldPos) const {
	const Chunk* chunk = &getChunkAtPosition(worldPos);
	if (chunk->isDataReady()) {
		ui32 x = (ui32)worldPos.x & (CHUNK_WIDTH - 1); // Fast modulus
		ui32 y = (ui32)worldPos.y & (CHUNK_WIDTH - 1); // Fast modulus
		return &chunk->getTileAt(TileIndex(x, y));
	}
	return nullptr;
}

const Tile* World::tryGetTileAtWorldPos(const ui16v2& worldPos) const {
    const Chunk* chunk = &getChunkAtPosition(worldPos);
    if (chunk->isDataReady()) {
        ui32 x = (ui32)worldPos.x & (CHUNK_WIDTH - 1); // Fast modulus
        ui32 y = (ui32)worldPos.y & (CHUNK_WIDTH - 1); // Fast modulus
        return &chunk->getTileAt(TileIndex(x, y));
    }
    return nullptr;
}

const NavNode* World::tryGetNavNodeAtWorldPos(const ui32v2& worldPos) const {
	const Chunk& chunk = getChunkAtPosition(worldPos); // TODO: Stop casting??
	if (!chunk.isDataReady()) return nullptr;
    ui32 x = (ui32)worldPos.x & (CHUNK_WIDTH - 1); // Fast modulus
    ui32 y = (ui32)worldPos.y & (CHUNK_WIDTH - 1); // Fast modulus
	const Tile& tile = chunk.getTileAt(TileIndex(x, y));
	ui16 navNodeIndex = tile.getNavNodeIndex();
	if (navNodeIndex == INVALID_NAV_NODE_INDEX) return nullptr;
	return mNavGraph->getNode({ chunk.getChunkID().id, navNodeIndex });
}

void World::enumVisibleChunks(std::function<void(const Chunk& chunk)> func) const {
	// TODO: Might be smart to make a variant that doesnt need an std::function for faster iteration/calls since
	// we call this many times
	for (auto&& chunk : mVisibleChunks) {
		func(*chunk);
	}
}

void World::enumActiveChunks(std::function<void(const Chunk&)> func) const {
    for (auto&& chunk : mActiveChunks) {
        func(*chunk);
    }
}

void World::efficientEnumTileAABB(const ui32AABB2& aabb, std::function<void(Chunk&, TileIndex)> func) {
	assert(IS_MAIN_THREAD());
	// TODO: handle this without asserts
	// Start at bottom left
	ui32v2 worldPos;
	ui32 spanX = CHUNK_WIDTH; // Logically these initial values wont actually be used, but need to please compiler
	ui32 spanY = CHUNK_WIDTH;
	for (worldPos.y = aabb.y; worldPos.y < aabb.y + aabb.height;) {
        for (worldPos.x = aabb.x; worldPos.x < aabb.x + aabb.height;) {
            TileHandle cornerHandle = getTileHandleAtWorldPos(worldPos);
            assert(cornerHandle.chunk && cornerHandle.chunk->isDataReady());
            Chunk& chunk = *cornerHandle.getMutableChunk();
            const ui32 distFromRightEdge = CHUNK_WIDTH - cornerHandle.index.getX();
            const ui32 distFromTopEdge = CHUNK_WIDTH - cornerHandle.index.getY();
            spanX = std::min(distFromRightEdge, aabb.width);
            spanY = std::min(distFromTopEdge, aabb.height); // TODO: Prob clever way to move this up a loop
            const ui32 x = cornerHandle.index.getX();
            const ui32 y = cornerHandle.index.getY();
			for (ui32 dy = 0; dy < spanY; ++dy) {
				for (ui32 dx = 0; dx < spanX; ++dx) {
					func(chunk, TileIndex(x + dx, y + dy));
				}
			}
			worldPos.x += spanX;
		}
		worldPos.y += spanY;
	}
}

void World::dirtyTerrainFromBrush(const f32v2& pos, f32 brushRadius) {
    for (auto&& quadtree : mTerrainTrees) {
        quadtree.onDataChanged(pos, brushRadius);
    }
    for (Chunk* chunk : mActiveChunks) {
		chunk->onTerrainDataChanged(pos, brushRadius);
    }
}

void World::setTimeOfDay(float time) {
	assert(time >= 0.0f && time <= HOURS_PER_DAY);

	// Offset debug time
	const f64 timeOffset = time - mTimeOfDay;
	sDebugOptions.mTimeOffset += timeOffset * SECONDS_PER_HOUR;

}

City* World::getClosestCityToPoint(const f32v2& pos) const
{
	City* closest = nullptr;
	f32 closestDist2 = FLT_MAX;
	for (auto&& city : mCities->mNodes) {
		const f32 dist2 = glm::length2(f32v2(city->getCityCenterWorldPos()) - pos);
		if (dist2 < closestDist2) {
			closestDist2 = dist2;
			closest = city.get();
		}
	}
	return closest;
}

IntersectionHit2D World::tryGetRaycastIntersect2D(const f32v2& start, const f32v2& end, f32 zPos)
{

    //Find All Distances To Next Voxel In Each Direction
	f32 currDist = 0.0f;
	f32v2 currentPos = start;
	// TODO: do we care about the world wrap lol
	ui32v2 currentCellPos = currentPos;
	const f32v2 offset = end - start;
	const f32 rayLength = glm::length(offset);
	const f32v2 direction = offset / rayLength;

	while (currDist < rayLength) {
		f32v2 next;
		f32v2 r;
		// X-Distance
		if (direction.x > 0) {
			if (currentPos.x == (i32)currentPos.x) next.x = currentPos.x + 1;
			else next.x = fastCeilf(currentPos.x);
			r.x = (next.x - currentPos.x) / direction.x;
		}
		else if (direction.x < 0) {
			if (currentPos.x == (i32)currentPos.x) next.x = currentPos.x - 1;
			else next.x = fastFloorf(currentPos.x);
			r.x = (next.x - currentPos.x) / direction.x;
		}
		else {
			r.x = FLT_MAX;
		}

		// Y-Distance
		if (direction.y > 0) {
			if (currentPos.y == (i32)currentPos.y) next.y = currentPos.y + 1;
			else next.y = fastCeilf(currentPos.y);
			r.y = (next.y - currentPos.y) / direction.y;
		}
		else if (direction.y < 0) {
			if (currentPos.y == (i32)currentPos.y) next.y = currentPos.y - 1;
			else next.y = fastFloorf(currentPos.y);
			r.y = (next.y - currentPos.y) / direction.y;
		}
		else {
			r.y = FLT_MAX;
		}

		// Get minimum movement to the next cell
		f32 rat;
		if (r.x < r.y) {
			// Move In The X-Direction
			rat = r.x;
			currentPos += direction * rat;
			if (direction.x > 0) ++currentCellPos.x;
			else if (direction.x < 0) --currentCellPos.x;
		}
		else {
			// Move In The Y-Direction
			rat = r.y;
			currentPos += direction * rat;
			if (direction.y > 0) ++currentCellPos.y;
			else if (direction.y < 0) --currentCellPos.y;
		}

		const Tile* tile = tryGetTileAtWorldPos(currentCellPos);

		// Check collision
		// TODO: Pass in collision radius
		if (tile) {
			IntersectionHit2D hit = TileUtil::tryRayTileIntersect(*tile, currentCellPos, start, end, zPos, 0.3f);
			if (hit.didHit()) {
				return hit;
			}
		}

		// Add The Distance The Ray Has Traversed
		currDist += rat;
	}

	return IntersectionHit2D();
}

void World::updateSun() {
    const float SUNRISE_TIME = 6.0f; // 6am
	const float SUN_HEIGHT_OFFSET = 0.3f; // Smaller exponent means brighter days
	// TODO: Better time manager
	const f64 adjustedTime = /*sTotalTimeSeconds + */sDebugOptions.mTimeOffset;
    mTimeOfDay = (float)fmod(adjustedTime / (f64)SECONDS_PER_HOUR, (f64)HOURS_PER_DAY);

	const f32 sunDelta = (mTimeOfDay - SUNRISE_TIME) / 24.0f;
	const f32 sunRotate = sunDelta * M_PIF * 2.0f;
    mSunPosition = glm::rotateY(f32v3(-1.0f, 0.0f, 0.0f), sunRotate);
    mSunHeight = glm::min(mSunPosition.z + SUN_HEIGHT_OFFSET, 0.999f); // Store sun height before modification, cap at an epsilon to fix sampler issue
    mSunPosition.z += 0.2f; // Make it more up lol
    mSunPosition = glm::normalize(mSunPosition);

	mSkyRotMatrix = glm::rotate(sunRotate, f32v3(0.0f, 1.0f, 0.0f));

	// Colors
    f32v3 sunSet(1.0f, 0.5f, 0.0f);
    f32v3 sunPeak(1.0f, 1.0f, 1.0f);
    const float c = vmath::max(mSunHeight, 0.0f);
	mSunColor = f32v3(
        vmath::lerp(sunSet.r, sunPeak.r, c),
		vmath::lerp(sunSet.g, sunPeak.g, c),
		vmath::lerp(sunSet.b, sunPeak.b, c)
	);
}

bool World::updateChunk(Chunk& chunk) {
	if (!isChunkInLoadDistance(chunk.getWorldPos(), CHUNK_UNLOAD_TOLERANCE)) {
		if (chunk.mRefCount.load()) {
			// Waiting on a thread or handle to release us
			return false;
		}
		// Unload
		return true;
	}

	if (chunk.isDataReady()) {
		// Check for new neighbors
		if (chunk.mDataReadyNeighborCount < CHUNK_NEIGHBOR_COUNT) {
			tryCreateNeighbors(chunk);
		}
		else if (chunk.mDirtyNavGraph && chunk.mIsNavmeshing.load(/*memory order relaxed?*/) == false) {
            // Update nav graph when all neighbors are loaded
			// TODO: Async?
            chunk.mDirtyNavGraph = false;
			Services::NavThread::ref().addNavgraphBuildTask(chunk);
		}
		else {
			// Update grass
            const f32v2 centerPos = chunk.getWorldPos() + f32v2(HALF_CHUNK_WIDTH);
            const f32v2 offset = centerPos - mLoadCenter;
			const f32 distSq = glm::length2(offset);

			if (chunk.mChunkRenderData.mGrassLod) {
                if (distSq > sDebugOptions.mGrassSettings.distanceSq + 10.0f) {
					if (chunk.mChunkRenderData.mGrassLod->getRefCount() == 0) {
						chunk.mChunkRenderData.mGrassLod.reset();
					}
				}
				else {
					chunk.mChunkRenderData.mGrassLod->update(mLoadCenter);
				}
			}
			else {
                if (distSq < sDebugOptions.mGrassSettings.distanceSq) {
					chunk.mChunkRenderData.mGrassLod = std::make_unique<ChunkGrassQuadtree>(chunk, mWorldGrid);
                }
			}
            
		}
		
	}
	else if (chunk.mRefCount.load() == 0) {
		// If we are not in use, we are done generating
		onChunkDataReady(chunk);
	}
	
	return false;
}

void World::onChunkDataReady(Chunk& chunk) {
    assert(!chunk.isDataReady());

    chunk.setState(ChunkState::FINISHED);
	// Don't update neighbors until we are data ready
	assert(chunk.isDataReady());
	// Neighbors
    const ChunkID& myId = chunk.getChunkID();
    dataReadyTryNotifyNeighbor(chunk, myId.getBottomID());
    dataReadyTryNotifyNeighbor(chunk, myId.getLeftID());
    dataReadyTryNotifyNeighbor(chunk, myId.getRightID());
    dataReadyTryNotifyNeighbor(chunk, myId.getTopID());
	
	assert(chunk.mDataReadyNeighborCount <= CHUNK_NEIGHBOR_COUNT);
}

void World::onChunkAllNeighborsDataReady(Chunk& chunk) {
    assert(chunk.getBottomNeighbor().isDataReady());
    assert(chunk.getLeftNeighbor().isDataReady());
    assert(chunk.getRightNeighbor().isDataReady());
    assert(chunk.getTopNeighbor().isDataReady());

	// Dirty our nav graph
    chunk.mDirtyNavGraph = true;
	// Update our mesh
    chunk.dirtyMesh();
    mChunkMesher->updateMesh(chunk, f32v3(mLoadCenter, 0.0f));
}

void World::dataReadyTryNotifyNeighbor(Chunk& chunk, const ChunkID& id) {
	Chunk& neighbor = mWorldGrid.getChunk(id.id);
    if (neighbor.isDataReady()) {
		// Set up data ready ref counts
		++neighbor.mDataReadyNeighborCount;
		assert(neighbor.mDataReadyNeighborCount <= CHUNK_NEIGHBOR_COUNT);
        ++chunk.mDataReadyNeighborCount;
        assert(chunk.mDataReadyNeighborCount <= CHUNK_NEIGHBOR_COUNT);
		if (neighbor.mDataReadyNeighborCount == CHUNK_NEIGHBOR_COUNT) {
			onChunkAllNeighborsDataReady(neighbor);
		}
        if (chunk.mDataReadyNeighborCount == CHUNK_NEIGHBOR_COUNT) {
            onChunkAllNeighborsDataReady(chunk);
		}
	}
	else if (neighbor.isInvalid() && isChunkInLoadDistance(id)) {
		// Create the chunk, but dont update neighbor count until its done
		initChunk(neighbor);
	}
}

void World::tryCreateNeighbors(Chunk& chunk) {
    // Neighbors
    const ChunkID& myId = chunk.getChunkID();
	tryCreateNeighbor(chunk, myId.getLeftID());
	tryCreateNeighbor(chunk, myId.getTopID());
	tryCreateNeighbor(chunk, myId.getRightID());
	tryCreateNeighbor(chunk, myId.getBottomID());
}

void World::tryCreateNeighbor(Chunk& chunk, const ChunkID& id) {
    Chunk& neighbor = mWorldGrid.getChunk(id.id);
    if (neighbor.isInvalid() && isChunkInLoadDistance(id)) {
        // Create the chunk, but dont update neighbor count until its done
        initChunk(neighbor);
    }
}

bool World::isChunkInLoadDistance(const ChunkID& chunkPos, float addOffset /* = 0.0f*/)
{
	const f32v2 centerPos = chunkPos.getWorldPos() + f32v2(HALF_CHUNK_WIDTH);
	const f32v2 offset = centerPos - mLoadCenter;

	return glm::length2(offset) <= sDebugOptions.mLoadRangeSq + addOffset;
}

void World::initChunk(Chunk& chunk)
{
	const ChunkID& chunkId = chunk.getChunkID();
    // If this is a sentinel chunk, stop here
    if (chunkId.isSentinelID()) {
        return;
    }

    generateChunkAsync(chunk);
}

void World::generateChunkAsync(Chunk& chunk) {
    chunk.incRef();
	// TODO: should we be inactive?
    mActiveChunks.push_back(&chunk);
	const ChunkID& id = chunk.getChunkID();

	if (mWorldGrid.tryGetHeightDataAt(id)) {
        chunk.mState.store(e_cast(ChunkState::LOADING_TILES));
		const HeightmapPatchData* heightData = mWorldGrid.aquireHeightData(id);
        Services::Threadpool::ref().addTask([&, heightData](ThreadPoolWorkerData* workerData) {
            mChunkGenerator->GenerateChunk(chunk, mWorldGrid, heightData);
            chunk.decRef();
        }, nullptr);
	}
	else {
        chunk.mState.store(e_cast(ChunkState::WAITING_HEIGHT));
		mWorldGrid.requestHeightDataGenAndAquireAt(id, [this, &chunk]() {
            chunk.mState.store(e_cast(ChunkState::LOADING_TILES));
			const HeightmapPatchData* heightData = mWorldGrid.getHeightDataAt(chunk.getChunkID());
            Services::Threadpool::ref().addTask([&, heightData](ThreadPoolWorkerData* workerData) {
                mChunkGenerator->GenerateChunk(chunk, mWorldGrid, heightData);
                chunk.decRef();
            }, nullptr);
		});
	}

}

void World::editorInvalidateWorldGen() {
	initPostLoad(*mChunkMesher);
}

void World::debugRefreshWorldGeneration() {
	// Tell terain to regenerate
    for (size_t i = 0; i < mTerrainTrees.size(); ++i) {
		mTerrainTrees[i].markDirty();
    }
}

std::vector<EntityDistSortKey> World::queryActorsInRadius(const f32v2& pos, float radius, ActorTypesMask includeMask, ActorTypesMask excludeMask, bool sorted, entt::entity except /*= (entt::entity)0*/) {
	// TODO: No allocation?

	// Empty mask = all types
	if (includeMask == 0) {
		includeMask = ~0;
	}

	// TODO: Components as well? Better lookup?
	std::vector<EntityDistSortKey> entities;

	PhysQueryCallback queryCallBack(entities, pos, mEcs->mRegistry, includeMask, excludeMask, radius, except);
	b2AABB aabb;
	aabb.lowerBound = b2Vec2(pos.x - radius, pos.y - radius);
	aabb.upperBound = b2Vec2(pos.x + radius, pos.y + radius);
	mPhysWorld->QueryAABB(&queryCallBack, aabb);

	if (sDebugOptions.mShowEntityQueries) {
		f32 height = mWorldGrid.tryComputeHeightAtPoint(pos);
		DebugRenderer::drawAABB(aabb, height, color4(0.0f, 1.0f, 0.0f), 100);
	}

	if (sorted) {
		std::sort(entities.begin(), entities.end(), [](const EntityDistSortKey& a, const EntityDistSortKey& b) {
			return a.first.dist < b.first.dist;
		});
	}

	return entities;
}

inline void testExtremePoint(const f32v2& point, b2AABB& aabb) {
	if (point.x < aabb.lowerBound.x) {
		aabb.lowerBound.x = point.x;
	}
	else if (point.x > aabb.upperBound.x) {
		aabb.upperBound.x = point.x;
	}
	if (point.y < aabb.lowerBound.y) {
		aabb.lowerBound.y = point.y;
	}
	else if (point.y > aabb.upperBound.y) {
		aabb.upperBound.y = point.y;
	}
}

std::vector<EntityDistSortKey> World::queryActorsInArc(const f32v2& pos, float radius, const f32v2& normal, float arcAngle, ActorTypesMask includeMask, ActorTypesMask excludeMask, bool sorted, int quadrants, entt::entity except /*= (entt::entity)0*/) {
	const float halfAngle = arcAngle * 0.5f;

	// Empty mask = all types
	if (includeMask == 0) {
		includeMask = ~0;
	}
	
	// TODO: Implementation is wrong for >= 180 degrees angles
	// To fix we would need to start in our aim quadrant and increment/decrement nearby quadrants to see if they lie within
	assert(arcAngle <= M_PI);

	// TODO: Components as well? Better lookup?
	std::vector<EntityDistSortKey> entities;

	CONST f32v2 scaledNormal = normal * radius;
	
	// Center
	b2AABB aabb;
	aabb.lowerBound = TO_BVEC2_C(pos);
	aabb.upperBound = TO_BVEC2_C(pos);

	// Left ray
	f32v2 offset = glm::rotate(scaledNormal, -halfAngle);
	f32v2 point1 = pos + offset;
	testExtremePoint(point1, aabb);

	float centerAngle = atan2(normal.y, normal.x);

	float startAngle = centerAngle - halfAngle;
	float endAngle = centerAngle + halfAngle;

	// Right ray
	offset = glm::rotate(scaledNormal, +halfAngle);
	f32v2 point2 = pos + offset;
	testExtremePoint(point2, aabb);

	static const f32v2 axisExtrema[5] = { {-1.0f, 0.0f}, {0.0f, -1.0f}, {1.0f, 0.0f}, {0.0f, 1.0f}, {-1.0f, 0.0f} };

	int i = 0;
	for (float angle = -M_PIf; i < 5; angle += M_PI_2f, ++i) {
		if (angle > startAngle && angle < endAngle) {
			testExtremePoint(pos + axisExtrema[i] * radius, aabb);
			if (sDebugOptions.mShowEntityQueries) {
				const f32 height = mWorldGrid.tryComputeHeightAtPoint(pos);
				const f32v3 pos3d(pos.x, pos.y, height);
				const f32v2 extrema = (pos + axisExtrema[i] * radius);
				DebugRenderer::drawLine(pos3d, f32v3(extrema.x, extrema.y, height) - pos3d, color4(0.0f, 1.0f, 0.0f), 1);
			}
		}
	}

	ArcQueryCallback queryCallBack(entities, pos, mEcs->mRegistry, includeMask, excludeMask, radius, except, normal, halfAngle, quadrants);
	mPhysWorld->QueryAABB(&queryCallBack, aabb);

	if (sorted) {
		std::sort(entities.begin(), entities.end(), [](const EntityDistSortKey& a, const EntityDistSortKey& b) {
			return a.first.dist < b.first.dist;
		});
	}

	if (sDebugOptions.mShowEntityQueries) {
		static const int lifetime = 1;

		const f32v2& bottomLeft = TO_VVEC2_C(aabb.lowerBound);
		const f32v2& topRight = TO_VVEC2_C(aabb.upperBound);
		const f32v2 topLeft = f32v2(bottomLeft.x, topRight.y);
		const f32v2 bottomRight = f32v2(topRight.x, bottomLeft.y);

        const f32 height = mWorldGrid.tryComputeHeightAtPoint(pos);
		const f32v3 pos3d(pos.x, pos.y, height);
		DebugRenderer::drawAABB(bottomLeft, bottomRight, topLeft, topRight, height, color4(1.0f, 0.0f, 1.0f), lifetime);
		DebugRenderer::drawLine(pos3d, f32v3(point1.x, point1.y, height) - pos3d, color4(0.0f, 0.0f, 1.0f), lifetime);
		DebugRenderer::drawLine(pos3d, f32v3(point2.x, point2.y, height) - pos3d, color4(0.0f, 0.0f, 1.0f), lifetime);
		DebugRenderer::drawLine(pos3d, f32v3(scaledNormal.x, scaledNormal.y, height), color4(0.0f, 0.0f, 1.0f), lifetime);
	}

	return entities;
}

entt::entity World::createEntity(const f32v2& pos, const nString& typeName) {
	return mEntityFactory->createEntity(pos, typeName);
}

b2Body* World::createPhysBody(const b2BodyDef* bodyDef) {
	return mPhysWorld->CreateBody(bodyDef);
}

void World::createCityAt(const ui32v2& worldPos) {
	std::unique_ptr<City> newCity = std::make_unique<City>(worldPos, *this);
	mCities->mNodes.emplace_back(std::move(newCity));
}

void World::setTileAt(const ui32v2& worldPos, Tile tile) {

    TileHandle handle = getTileHandleAtWorldPos(worldPos);
	assert(handle.isValid());
    if (handle.isValid()) {
        Chunk* chunk = handle.getMutableChunk();
        chunk->setTileAt(handle.index, tile);
    }
}

void World::setTileAt(ChunkID id, TileIndex tileIndex, Tile tile) {
	Chunk& chunk = mWorldGrid.getChunk(id);
	chunk.setTileAt(tileIndex, tile);
}

void World::setTileLayerAt(const ui32v2& worldPos, TileID id, TileLayer layer) {
    TileHandle handle = getTileHandleAtWorldPos(worldPos);
	setTileLayerAt(handle, id, layer);
}

void World::setTileLayerAt(TileHandle& handle, TileID id, TileLayer layer) {
    assert(handle.isValid());
    if (handle.isValid()) {
        Chunk* chunk = handle.getMutableChunk();
        chunk->setTileLayer(handle.index, layer, id);
    }
}

void World::setTileFlagAt(const ui32v2& worldPos, TileFlags flag) {
    TileHandle handle = getTileHandleAtWorldPos(worldPos);
    Chunk* chunk = handle.getMutableChunk();
    chunk->setTileFlag(handle.index, flag);
}

void World::addTile(const ui32v2& worldPos, const TileData& tileData) {
    TileHandle handle = getTileHandleAtWorldPos(worldPos);
    assert(handle.isValid());
    if (handle.isValid()) {
        Chunk* chunk = handle.getMutableChunk();
        chunk->addTile(handle.index, tileData);
    }
}

bool World::tileHasHarvestableResource(const ui32v2& worldPos, TileResource resource, TileLayer* outLayer) {
	TileHandle handle = getTileHandleAtWorldPos(worldPos);
	if (handle.isValid()) {
		return handle.tile->hasHarvestableResource(resource, outLayer);
	}
	return false;
}
