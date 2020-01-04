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

	enum class View {
		ISO,
		TOP_DOWN
	};

	void draw(const Camera2D& camera);

	void setView(View view);
	View getView() const { return mView; }
	f32v2 convertScreenCoordToWorld(const f32v2& screenPos) const;
	f32v2 convertWorldCoordToScreen(const f32v2& worldPos) const;
	int getTileIndexFromScreenPos(const f32v2& screenPos, const Camera2D& camera);
	void setTile(int index, Tile tile);

//private:
public:
	View mView;
	i32v2 mDims;
	f32v2 mRealDims;
	f32v2 mAxis[2];
	std::vector<Tile> mTiles;
	TileSet mTileSet;
	bool mDirty = true;
	vg::Texture mTexture;
	f32m2 mIsoTransform;
	f32m2 mInvIsoTransform;

	std::unique_ptr<vg::SpriteBatch> mSb;
};

