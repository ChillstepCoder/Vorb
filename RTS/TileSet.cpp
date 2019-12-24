#include "stdafx.h"
#include "TileSet.h"

#include <Vorb/graphics/SpriteBatch.h>
#include <Vorb/graphics/Texture.h>

TileSet::TileSet()
{
}

void TileSet::init(vg::Texture* texture, i32v2 dims)
{
	m_texture = texture;
	m_dims = dims;
	m_uvMult.x = 1.0f / m_dims.x;
	m_uvMult.y = 1.0f / m_dims.y;

	m_tileDims.x = texture->width / dims.x;
	m_tileDims.y = texture->height / dims.y;
}

void TileSet::renderTile(vg::SpriteBatch& spriteBatch, i32 index, const f32v2& pos)
{
	f32v4 uvRect;
	uvRect.x = (index % m_dims.x) * m_uvMult.x;
	uvRect.y = ((index / m_dims.x) + 1) * m_uvMult.y;
	uvRect.z = m_uvMult.x;
	uvRect.w = -m_uvMult.y;
	spriteBatch.draw(m_texture->id, &uvRect, pos, m_tileDims, color4(1.0f, 1.0f, 1.0f));
}
