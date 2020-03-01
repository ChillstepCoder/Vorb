#include "stdafx.h"
#include "TileGrid.h"
#include "Camera2D.h"
#include "EntityComponentSystem.h"
#include "DebugRenderer.h"

#include <Vorb/graphics/SpriteBatch.h>
#include <Vorb/graphics/TextureCache.h>
#include <Vorb/math/VectorMath.hpp>
#include <glm/gtx/rotate_vector.hpp>

#include <box2d/b2_fixture.h>

#include "physics/PhysQueryCallback.h"
#include "Utils.h"

// TODO: Shared
const float TILE_SCALE = 0.5f;

#define ENABLE_DEBUG_RENDER 1
#if ENABLE_DEBUG_RENDER == 1
#include <Vorb/ui/InputDispatcher.h>
static bool s_debugToggle = false;
static bool s_wasTogglePressed = false;
#endif

TileGrid::TileGrid(b2World& physWorld, const i32v2& dims, vg::TextureCache& textureCache, EntityComponentSystem& ecs, const std::string& tileSetFile, const i32v2& tileSetDims, float isoDegrees)
	: mDims(dims)
	, mTiles(dims.x * dims.y, TileGrid::GRASS_0)
	, mEcs(ecs)
	, mPhysWorld(physWorld) {
	mTexture = textureCache.addTexture("data/textures/tiles.png");
	mTileSet.init(&mTexture, tileSetDims);

	//m_tiles[0] = STONE_2;
	//m_tiles[1] = STONE_2;

	mSb = std::make_unique<vg::SpriteBatch>();
	mSb->init();

	setView(View::ISO);

	mRealDims = f32v2(mDims.x * mTileSet.getTileDims().x, mDims.y * mTileSet.getTileDims().y);
}

TileGrid::~TileGrid() {
	mSb->dispose();
}

void TileGrid::draw(const Camera2D& camera) {

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


	if  (mDirty) {
		const f32v2 tileDims(TILE_SCALE);
		const f32v2 offset(-tileDims.x, -tileDims.y);

		mSb->begin();
		for (int y = 0; y < mDims.y; ++y) {
			for (int x = 0; x < mDims.x; ++x) {
				f32v2 pos(x * tileDims.x, -y * tileDims.y);
				mTileSet.renderTile(*mSb, (int)mTiles[y * mDims.x + x], pos * mIsoTransform + offset);
			}
		}
		mSb->end();
		mDirty = false;
	}

	mSb->render(f32m4(1.0f), camera.getCameraMatrix());
}

void TileGrid::updateWorldMousePos(const Camera2D& camera) {
	const i32v2& mousePos = vui::InputDispatcher::mouse.getPosition();
	const f32v2 screenCoord = camera.convertScreenToWorld(f32v2(mousePos.x, mousePos.y));
	mWorldMousePos = convertScreenCoordToWorld(screenCoord);
}

void TileGrid::setView(View view) {
	mView = view;
	switch (view) {
		case View::ISO: {
			// We rotate 45 degrees and then scale X and Y
			f32m4 mat4 = glm::rotate(f32m4(1.0f), glm::radians(45.0f), glm::vec3(0.0f, 0.0f, 1.0f));
			const float scaleDiff = 0.3333f;
			mat4 = glm::scale(mat4, f32v3(1.0f + scaleDiff, 1.0f - scaleDiff, 1.0f));
			mIsoTransform = mat4;
			break;
		}
		case View::TOP_DOWN: {
			mIsoTransform = f32m4(1.0f);
			break;
		}
	}
	mInvIsoTransform = glm::inverse(mIsoTransform);

	// Useful axis vectors
	mAxis[0] = glm::normalize(f32v2(1.0f, 0.0f) * mIsoTransform);
	mAxis[1] = glm::normalize(f32v2(0.0f, -1.0f) * mIsoTransform);

	// Update render
	mDirty = true;
}

f32v2 TileGrid::convertScreenCoordToWorld(const f32v2& screenPos) const {
	return screenPos * mInvIsoTransform;
}

f32v2 TileGrid::convertWorldCoordToScreen(const f32v2& worldPos) const {
	return worldPos * mIsoTransform;
}

int TileGrid::getTileIndexFromScreenPos(const f32v2& screenPos, const Camera2D& camera) {
	float cameraScale = camera.getScale();

	f32v2 worldPos = convertScreenCoordToWorld(screenPos);

	const int xIndex = worldPos.x / (mTileSet.getTileDims().x * 0.5f);
	const int yIndex = -worldPos.y / (mTileSet.getTileDims().y * 0.5f);

	if (xIndex < 0 || yIndex < 0) {
		return -1;
	}

	if (xIndex > mDims.x || yIndex > mDims.y) {
		return -1;
	}

	return yIndex * mDims.x + xIndex;
}

void TileGrid::setTile(int index, Tile tile) {
	if (index < 0 || index >= mTiles.size()) {
		return;
	}

	mTiles[index] = tile;
	mDirty = true;
}

std::vector<EntityDistSortKey> TileGrid::queryActorsInRadius(const f32v2& pos, float radius, ActorTypesMask includeMask, ActorTypesMask excludeMask, bool sorted, vecs::EntityID except /*= ENTITY_ID_NONE*/) {
	// TODO: No allocation?

	// Empty mask = all types
	if (includeMask == 0) {
		includeMask = ~0;
	}

	// TODO: Components as well? Better lookup?
	std::vector<EntityDistSortKey> entities;

	PhysQueryCallback queryCallBack(entities, pos, mEcs.getPhysicsComponents(), includeMask, excludeMask, radius, except);
	b2AABB aabb;
	aabb.lowerBound = b2Vec2(pos.x - radius, pos.y - radius);
	aabb.upperBound = b2Vec2(pos.x + radius, pos.y + radius);
	mPhysWorld.QueryAABB(&queryCallBack, aabb);

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

std::vector<EntityDistSortKey> TileGrid::queryActorsInArc(const f32v2& pos, float radius, const f32v2& normal, float arcAngle, ActorTypesMask includeMask, ActorTypesMask excludeMask, bool sorted, int quadrants, vecs::EntityID except /*= ENTITY_ID_NONE*/) {
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
				DebugRenderer::drawLine(convertWorldCoordToScreen(pos), convertWorldCoordToScreen(pos + axisExtrema[i] * radius) - convertWorldCoordToScreen(pos), color4(0.0f, 1.0f, 0.0f), 1);
			}
#endif
		}
	}

	ArcQueryCallback queryCallBack(entities, pos, mEcs.getPhysicsComponents(), includeMask, excludeMask, radius, except, normal, halfAngle, quadrants);
	mPhysWorld.QueryAABB(&queryCallBack, aabb);

	if (sorted) {
		std::sort(entities.begin(), entities.end(), [](const EntityDistSortKey& a, const EntityDistSortKey& b) {
			return a.first.dist < b.first.dist;
		});
	}

#if ENABLE_DEBUG_RENDER == 1
	if (s_debugToggle) {
		static const int lifetime = 1;
		const f32v2 screenPos = convertWorldCoordToScreen(pos);

		const f32v2& bottomLeft = TO_VVEC2_C(aabb.lowerBound);
		const f32v2& topRight = TO_VVEC2_C(aabb.upperBound);
		const f32v2 topLeft = f32v2(bottomLeft.x, topRight.y);
		const f32v2 bottomRight = f32v2(topRight.x, bottomLeft.y);

		DebugRenderer::drawAABB(convertWorldCoordToScreen(bottomLeft), convertWorldCoordToScreen(bottomRight), convertWorldCoordToScreen(topLeft), convertWorldCoordToScreen(topRight), color4(1.0f, 0.0f, 1.0f), lifetime);
		DebugRenderer::drawLine(screenPos, convertWorldCoordToScreen(point1) - screenPos, color4(0.0f, 0.0f, 1.0f), lifetime);
		DebugRenderer::drawLine(screenPos, convertWorldCoordToScreen(point2) - screenPos, color4(0.0f, 0.0f, 1.0f), lifetime);
		DebugRenderer::drawLine(screenPos, convertWorldCoordToScreen(scaledNormal), color4(0.0f, 0.0f, 1.0f), lifetime);
	}
#endif

	return entities;
}
