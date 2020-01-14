#pragma once
#include <Vorb/graphics/Texture.h>
#include <Vorb/ecs/Entity.h>
#include <functional>
#include <optional>

#include <box2d/b2_world.h>

#include "actor/ActorTypes.h"
#include "TileSet.h"

DECL_VG(class SpriteBatch);
DECL_VG(class TextureCache)

class Camera2D;
class EntityComponentSystem;

typedef std::pair<float, vecs::EntityID> EntityDistSortKey;

class TileGrid
{
public:
	TileGrid(b2World& physWorld, const i32v2& dims, vg::TextureCache& textureCache, EntityComponentSystem& ecs, const std::string& tileSetFile, const i32v2& tileSetDims, float isoDegrees);
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

	std::vector<EntityDistSortKey> queryActorsInRadius(const f32v2& pos, float radius, ActorTypesMask mask, bool sorted, vecs::EntityID except = ENTITY_ID_NONE);
	std::vector<EntityDistSortKey> queryActorsInArc(const f32v2& pos, float radius, const f32v2& normal, float arcAngle, bool sorted, vecs::EntityID except = ENTITY_ID_NONE);

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
	const EntityComponentSystem& mEcs;
	b2World& mPhysWorld;
};

