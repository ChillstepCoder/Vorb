#include "stdafx.h"
#include "World.h"

#include "ecs/EntityComponentSystem.h"
#include "DebugRenderer.h"
#include "world/ChunkGenerator.h"
#include "world/HeightmapTerrainQuadtree.h"
#include "resources/TileRepository.h"
#include "weather/CloudManager.h"
#include "item/ItemStockpileRegistry.h"
#include "structure/StructureManager.h"

#include "physics/PhysicsWorld.h"

#include "ecs/factory/EntityFactory.h"

#include <Vorb/ui/InputDispatcher.h>
#include <Vorb/graphics/SpriteBatch.h>
#include <Vorb/graphics/TextureCache.h>
#include <Vorb/math/VectorMath.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/transform.hpp>

#include "rendering/ChunkMesher.h"
#include "rendering/ChunkGrassQuadtree.h"

#include "physics/PhysHitResult.h"

#include "ui/UIContext.h"

#include "Utils.h"

#include "city/City.h"
#include "camera/Camera3D.h"

#include "generation/WorldGeneration.h"
#include "pathfinding/NavGraph.h"
#include "pathfinding/NavThread.h"

// TODO: remove?
#include "ResourceManager.h"
#include "particles/ParticleSystemManager.h"

#include "tile/TileUtil.h"

#include "options/DebugOptions.h"

const float CHUNK_UNLOAD_TOLERANCE = -10.0f; // How many extra blocks we add when checking unload distance


World::World() :
	mWorldGrid(*this)
{
    // Init generation
    mChunkGenerator = std::make_unique<ChunkGenerator>();

    // Init ECS
    mEcs = std::make_unique<EntityComponentSystem>(*this);

    // Init factories
	mEntityFactory = std::make_unique<EntityFactory>(*mEcs);

	// Cities
	mCities = std::make_unique<CityGraph>();

	// Structures
	mStructuremanager = std::make_unique<StructureManager>(*this);

	// Stockpiles
	mItemStockpileRegistry = std::make_unique<ItemStockpileRegistry>(*this);

	// Nav graph
	mNavGraph = std::make_unique<NavGraph>(*this);

    // Weather (Init post load because it contains rendering and requires render context to be initialized, TODO: Fix this)
    mCloudManager = std::make_unique<CloudManager>(*this);

	// Physics
	mPhysWorld = std::make_unique<PhysicsWorld>();

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
			chunk->updateMainThread();
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
			mWorldGrid.releaseHeightDataAt(chunk.getHeightmapPatchID());
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

	// Update particles (TODO: Ecs?)
	// TODO: eww why is a resource updating?
	Services::ResourceManager::ref().getParticleSystemManager().update(playerPos);

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

void World::frameUpdate(const Camera3D& camera, f32 elapsedSec) {
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

	// Physworld will handle internal interpolation and timestep itself
    mPhysWorld->stepSimulation(elapsedSec);
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
		return chunk->getTileHandleAt(chunk->getTileContainer().getTileIndexFromXYZOffset(x, y, 0));
	}
	return TileHandle();
}

TileHandle World::getTileHandleAtWorldPos(const ui32v2& worldPos) const {
    TileHandle handle;
    const Chunk* chunk = &getChunkAtPosition(worldPos);
    if (chunk->isDataReady()) {
        ui32 x = (ui32)worldPos.x & (CHUNK_WIDTH - 1); // Fast modulus
        ui32 y = (ui32)worldPos.y & (CHUNK_WIDTH - 1); // Fast modulus
        return chunk->getTileHandleAt(chunk->getTileContainer().getTileIndexFromXYZOffset(x, y, 0));
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
    return chunk->getTileContainer().getTileAt(x, y, 0);
}

const Tile* World::tryGetTileAtWorldPos(const f32v2& worldPos) const {
    const Chunk* chunk = &getChunkAtPosition(worldPos);
    if (chunk->isDataReady()) {
        ui32 x = (ui32)worldPos.x & (CHUNK_WIDTH - 1); // Fast modulus
        ui32 y = (ui32)worldPos.y & (CHUNK_WIDTH - 1); // Fast modulus
        return &chunk->getTileContainer().getTileAt(x, y, 0);
    }
    return nullptr;
}

const Tile* World::tryGetTileAtWorldPos(const ui32v2& worldPos) const {
	const Chunk* chunk = &getChunkAtPosition(worldPos);
	if (chunk->isDataReady()) {
		ui32 x = (ui32)worldPos.x & (CHUNK_WIDTH - 1); // Fast modulus
		ui32 y = (ui32)worldPos.y & (CHUNK_WIDTH - 1); // Fast modulus
		return &chunk->getTileContainer().getTileAt(x, y, 0);
	}
	return nullptr;
}

const Tile* World::tryGetTileAtWorldPos(const ui16v2& worldPos) const {
    const Chunk* chunk = &getChunkAtPosition(worldPos);
    if (chunk->isDataReady()) {
        ui32 x = (ui32)worldPos.x & (CHUNK_WIDTH - 1); // Fast modulus
        ui32 y = (ui32)worldPos.y & (CHUNK_WIDTH - 1); // Fast modulus
        return &chunk->getTileContainer().getTileAt(x, y, 0);
    }
    return nullptr;
}

StructureArrayPtr World::tryGetStructuresAtWorldPos(const ui32v2& worldPos) const {
    const Chunk* chunk = &getChunkAtPosition(worldPos);
    if (chunk->isDataReady()) {
        ui32 x = (ui32)worldPos.x & (CHUNK_WIDTH - 1); // Fast modulus
        ui32 y = (ui32)worldPos.y & (CHUNK_WIDTH - 1); // Fast modulus
		return chunk->getStructuresAt(chunk->getTileContainer().getTileIndexFromXYZOffset(x, y, 0));
    }
    return std::make_pair(nullptr, 0);
}

const TerrainNavNode* World::tryGetNavNodeAtWorldPos(const ui32v2& worldPos) const {
	const Chunk& chunk = getChunkAtPosition(worldPos); // TODO: Stop casting??
	if (!chunk.isDataReady()) return nullptr;
    ui32 x = (ui32)worldPos.x & (CHUNK_WIDTH - 1); // Fast modulus
    ui32 y = (ui32)worldPos.y & (CHUNK_WIDTH - 1); // Fast modulus
	const Tile& tile = chunk.getTileContainer().getTileAt(x, y, 0);
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
	for (worldPos.y = aabb.y; worldPos.y < aabb.y + aabb.depth;) {
        for (worldPos.x = aabb.x; worldPos.x < aabb.x + aabb.depth;) {
            TileHandle cornerHandle = getTileHandleAtWorldPos(worldPos);
            assert(cornerHandle.container);
            Chunk& chunk = mWorldGrid.getChunk(ChunkID::fromWorldUI32v2(cornerHandle.getWorldPos2D()));
			ui32v3 offset = cornerHandle.getContainerOffset();
			const ui32 distFromRightEdge = CHUNK_WIDTH - offset.x;
            const ui32 distFromTopEdge = CHUNK_WIDTH - offset.y;
            spanX = std::min(distFromRightEdge, aabb.width);
            spanY = std::min(distFromTopEdge, aabb.depth); // TODO: Prob clever way to move this up a loop
			for (ui32 dy = 0; dy < spanY; ++dy) {
				for (ui32 dx = 0; dx < spanX; ++dx) {
					func(chunk, chunk.getTileContainer().getTileIndexFromXYZOffset(offset.x + dx, offset.y + dy, 0));
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
		if (chunk.getTileContainer().getRefCount()) {
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
		else if (chunk.mTileContainer.isDirtyNav() && chunk.mIsNavmeshing.load(/*memory order relaxed?*/) == false) {
            // Update nav graph when all neighbors are loaded
			// TODO: Async?
			chunk.mTileContainer.setDirtyNav(false);
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
	else if (chunk.getTileContainer().getRefCount() == 0) {
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
    chunk.mTileContainer.setDirtyNav(true);
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
	const HeightmapPatchID& id = chunk.getHeightmapPatchID();

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
			const HeightmapPatchData* heightData = mWorldGrid.getHeightDataAt(chunk.getHeightmapPatchID());
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

entt::entity World::createEntity(const f32v3& pos, const nString& typeName) {
	return mEntityFactory->createEntity(*mPhysWorld, pos, typeName);
}


void World::createCityAt(const ui32v2& worldPos) {
	std::unique_ptr<City> newCity = std::make_unique<City>(worldPos, *this);
	mCities->mNodes.emplace_back(std::move(newCity));
}

bool World::tileHasHarvestableResource(const ui32v2& worldPos, TileResource resource, TileLayer* outLayer) {
	TileHandle handle = getTileHandleAtWorldPos(worldPos);
	if (handle.isValid()) {
		return handle.tile->hasHarvestableResource(resource, outLayer);
	}
	return false;
}
