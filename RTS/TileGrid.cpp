#include "stdafx.h"
#include "TileGrid.h"
#include "Camera2D.h"
#include <Vorb/graphics/SpriteBatch.h>
#include <Vorb/graphics/TextureCache.h>
#include <Vorb/math/VectorMath.hpp>
#include <glm/gtx/rotate_vector.hpp>

#include "Utils.h"

TileGrid::TileGrid(const i32v2& dims, vg::TextureCache& textureCache, const std::string& tileSetFile, const i32v2& tileSetDims, float isoDegrees)
	: m_dims(dims)
	, m_tiles(dims.x * dims.y, TileGrid::GRASS_0) {
	m_texture = textureCache.addTexture("data/textures/tiles.png");
	m_tileSet.init(&m_texture, tileSetDims);

	m_tiles[0] = STONE_2;
	m_tiles[1] = STONE_2;

	m_sb = std::make_unique<vg::SpriteBatch>();
	m_sb->init();

	// We rotate 45 degrees and then scale X and Y
	
	f32m4 mat4 = glm::rotate(f32m4(1.0f), glm::radians(45.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	const float scaleDiff = 0.3333f;
	mat4 = glm::scale(mat4, f32v3(1.0f + scaleDiff, 1.0f - scaleDiff, 1.0f));
	m_isoTransform = mat4;
	m_invIsoTransform = glm::inverse(mat4);

	m_axis[0] = glm::normalize(f32v2(1.0f, 0.0f) * m_isoTransform);
	m_axis[1] = glm::normalize(f32v2(0.0f, -1.0f) * m_isoTransform);

	m_realDims = f32v2(m_dims.x * m_tileSet.getTileDims().x, m_dims.y * m_tileSet.getTileDims().y);
}

TileGrid::~TileGrid() {
	m_sb->dispose();
}

void TileGrid::draw(const Camera2D& camera) {

	if  (m_dirty) {
		const f32v2 tileDims = m_tileSet.getTileDims() * 0.5f;
		const f32v2 offset(-tileDims.x, -tileDims.y);

		m_sb->begin();
		for (int y = 0; y < m_dims.y; ++y) {
			for (int x = 0; x < m_dims.x; ++x) {
				f32v2 pos(x * tileDims.x, -y * tileDims.y);
				m_tileSet.renderTile(*m_sb, (int)m_tiles[y * m_dims.x + x], pos * m_isoTransform + offset);
			}
		}
		m_sb->end();
		m_dirty = false;
	}

	m_sb->render(f32m4(1.0f), camera.getCameraMatrix());
}

int TileGrid::getTileIndexFromScreenPos(const f32v2& screenPos, const Camera2D& camera) {
	float cameraScale = camera.getScale();

	f32v2 worldPos = screenPos * m_invIsoTransform;

	std::cout << screenPos.x << " " << screenPos.y << " " << worldPos.x << " " << worldPos.y << std::endl;

	const int xIndex = worldPos.x / (m_tileSet.getTileDims().x * 0.5f);
	const int yIndex = -worldPos.y / (m_tileSet.getTileDims().y * 0.5f);

	if (xIndex < 0 || yIndex < 0) {
		return -1;
	}

	if (xIndex > m_dims.x || yIndex > m_dims.y) {
		return -1;
	}

	return yIndex * m_dims.x + xIndex;
}

void TileGrid::setTile(int index, Tile tile) {
	if (index < 0 || index >= m_tiles.size()) {
		return;
	}

	m_tiles[index] = tile;
	m_dirty = true;
}
