#include "stdafx.h"
#include "World.h"

#include "ecs/EntityComponentSystem.h"
#include "DebugRenderer.h"
#include "world/ChunkGenerator.h"
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

#include <box2d/b2_world.h>
#include <box2d/b2_fixture.h>

#include "physics/PhysQueryCallback.h"
#include "Utils.h"

#include "city/City.h"
#include "camera/ICamera.h"

// TODO: remove?
#include "ResourceManager.h"
#include "particles/ParticleSystemManager.h"

#include "util/TileUtil.h"

#include "options/DebugOptions.h"

#define ENABLE_DEBUG_RENDER 1

const float CHUNK_UNLOAD_TOLERANCE = -10.0f; // How many extra blocks we add when checking unload distance

// Chunks to load
#ifdef DEBUG
constexpr float CHUNKS_LOAD_RANGE_MULT = 15.0f;
#else
constexpr float CHUNKS_LOAD_RANGE_MULT = 15.0f;
#endif

#ifdef USE_SMALL_CHUNK_WIDTH
const float CHUNK_LOAD_RANGE = CHUNK_WIDTH * CHUNKS_LOAD_RANGE_MULT * 2.0f;
#else
const float CHUNK_LOAD_RANGE = CHUNK_WIDTH * CHUNKS_LOAD_RANGE_MULT;
#endif

World::World(ResourceManager& resourceManager) :
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

	// Weather
	mCloudManager = std::make_unique<CloudManager>(*this);

	// Static load range for now
	mLoadRangeSq = SQ(CHUNK_LOAD_RANGE);
}

World::~World() {
	IS_SHUTTING_DOWN = true;
}

void World::initPostLoad() {
    // Init regions
    for (ui32 i = 0; i < mWorldGrid.numRegions(); ++i) {
        mChunkGenerator->GenerateRegionLODTextureAsync(mWorldGrid.getRegion(i));
    }
}

void World::update(const f32v2& playerPos, const ICamera& camera) {
	assert(mEcs);

	Services::Threadpool::ref().mainThreadUpdate();

	updateSun(camera);

	mLoadCenter = playerPos;
	
	// TODO: This now asserts out of bounds
	Chunk& playerChunk = getChunkAtPosition(playerPos);
	if (playerChunk.isInvalid()) {
		initChunk(playerChunk);
	}

	mVisibleChunks.clear();
    for (size_t i = 0; i < mActiveChunks.size();) {
        Chunk& chunk = *mActiveChunks[i];
        if (updateChunk(chunk)) {
            chunk.dispose();
			mActiveChunks[i] = mActiveChunks.back();
			mActiveChunks.pop_back();
			continue;
        }
        ++i;
		// Determine visibility
		if (!chunk.isInvalid()) {
			const f32v2& worldPos = chunk.getWorldPos();
			if (camera.sphereIsVisible(f32v3(worldPos.x + HALF_CHUNK_WIDTH, worldPos.y + HALF_CHUNK_WIDTH, 0.0f), CHUNK_DIAGONAL_RADIUS)) {
				mVisibleChunks.push_back(&chunk);
				chunk.mChunkRenderData.mIsVisible = true;
			}
			else {
                chunk.mChunkRenderData.mIsVisible = false;
			}
		}
    }

	// Update cities
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

	// Update ECS
    mEcs->update(mClientEcsData);
}

Chunk& World::getChunkAtPosition(const f32v2& worldPos) {
	return getChunkAtPosition(ChunkID(worldPos));
}

Chunk& World::getChunkAtPosition(ChunkID chunkId) {
	assert(chunkId.id < WorldData::WORLD_SIZE_CHUNKS);
	return mWorldGrid.getChunk(chunkId.id);
}

const Chunk& World::getChunkAtPosition(ChunkID chunkId) const {
	assert(chunkId.id < WorldData::WORLD_SIZE_CHUNKS);
    return mWorldGrid.getChunk(chunkId.id);
}

Chunk& World::getChunkAtPosition(const ui32v2& worldPos) {
    return getChunkAtPosition(ChunkID(worldPos));
}

inline f32 fastFloorf(f32 x) {
    return FastConversion<f32, f32>::floor(x);
}
inline f32 fastCeilf(f32 x) {
    return FastConversion<f32, f32>::ceiling(x);
}

TileHandle World::getTileFromCameraPickVector(const ICamera& camera, const f32v3& rayDir) const {
	constexpr bool ENABLE_DEBUG_PICK_RENDER = false;

	const f32v3 rayStart = camera.getPosition();
	PreciseTimer timer;

	// TODO: https://vercidium.com/blog/optimised-voxel-raymarching/

	constexpr f32 RAY_CHECK_LENGTH = 10000.0f;
	f32v3 rayEnd = rayStart + rayDir * RAY_CHECK_LENGTH;

	const ui32 duration = 300;
	bool didHit = false;
	std::vector<std::pair<IntersectionHit3D, Chunk*> > sortedHits;
	for (auto&& chunk : mVisibleChunks) {
        if (!chunk->isFinished()) {
            continue;
        }
        IntersectionHit3D hit = IntersectionUtil::LineAABBIntersection(chunk->getAABB(), rayStart, rayEnd);
        if (hit.didHit()) {
            sortedHits.push_back(std::make_pair(hit, chunk));
        }
	}
    if (sortedHits.empty()) {
		if (ENABLE_DEBUG_PICK_RENDER) {
			DebugRenderer::drawVector(rayStart, rayEnd - rayStart, color4(1.0f, 0.0f, 0.0f), duration);
		}
        return TileHandle();
	}

	// Sort for nearest
	std::sort(sortedHits.begin(), sortedHits.end(), [](const std::pair<IntersectionHit3D, Chunk*>& a, const std::pair<IntersectionHit3D, Chunk*>& b) -> bool {
		return a.first.closeTime < b.first.closeTime;
	});

	for (auto&& hitPair : sortedHits) {
		IntersectionHit3D& hit = hitPair.first;
		Chunk* chunk = hitPair.second;

		f32v3 currentPos = hit.position;
		i32v3 currentVoxelPos = i32v3(fastFloor(currentPos.x), fastFloor(currentPos.y), fastFloor(currentPos.z));
        f32v3 farIntersect = (rayEnd - rayStart) * hit.farTime + rayStart;
        const f32v3 offset = farIntersect - hit.position;
        const f32 maxDistance = glm::length(offset);
		if (ENABLE_DEBUG_PICK_RENDER) {
			DebugRenderer::drawWireQuad(f32v2(chunk->getAABB().x, chunk->getAABB().y), f32v2(chunk->getAABB().width, chunk->getAABB().depth), color4(1.0f, 0.0f, 0.0f), duration);
			DebugRenderer::drawVector(rayStart, hit.position - rayStart, color4(1.0f, 0.0f, 0.0f), duration);
			DebugRenderer::drawVector(hit.position, offset, color4(0.0f, 1.0f, 0.0f), duration);
			DebugRenderer::drawWireQuad(f32v2(currentVoxelPos.x, currentVoxelPos.y), f32v2(1.0f, 1.0f), color4(1.0f, 1.0f, 1.0f), duration);
		}
		float currDistance = 0.0f;

		while (currDistance < maxDistance) {

			TileIndex index = TileIndex(currentVoxelPos.x % CHUNK_WIDTH, currentVoxelPos.y % CHUNK_WIDTH);
			
            Tile tile = chunk->getTileAt(index);
            if ((int)tile.baseZPosition >= currentVoxelPos.z) {
				if (ENABLE_DEBUG_PICK_RENDER) {
					DebugRenderer::drawWireQuad(f32v3(currentVoxelPos), f32v2(1.0f, 1.0f), color4(1.0f, 1.0f, 0.0f), duration);
					DebugRenderer::drawWireQuad(f32v2(currentVoxelPos.x, currentVoxelPos.y), f32v2(1.0f, 1.0f), color4(1.0f, 1.0f, 0.0f), duration);
				}
				return TileHandle(chunk, index);
            }
			if (ENABLE_DEBUG_PICK_RENDER) {
				DebugRenderer::drawWireQuad(f32v3(currentVoxelPos), f32v2(1.0f, 1.0f), color4(1.0f, 0.0f, 0.0f), duration);
			}

			f32v3 next;
			f32v3 r;

			// X-Distance
			if (rayDir.x > 0) {
				if (currentPos.x == fastCeilf(currentPos.x)) next.x = currentPos.x + 1;
				else next.x = fastCeilf(currentPos.x);
				r.x = (next.x - currentPos.x) / rayDir.x;
			}
			else if (rayDir.x < 0) {
				if (currentPos.x == fastFloorf(currentPos.x)) next.x = currentPos.x - 1;
				else next.x = fastFloorf(currentPos.x);
				r.x = (next.x - currentPos.x) / rayDir.x;
			}
			else {
				r.x = FLT_MAX;
			}

			// Y-Distance
			if (rayDir.y > 0) {
				if (currentPos.y == fastCeilf(currentPos.y)) next.y = currentPos.y + 1;
				else next.y = fastCeilf(currentPos.y);
				r.y = (next.y - currentPos.y) / rayDir.y;
			}
			else if (rayDir.y < 0) {
				if (currentPos.y == fastFloorf(currentPos.y)) next.y = currentPos.y - 1;
				else next.y = fastFloorf(currentPos.y);
				r.y = (next.y - currentPos.y) / rayDir.y;
			}
			else {
				r.y = FLT_MAX;
			}

			// Z-Distance
			if (rayDir.z > 0) {
				if (currentPos.z == fastCeilf(currentPos.z)) next.z = currentPos.z + 1;
				else next.z = fastCeilf(currentPos.z);
				r.z = (next.z - currentPos.z) / rayDir.z;
			}
			else if (rayDir.z < 0) {
				if (currentPos.z == fastFloorf(currentPos.z)) next.z = currentPos.z - 1;
				else next.z = fastFloorf(currentPos.z);
				r.z = (next.z - currentPos.z) / rayDir.z;
			}
			else {
				r.z = FLT_MAX;
			}
			f32v3 prevPos = currentPos; // DEBUG
			// Get Minimum Movement To The Next Voxel
			f32 rat;
			if (r.x < r.y && r.x < r.z) {
				// Move In The X-Direction
				rat = r.x;
				currentPos += rayDir * rat;
				if (rayDir.x > 0) currentVoxelPos.x++;
				else if (rayDir.x < 0) currentVoxelPos.x--;
			}
			else if (r.y < r.z) {
				// Move In The Y-Direction
				rat = r.y;
				currentPos += rayDir * rat;
				if (rayDir.y > 0) currentVoxelPos.y++;
				else if (rayDir.y < 0) currentVoxelPos.y--;
			}
			else {
				// Move In The Z-Direction
				rat = r.z;
				currentPos += rayDir * rat;
				if (rayDir.z > 0) currentVoxelPos.z++;
				else if (rayDir.z < 0) currentVoxelPos.z--;
            }
			if (ENABLE_DEBUG_PICK_RENDER) {
				DebugRenderer::drawVector(currentPos, currentPos - prevPos, color4(0.0f, 0.0f, 1.0f), duration);
			}


			// Add The Distance The Ray Has Traversed
			currDistance += rat;
		}
	}
	std::cout << "Pick time " << timer.stop() << std::endl;
	return TileHandle();
}

TileHandle World::getTileHandleAtWorldPos(const f32v2& worldPos) const {
	TileHandle handle;
	const Chunk* chunk = &getChunkAtPosition(worldPos);
	if (chunk->getState() == ChunkState::FINISHED) {
		ui32 x = (ui32)worldPos.x & (CHUNK_WIDTH - 1); // Fast modulus
		ui32 y = (ui32)worldPos.y & (CHUNK_WIDTH - 1); // Fast modulus
		return chunk->getTileHandleAt(TileIndex(x, y));
	}
	return TileHandle();
}

TileHandle World::getTileHandleAtWorldPos(const ui32v2& worldPos) const {
	return getTileHandleAtWorldPos(f32v2(worldPos));
}

TileCollision World::getTileCollisionAtWorldPos(const f32v2& worldPos) const
{
    const Chunk* chunk = &getChunkAtPosition(worldPos);
    if (chunk->getState() == ChunkState::FINISHED) {
        ui32 x = (ui32)worldPos.x & (CHUNK_WIDTH - 1); // Fast modulus
        ui32 y = (ui32)worldPos.y & (CHUNK_WIDTH - 1); // Fast modulus
        return chunk->getTileCollisionAt(TileIndex(x, y));
    }
    return TileCollision();
}

TileCollision World::getTileCollisionAtWorldPos(const ui32v2& worldPos) const {
    return getTileCollisionAtWorldPos(f32v2(worldPos));
}

const NavNode* World::tryGetNavNodeAtWorldPos(const ui32v2& worldPos) const
{
	const Chunk& chunk = getChunkAtPosition(f32v2(worldPos)); // TODO: Stop casting??
	if (!chunk.isDataReady()) return nullptr;
    ui32 x = (ui32)worldPos.x & (CHUNK_WIDTH - 1); // Fast modulus
    ui32 y = (ui32)worldPos.y & (CHUNK_WIDTH - 1); // Fast modulus
	const TileCollision& collision = chunk.getTileCollisionAt(TileIndex(x, y));
	return mNavGraph->getNode({ chunk.getChunkID().id, collision.navNodeIndex });
}

void World::enumVisibleChunks(std::function<void(const Chunk& chunk)> func) const {
	for (auto&& chunk : mVisibleChunks) {
		func(*chunk);
	}
}

void World::enumVisibleRegions(const ICamera& camera, std::function<void(const Region& chunk)> func) const {
    for (ui32 i = 0; i < mWorldGrid.numRegions(); ++i) {
        const Region& region = mWorldGrid.getRegion(i);
        const f32v2& worldPos = region.getWorldPos();
        if (camera.sphereIsVisible(f32v3(worldPos.x + WorldData::REGION_WIDTH_TILES, worldPos.y + WorldData::REGION_WIDTH_TILES, 0.0f), WorldData::REGION_DIAGONAL_RADIUS)) {
            func(region);
        }
    }
}

void World::efficientEnumTileAABB(const ui32AABB2& aabb, std::function<void(Chunk&, Tile&)> func) {
	// TODO: implement locking (write/read)
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
					func(chunk, chunk.mTiles[TileIndex(x + dx, y + dy)]);
				}
			}
			worldPos.x += spanX;
		}
		worldPos.y += spanY;
	}
}

void World::updateClientEcsData(Cartesian worldLookCardinalDirection) {
	mClientEcsData.worldLookCardinalDirection = worldLookCardinalDirection;
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

		TileCollision collision = getTileCollisionAtWorldPos(currentCellPos);

		// Check collision
		// TODO: Pass in collision radius
		IntersectionHit2D hit = TileUtil::tryRayTileIntersect(collision, currentCellPos, start, end, zPos, 0.3f);
		if (hit.didHit()) {
			return hit;
		}

		// Add The Distance The Ray Has Traversed
		currDist += rat;
	}

	return IntersectionHit2D();
}

void World::updateSun(const ICamera& camera) {
    const float SUNRISE_TIME = 6.0f; // 6am
	const float SUN_HEIGHT_OFFSET = 0.3f; // Smaller exponent means brighter days
	// TODO: Better time manager
	const f64 adjustedTime = /*sTotalTimeSeconds + */sDebugOptions.mTimeOffset;
    mTimeOfDay = (float)fmod(adjustedTime / (f64)SECONDS_PER_HOUR, (f64)HOURS_PER_DAY);

	const f32 sunDelta = (mTimeOfDay - SUNRISE_TIME) / 24.0f;
	const f32 sunRotate = sunDelta * M_PIF * 2.0f;
    mSunPosition = glm::rotateY(f32v3(-1.0f, 0.1f, 0.0f), sunRotate);
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
		if (chunk.mRefCount) {
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
		else if (chunk.mDirtyNavGraph && chunk.mState != ChunkState::GENERATING_NAVGRAPH) {
            // Update nav graph when all neighbors are loaded
			// TODO: Async?
			mNavGraph->buildNavNodesForChunkSynchronous(chunk);
			chunk.mDirtyNavGraph = false;
            if (sDebugOptions.mNavGraph) {
                mNavGraph->debugDrawNavGraphForChunk(chunk, 250);
            }
		}
	}
	
	return false;
}

void World::onChunkDataReady(Chunk& chunk) {

	// Don't update neighbors until we are data ready
	assert(chunk.isDataReady());
	// Neighbors
	const ChunkID& myId = chunk.getChunkID();
    dataReadyTryNotifyNeighbor(chunk, myId.getLeftID());
    dataReadyTryNotifyNeighbor(chunk, myId.getTopID());
    dataReadyTryNotifyNeighbor(chunk, myId.getRightID());
    dataReadyTryNotifyNeighbor(chunk, myId.getBottomID());
	
	assert(chunk.mDataReadyNeighborCount <= CHUNK_NEIGHBOR_COUNT);
}

void World::onChunkAllNeighborsDataReady(Chunk& chunk) {
	chunk.incRef();
	if (chunk.mState == ChunkState::GENERATING_NAVGRAPH) {
        Services::Threadpool::ref().addTask([&](ThreadPoolWorkerData* workerData) {
            // Update nav graph on worker thread
            mNavGraph->buildNavNodesForChunkSynchronous(chunk);
        }, [&]() {

            if (sDebugOptions.mNavGraph) {
				mNavGraph->debugDrawNavGraphForChunk(chunk, 250);
            }

            chunk.mState = ChunkState::FINISHED;
            chunk.decRef();
        });
	}
}

void World::dataReadyTryNotifyNeighbor(Chunk& chunk, const ChunkID& id) {
	Chunk& neighbor = mWorldGrid.getChunk(id.id);
    if (neighbor.isDataReady()) {
		// Set up data ready ref counts
		++neighbor.mDataReadyNeighborCount;
		++chunk.mDataReadyNeighborCount;
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

	return glm::length2(offset) <= mLoadRangeSq + addOffset;
}

void World::initChunk(Chunk& chunk)
{
	const ChunkID& chunkId = chunk.getChunkID();
    // If this is a sentinel chunk, stop here
    if (chunkId.isSentinelID()) {
        return;
    }

    generateChunkAsync(chunk);
    mActiveChunks.push_back(&chunk);
}

void World::generateChunkAsync(Chunk& chunk) {
	chunk.mState = ChunkState::LOADING_TILES;
	chunk.incRef();
	Services::Threadpool::ref().addTask([&](ThreadPoolWorkerData* workerData) {
        mChunkGenerator->GenerateChunk(chunk);
    }, [&]() {
        chunk.mState = ChunkState::GENERATING_NAVGRAPH;
		onChunkDataReady(chunk);
		chunk.decRef();
    });
}

void World::editorInvalidateWorldGen() {
	initPostLoad();
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

#if ENABLE_DEBUG_RENDER == 1
        DebugRenderer::drawAABB(aabb, color4(0.0f, 1.0f, 0.0f), 100);
#endif

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
#if ENABLE_DEBUG_RENDER == 1
			DebugRenderer::drawLine(pos, (pos + axisExtrema[i] * radius) - pos, color4(0.0f, 1.0f, 0.0f), 1);
#endif
		}
	}

	ArcQueryCallback queryCallBack(entities, pos, mEcs->mRegistry, includeMask, excludeMask, radius, except, normal, halfAngle, quadrants);
	mPhysWorld->QueryAABB(&queryCallBack, aabb);

	if (sorted) {
		std::sort(entities.begin(), entities.end(), [](const EntityDistSortKey& a, const EntityDistSortKey& b) {
			return a.first.dist < b.first.dist;
		});
	}

#if ENABLE_DEBUG_RENDER == 1
	static const int lifetime = 1;

	const f32v2& bottomLeft = TO_VVEC2_C(aabb.lowerBound);
	const f32v2& topRight = TO_VVEC2_C(aabb.upperBound);
	const f32v2 topLeft = f32v2(bottomLeft.x, topRight.y);
	const f32v2 bottomRight = f32v2(topRight.x, bottomLeft.y);

	DebugRenderer::drawAABB(bottomLeft, bottomRight, topLeft, topRight, color4(1.0f, 0.0f, 1.0f), lifetime);
	DebugRenderer::drawLine(pos, point1 - pos, color4(0.0f, 0.0f, 1.0f), lifetime);
	DebugRenderer::drawLine(pos, point2 - pos, color4(0.0f, 0.0f, 1.0f), lifetime);
	DebugRenderer::drawLine(pos, scaledNormal, color4(0.0f, 0.0f, 1.0f), lifetime);
#endif

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

void World::setTileLayerAt(const ui32v2& worldPos, TileID id, TileLayer layer) {
    TileHandle handle = getTileHandleAtWorldPos(worldPos);
	setTileLayerAt(handle, id, layer);
}

void World::setTileLayerAt(TileHandle& handle, TileID id, TileLayer layer) {
    assert(handle.isValid());
    if (handle.isValid()) {
        Chunk* chunk = handle.getMutableChunk();
        chunk->setTileAt(handle.index, id, layer);
    }
}

void World::setTileFlagAt(const ui32v2& worldPos, TileFlags flag) {
    TileHandle handle = getTileHandleAtWorldPos(worldPos);
    Chunk* chunk = handle.getMutableChunk();
    chunk->setTileFlagAt(handle.index, flag);
}

void World::setTileCollisionNavFlagAt(const ui32v2& worldPos, TileCollisionNavFlags flag) {
    TileHandle handle = getTileHandleAtWorldPos(worldPos);
    Chunk* chunk = handle.getMutableChunk();
    chunk->setTileCollisionNavFlagAt(handle.index, flag);
}

bool World::tileHasHarvestableResource(const ui32v2& worldPos, TileResource resource, TileLayer* outLayer) {
	TileHandle handle = getTileHandleAtWorldPos(worldPos);
	if (handle.isValid()) {
		for (int i = 0; i < TILE_LAYER_COUNT; ++i) {
			TileID tileId = handle.tile.layers[i];
			if (tileId != INVALID_TILE_INDEX) {
				if (TileRepository::getTileData(tileId).resource == resource) {
					if (outLayer) {
						*outLayer = (TileLayer)i;
					}
					return true;
				}
			}
		}
	}
	return false;
}
