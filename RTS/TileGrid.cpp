#include "stdafx.h"
#include "TileGrid.h"
#include "Camera2D.h"
#include <Vorb/graphics/SpriteBatch.h>
#include <Vorb/graphics/TextureCache.h>
#include <Vorb/math/VectorMath.hpp>
#include <glm/gtx/rotate_vector.hpp>

#include "Utils.h"


TileGrid::TileGrid(const i32v2& dims, vg::TextureCache& textureCache, const std::string& tileSetFile, const i32v2& tileSetDims, float isoDegrees)
	: mDims(dims)
	, mTiles(dims.x * dims.y, TileGrid::GRASS_0) {
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

	if  (mDirty) {
		const f32v2 tileDims = mTileSet.getTileDims() * 0.5f;
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

	std::cout << screenPos.x << " " << screenPos.y << " " << worldPos.x << " " << worldPos.y << std::endl;

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
