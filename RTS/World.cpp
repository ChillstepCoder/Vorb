#include "stdafx.h"
#include "World.h"

#include "Camera2D.h"
#include "EntityComponentSystem.h"
#include "DebugRenderer.h"
#include "rendering/ChunkRenderer.h"
#include "world/ChunkGenerator.h"
#include "physics/ContactListener.h"

#include "actor/HumanActorFactory.h"
#include "actor/UndeadActorFactory.h"
#include "actor/PlayerActorFactory.h"
#include "ecs/factory/EntityFactory.h"

#include <Vorb/ui/InputDispatcher.h>
#include <Vorb/graphics/SpriteBatch.h>
#include <Vorb/graphics/TextureCache.h>
#include <Vorb/math/VectorMath.hpp>
#include <glm/gtx/rotate_vector.hpp>

#include <box2d/b2_world.h>
#include <box2d/b2_fixture.h>

#include "physics/PhysQueryCallback.h"
#include "Utils.h"

// TODO: remove?
#include "ResourceManager.h"
#include "particles/ParticleSystemManager.h"


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

	// Static load range for now
	mLoadRangeSq = SQ(CHUNK_LOAD_RANGE);
}

World::~World() {

}

void World::initPostLoad() {
    // Init regions
    for (ui32 i = 0; i < mWorldGrid.numRegions(); ++i) {
        mChunkGenerator->GenerateRegionLODTextureAsync(mWorldGrid.getRegion(i));
    }
}

void World::update(float deltaTime, const f32v2& playerPos, const Camera2D& camera) {
	assert(mEcs);

	Services::Threadpool::ref().mainThreadUpdate();

	updateSun();

	mLoadCenter = playerPos;
	
	// TODO: This now asserts out of bounds
	Chunk& playerChunk = getChunkAtPosition(playerPos);
	if (playerChunk.isInvalid()) {
		initChunk(playerChunk);
	}
	
	const f32v2 topRight = camera.convertScreenToWorld(f32v2(camera.getScreenWidth(), 0.0f));
	const f32v2 center = camera.convertScreenToWorld(f32v2(camera.getScreenWidth() * 0.5f, camera.getScreenHeight() * 0.5f));

    mViewRange.x = topRight.x - center.x + CHUNK_WIDTH / 2;
	mViewRange.y = topRight.y - center.y + CHUNK_WIDTH / 2;

	// Always load an extra border of chunks
	mViewRange.x = MAX(mViewRange.x, CHUNK_WIDTH) + CHUNK_WIDTH;
	mViewRange.y = MAX(mViewRange.y, CHUNK_WIDTH) + CHUNK_WIDTH;

    for (size_t i = 0; i < mActiveChunks.size();) {
        Chunk& chunk = *mActiveChunks[i];
        if (updateChunk(chunk)) {
            chunk.dispose();
			mActiveChunks[i] = mActiveChunks.back();
			mActiveChunks.pop_back();
        }
        else {
            ++i;
        }
    }

	// Update physics
	mPhysWorld->Step(deltaTime, 1, 1);

	// Update particles (TODO: Ecs?)
	mResourceManager.getParticleSystemManager().update(deltaTime, playerPos);

	// Update ECS
    mEcs->update(deltaTime, mClientEcsData);
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

TileHandle World::getTileHandleAtScreenPos(const f32v2& screenPos, const Camera2D& camera) {
	assert(false); // Implement!
	return TileHandle();
}

TileHandle World::getTileHandleAtWorldPos(const f32v2& worldPos) {
	TileHandle handle;
	handle.chunk = &getChunkAtPosition(worldPos);
	if (handle.chunk->getState() == ChunkState::FINISHED) {
		unsigned x = (unsigned)floor(worldPos.x) & (CHUNK_WIDTH - 1); // Fast modulus
		unsigned y = (unsigned)floor(worldPos.y) & (CHUNK_WIDTH - 1); // Fast modulus
		handle.index = ui16(y * CHUNK_WIDTH + x);
		handle.tile = handle.chunk->getTileAt(handle.index);
		return handle;
	}
	return TileHandle();
}

void World::enumVisibleChunks(const Camera2D& camera, std::function<void(const Chunk& chunk)> func) const
{
    // Stick to positive numbers
    f32v2 bottomLeftCorner = glm::max(camera.convertScreenToWorld(f32v2(0.0f, camera.getScreenHeight())), 0.0f);
	// Start one chunk down for mountains
	bottomLeftCorner.y -= CHUNK_WIDTH;

    const ChunkID bottomLeftID(bottomLeftCorner);

	ChunkID enumerator = bottomLeftID;
    
    const float scaledWidth = camera.getScreenWidth() / camera.getScale();
    const float scaledHeight = camera.getScreenHeight() / camera.getScale();
    ui32 widthInChunks = (ui32)((scaledWidth + CHUNK_WIDTH / 2) / CHUNK_WIDTH + 1);
	ui32 heightInChunks = (ui32)((scaledHeight + CHUNK_WIDTH / 2) / CHUNK_WIDTH + 1) + 1;

	if (widthInChunks + enumerator.pos.x >= WorldData::WORLD_WIDTH_CHUNKS) {
		widthInChunks -= (widthInChunks + enumerator.pos.x) - WorldData::WORLD_WIDTH_CHUNKS + 1;
	}
    if (heightInChunks + enumerator.pos.y >= WorldData::WORLD_WIDTH_CHUNKS) {
		heightInChunks -= (heightInChunks + enumerator.pos.y) - WorldData::WORLD_WIDTH_CHUNKS + 1;
    }
	// Dont crash if we go out of world
	if (enumerator.pos.x >= WorldData::WORLD_WIDTH_CHUNKS || enumerator.pos.y >= WorldData::WORLD_WIDTH_CHUNKS) {
		return;
	}

	while (true) {
        if (enumerator.pos.x - bottomLeftID.pos.x > widthInChunks) {
            enumerator.pos.x -= widthInChunks + 1;
			enumerator = enumerator.getTopID();
        }
        if (enumerator.pos.y - bottomLeftID.pos.y > heightInChunks) {
            // Off screen
            return;
        }
		const Chunk& chunk = getChunkAtPosition(enumerator);
        if (!chunk.isInvalid()) {
            func(chunk);
        }
        enumerator = enumerator.getRightID();
	}
}

void World::enumVisibleRegions(const Camera2D& camera, std::function<void(const Region& chunk)> func) const {
    // Stick to positive numbers
    const f32v2 bottomLeftCorner = glm::max(camera.convertScreenToWorld(f32v2(0.0f, camera.getScreenHeight())), 0.0f);

    const RegionID bottomLeftID(bottomLeftCorner);

    RegionID enumerator = bottomLeftID;

    const float scaledWidth = camera.getScreenWidth() / camera.getScale();
    const float scaledHeight = camera.getScreenHeight() / camera.getScale();
    ui32 widthInRegions = (ui32)((scaledWidth + WorldData::REGION_WIDTH_TILES / 2) / WorldData::REGION_WIDTH_TILES + 1);
    ui32 heightInRegions = (ui32)((scaledHeight + WorldData::REGION_WIDTH_TILES / 2) / WorldData::REGION_WIDTH_TILES + 1);

    if (widthInRegions + enumerator.pos.x >= WorldData::WORLD_WIDTH_REGIONS) {
        widthInRegions -= (widthInRegions + enumerator.pos.x) - WorldData::WORLD_WIDTH_REGIONS + 1;
    }
    if (heightInRegions + enumerator.pos.y >= WorldData::WORLD_WIDTH_REGIONS) {
        heightInRegions -= (heightInRegions + enumerator.pos.y) - WorldData::WORLD_WIDTH_REGIONS + 1;
    }
    // Dont crash if we go out of world
    if (enumerator.pos.x >= WorldData::WORLD_WIDTH_REGIONS || enumerator.pos.y >= WorldData::WORLD_WIDTH_REGIONS) {
        return;
    }

    while (true) {
        if (enumerator.pos.x - bottomLeftID.pos.x > widthInRegions) {
            enumerator.pos.x -= widthInRegions + 1;
            enumerator = enumerator.getTopID();
        }
        if (enumerator.pos.y - bottomLeftID.pos.y > heightInRegions) {
            // Off screen
            return;
        }
		assert(enumerator.id < WorldData::WORLD_SIZE_REGIONS);
        const Region& region = mWorldGrid.getRegion(enumerator.id);
        func(region);
        enumerator = enumerator.getRightID();
    }
}

void World::updateClientEcsData(const Camera2D& camera) {
    const i32v2& mousePos = vui::InputDispatcher::mouse.getPosition();
    mClientEcsData.worldMousePos = camera.convertScreenToWorld(f32v2(mousePos.x, mousePos.y));
}

void World::setTimeOfDay(float time) {
	assert(time >= 0.0f && time <= HOURS_PER_DAY);

	// Get initial
    updateSun();

	// Offset debug time
	const f64 timeOffset = time - mTimeOfDay;
	sDebugOptions.mTimeOffset += timeOffset * SECONDS_PER_HOUR;

	// TODO: Better time manager
	// Update with new offset
	updateSun();
}

void World::updateSun() {
    const float SUNRISE_TIME = 6.0f; // 6am
    const float SUNSET_TIME = 19.0f; // 7pm
	const float SUN_HEIGHT_EXPONENT = 0.5f; // Smaller exponent means brighter days
	const float DAY_SPAN = SUNSET_TIME - SUNRISE_TIME;
	// TODO: Better time manager
	const f64 adjustedTime = sTotalTimeSeconds + sDebugOptions.mTimeOffset;
    mTimeOfDay = (float)fmod(adjustedTime / (f64)SECONDS_PER_HOUR, (f64)HOURS_PER_DAY);

	const float dayDelta = (mTimeOfDay - SUNRISE_TIME) / DAY_SPAN;
	mSunHeight = sin(dayDelta * M_PIF);
	if (mSunHeight > 0.0f) {
		// We can only do exponent curve on nonzero numbers
		mSunHeight = pow(mSunHeight, SUN_HEIGHT_EXPONENT);
	}
	mSunPosition = vmath::lerp(-1.0f, 1.0f, dayDelta);

	// Colors
    f32v3 sunSet(1.0f, 0.5f, 0);
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
			// Waiting on a thread to release us
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

void World::dataReadyTryNotifyNeighbor(Chunk& chunk, const ChunkID& id) {
	Chunk& neighbor = mWorldGrid.getChunk(id.id);
    if (neighbor.isDataReady()) {
		// Set up data ready ref counts
		++neighbor.mDataReadyNeighborCount;
		++chunk.mDataReadyNeighborCount;
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
	chunk.mState = ChunkState::LOADING;
	chunk.incRef();
	Services::Threadpool::ref().addTask([&](ThreadPoolWorkerData* workerData) {
        mChunkGenerator->GenerateChunk(chunk);
    }, [&]() {
        chunk.mState = ChunkState::FINISHED;
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
			if (s_debugToggle) {
				assert(false); // Can we do shared debug  toggle
				DebugRenderer::drawLine(pos, (pos + axisExtrema[i] * radius) - pos, color4(0.0f, 1.0f, 0.0f), 1);
			}
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
	if (s_debugToggle) {
		static const int lifetime = 1;

		const f32v2& bottomLeft = TO_VVEC2_C(aabb.lowerBound);
		const f32v2& topRight = TO_VVEC2_C(aabb.upperBound);
		const f32v2 topLeft = f32v2(bottomLeft.x, topRight.y);
		const f32v2 bottomRight = f32v2(topRight.x, bottomLeft.y);

		DebugRenderer::drawAABB(bottomLeft, bottomRight, topLeft, topRight, color4(1.0f, 0.0f, 1.0f), lifetime);
		DebugRenderer::drawLine(pos, point1 - pos, color4(0.0f, 0.0f, 1.0f), lifetime);
		DebugRenderer::drawLine(pos, point2 - pos, color4(0.0f, 0.0f, 1.0f), lifetime);
		DebugRenderer::drawLine(pos, scaledNormal, color4(0.0f, 0.0f, 1.0f), lifetime);
	}
#endif

	return entities;
}

entt::entity World::createEntity(const f32v2& pos, EntityType type) {
	return mEntityFactory->createEntity(pos, type);
}

b2Body* World::createPhysBody(const b2BodyDef* bodyDef) {
	return mPhysWorld->CreateBody(bodyDef);
}
