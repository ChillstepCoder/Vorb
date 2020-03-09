#pragma once
#include "Camera2D.h"

#include <Vorb/graphics/Texture.h>

DECL_VG(class SpriteBatch)
DECL_VG(class TextureCache);

class Camera2D;
class PhysicsComponentTable;
class EntityComponentSystem;
class TileGrid;

class EntityComponentSystemRenderer {
public:
	EntityComponentSystemRenderer(vg::TextureCache& textureCache, const EntityComponentSystem& system, const TileGrid& tileGrid);
	void renderPhysicsDebug(const Camera2D& camera) const;
	void renderSimpleSprites(const Camera2D& camera) const;
	void renderCharacterModels(const Camera2D& camera);

private:
	std::unique_ptr<vg::SpriteBatch> mSpriteBatch;
	vg::TextureCache& mTextureCache;
	vg::Texture mCircleTexture;
	const EntityComponentSystem& mSystem;
	const TileGrid& mTileGrid;
};

