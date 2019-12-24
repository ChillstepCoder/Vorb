#pragma once

#include "TileSet.h"

#include <Vorb/graphics/Texture.h>

DECL_VG(class SpriteBatch);
DECL_VG(class TextureCache)

class Camera2D;

class TileGrid
{
public:
	TileGrid(const i32v2& dims, vg::TextureCache& textureCache, const std::string& tileSetFile, const i32v2& tileSetDims, float isoDegrees);
	~TileGrid();

	enum Tile : ui8 {
		GRASS_0 = 0,
		STONE_1 = 60,
		STONE_2 = 70,
		INVALID = 255
	};

	void draw(const Camera2D& camera);

	int getTileIndexFromScreenPos(const f32v2& screenPos, const Camera2D& camera);
	void setTile(int index, Tile tile);

//private:
public:
	i32v2 m_dims;
	f32v2 m_realDims;
	f32v2 m_axis[2];
	std::vector<Tile> m_tiles;
	TileSet m_tileSet;
	vg::Texture m_texture;
	bool m_dirty = true;
	f32m2 m_isoTransform;
	f32m2 m_invIsoTransform;

	std::unique_ptr<vg::SpriteBatch> m_sb;
};

