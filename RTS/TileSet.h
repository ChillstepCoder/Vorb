#pragma once

DECL_VG(class SpriteBatch);
DECL_VG(class Texture);

class TileSet
{
public:
	TileSet();

	void init(vg::Texture* texture, i32v2 dims);

	void renderTile(vg::SpriteBatch& spriteBatch, i32 index, const f32v2& pos);
	const f32v2& getTileDims() const { return m_tileDims; }

private:
	i32v2 m_dims;
	f32v2 m_uvMult;
	f32v2 m_tileDims;
	vg::Texture* m_texture = nullptr;
};

