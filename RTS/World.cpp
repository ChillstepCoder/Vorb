#include "stdafx.h"
#include "World.h"

#include "Camera2D.h"
#include "EntityComponentSystem.h"
#include "DebugRenderer.h"
#include "rendering/ChunkRenderer.h"
#include "world/ChunkGenerator.h"
#include "physics/ContactListener.h"

#include <Vorb/graphics/SpriteBatch.h>
#include <Vorb/graphics/TextureCache.h>
#include <Vorb/math/VectorMath.hpp>
#include <glm/gtx/rotate_vector.hpp>

#include <box2d/b2_world.h>
#include <box2d/b2_fixture.h>

#include "physics/PhysQueryCallback.h"
#include "Utils.h"


#define ENABLE_DEBUG_RENDER 1
#if ENABLE_DEBUG_RENDER == 1
#include <Vorb/ui/InputDispatcher.h>
static bool s_debugToggle = false;
static bool s_wasTogglePressed = false;
#endif

const float CHUNK_UNLOAD_TOLERANCE = -10.0f; // How many extra blocks we add when checking unload distance


World::World(vg::TextureCache& textureCache) {
	mChunkRenderer = std::make_unique<ChunkRenderer>(textureCache);
	mChunkGenerator = std::make_unique<ChunkGenerator>();
}

World::~World() {

}

void World::init(EntityComponentSystem& ecs) {
	assert(!mEcs);
	mEcs = &ecs;
	mPhysWorld = std::make_unique<b2World>(b2Vec2(0.0f, 0.0f));

	mContactListener = std::make_unique<ContactListener>(*mEcs);
	mPhysWorld->SetContactListener(mContactListener.get());

	initTileData();
}

void World::draw(const Camera2D& camera) {

#if ENABLE_DEBUG_RENDER == 1
	if (vui::InputDispatcher::key.isKeyPressed(VKEY_R)) {
		if (!s_wasTogglePressed) {
			s_wasTogglePressed = true;
			s_debugToggle = !s_debugToggle;
		}
	}
	else {
		s_wasTogglePressed = false;
	}
#endif

	for (auto&& chunk : mChunks) {
		mChunkRenderer->RenderChunk(*chunk.second.get(), camera);
	}

	// TODO: This feels wrong
	updateWorldMousePos(camera);
}

void World::update(float deltaTime, const f32v2& playerPos, const Camera2D& camera) {
	assert(mEcs);

	mLoadCenter = playerPos;

	getChunkOrCreateAtPosition(playerPos);
	updateWorldMousePos(camera);
	
	mLoadRange.x = std::max(camera.getScreenWidth() * (1.0f / camera.getScale()) * 0.5f, (float)CHUNK_WIDTH);
	mLoadRange.y = std::max(camera.getScreenHeight() * (1.0f / camera.getScale()) * 0.5f, (float)CHUNK_WIDTH);

	// Corners
	for (auto it = mChunks.begin(); it != mChunks.end(); /* no iASSSSncrement */) {
		Chunk& chunk = *it->second;
		if (updateChunk(chunk)) {
			chunk.dispose();
			mChunks.erase(it++);
		}
		else {
			++it;
		}
	}

	// Update physics
	mPhysWorld->Step(deltaTime, 1, 1);
}

void World::updateWorldMousePos(const Camera2D& camera) {
	const i32v2& mousePos = vui::InputDispatcher::mouse.getPosition();
	mWorldMousePos = camera.convertScreenToWorld(f32v2(mousePos.x, mousePos.y));
}

Chunk* World::getChunkAtPosition(const f32v2& worldPos) {
	ChunkID chunkId(worldPos);
	std::map<ChunkID, std::unique_ptr<Chunk>>::iterator it = mChunks.find(chunkId);
	if (it != mChunks.end()) {
		return it->second.get();
	}

	return nullptr;
}

Chunk* World::getChunkOrCreateAtPosition(const f32v2& worldPos) {
	ChunkID chunkId(worldPos);
	return getChunkOrCreateAtPosition(chunkId);
}

Chunk* World::getChunkOrCreateAtPosition(ChunkID chunkId) {
	auto it = mChunks.find(chunkId);
	if (it != mChunks.end()) {
		return it->second.get();
	}

	// Create
	auto newChunkIt = mChunks.insert(std::make_pair(chunkId, std::make_unique<Chunk>()));
	Chunk* newChunk = newChunkIt.first->second.get();
	initChunk(*newChunk, chunkId);
	mChunkGenerator->GenerateChunk(*newChunk);
	return newChunk;
}

TileHandle World::getTileHandleAtScreenPos(const f32v2& screenPos, const Camera2D& camera) {
	assert(false); // Implement!
	return TileHandle();
}

TileHandle World::getTileHandleAtWorldPos(const f32v2& worldPos) {
	TileHandle handle;
	handle.chunk = getChunkAtPosition(worldPos);
	if (handle.chunk) {
		unsigned x = (unsigned)floor(worldPos.x) & (CHUNK_WIDTH - 1); // Fast modulus
		unsigned y = (unsigned)floor(worldPos.y) & (CHUNK_WIDTH - 1); // Fast modulus
		handle.index = ui16(y * CHUNK_WIDTH + x);
		handle.tile = handle.chunk->getTileAt(handle.index);
	}
	return handle;
}

bool World::updateChunk(Chunk& chunk) {
	if (!shouldChunkLoad(chunk.getWorldPos(), CHUNK_UNLOAD_TOLERANCE)) {
		// Unload
		return true;
	}
	
	updateChunkNeighbors(chunk);
	
	return false;
}

void World::updateChunkNeighbors(Chunk& chunk) {
	// Neighbors
	ChunkID myId = chunk.getChunkID();
	// Left
	if (!chunk.mNeighborLeft) {
		ChunkID leftChunk = myId.getLeftID();
		if (shouldChunkLoad(leftChunk)) {
			chunk.mNeighborLeft = getChunkOrCreateAtPosition(leftChunk);
		}
	}
	if (!chunk.mNeighborTop) {
		ChunkID topChunk = myId.getTopID();
		if (shouldChunkLoad(topChunk)) {
			chunk.mNeighborTop = getChunkOrCreateAtPosition(topChunk);
		}
	}
	if (!chunk.mNeighborRight) {
		ChunkID rightChunk = myId.getRightID();
		if (shouldChunkLoad(rightChunk)) {
			chunk.mNeighborRight = getChunkOrCreateAtPosition(rightChunk);
		}
	}
	if (!chunk.mNeighborBottom) {
		ChunkID bottomChunk = myId.getBottomID();
		if (shouldChunkLoad(bottomChunk)) {
			chunk.mNeighborBottom = getChunkOrCreateAtPosition(bottomChunk);
		}
	}
}

bool World::shouldChunkLoad(ChunkID chunkPos, float addOffset /* = 0.0f*/)
{
	const f32v2 centerPos = chunkPos.getWorldPos() + f32v2(HALF_CHUNK_WIDTH);
	const f32v2 offset = centerPos - mLoadCenter;

	// TODO: only need to check this at the edges
	return (abs(offset.x) + addOffset <= mLoadRange.x && abs(offset.y) + addOffset <= mLoadRange.y);
}

void World::initChunk(Chunk& chunk, ChunkID chunkId) {
	chunk.init(chunkId);
}

std::vector<EntityDistSortKey> World::queryActorsInRadius(const f32v2& pos, float radius, ActorTypesMask includeMask, ActorTypesMask excludeMask, bool sorted, vecs::EntityID except /*= ENTITY_ID_NONE*/) {
	// TODO: No allocation?

	// Empty mask = all types
	if (includeMask == 0) {
		includeMask = ~0;
	}

	// TODO: Components as well? Better lookup?
	std::vector<EntityDistSortKey> entities;

	PhysQueryCallback queryCallBack(entities, pos, mEcs->getPhysicsComponents(), includeMask, excludeMask, radius, except);
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

std::vector<EntityDistSortKey> World::queryActorsInArc(const f32v2& pos, float radius, const f32v2& normal, float arcAngle, ActorTypesMask includeMask, ActorTypesMask excludeMask, bool sorted, int quadrants, vecs::EntityID except /*= ENTITY_ID_NONE*/) {
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
	for (float angle = -M_PI; i < 5; angle += M_PI_2, ++i) {
		if (angle > startAngle && angle < endAngle) {
			testExtremePoint(pos + axisExtrema[i] * radius, aabb);
#if ENABLE_DEBUG_RENDER == 1
			if (s_debugToggle) {
				DebugRenderer::drawLine(pos, (pos + axisExtrema[i] * radius) - pos, color4(0.0f, 1.0f, 0.0f), 1);
			}
#endif
		}
	}

	ArcQueryCallback queryCallBack(entities, pos, mEcs->getPhysicsComponents(), includeMask, excludeMask, radius, except, normal, halfAngle, quadrants);
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

b2Body* World::createPhysBody(const b2BodyDef* bodyDef) {
	return mPhysWorld->CreateBody(bodyDef);
}
