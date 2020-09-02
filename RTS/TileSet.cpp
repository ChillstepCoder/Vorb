#include "stdafx.h"
#include "TileSet.h"

#include <Vorb/graphics/SpriteBatch.h>
#include <Vorb/graphics/Texture.h>
#include <Vorb/graphics/TextureCache.h>

const float TILE_SCALE = 1.0f;

TileSet::TileSet(vg::TextureCache& textureCache, const std::string& filePath, i32v2 dims)
{
	m_texture = textureCache.addTexture(filePath);
	m_dims = dims;
	m_uvMult.x = 1.0f / m_dims.x;
	m_uvMult.y = 1.0f / m_dims.y;

	m_tileDims.x = TILE_SCALE;
	m_tileDims.y = TILE_SCALE;
}

void TileSet::renderTile(vg::SpriteBatch& spriteBatch, i32 index, const f32v2& pos)
{
	f32v4 uvRect;
	uvRect.x = (index % m_dims.x) * m_uvMult.x;
	uvRect.y = ((index / m_dims.x) + 1) * m_uvMult.y;
	uvRect.z = m_uvMult.x;
	uvRect.w = -m_uvMult.y;
	spriteBatch.draw(m_texture.id, &uvRect, pos, m_tileDims, color4(1.0f, 1.0f, 1.0f));
}