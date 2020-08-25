#pragma once

DECL_VG(class SpriteBatch);
DECL_VG(class TextureCache);

#include <Vorb/graphics/Texture.h>

class TileSet
{
public:
	TileSet(vg::TextureCache& textureCache, const std::string& filePath, i32v2 dims);

	void renderTile(vg::SpriteBatch& spriteBatch, i32 index, const f32v2& pos);
	const f32v2& getTileDims() const { return m_tileDims; }

	const vg::Texture& getTexture() const { return m_texture; }

private:
	i32v2 m_dims;
	f32v2 m_uvMult;
	f32v2 m_tileDims;
	vg::Texture m_texture;
};

