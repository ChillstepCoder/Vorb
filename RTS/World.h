#pragma once
#include <Vorb/graphics/Texture.h>
#include <Vorb/ecs/Entity.h>
#include <functional>
#include <optional>

#include <box2d/b2_world.h>

#include "actor/ActorTypes.h"
#include "TileSet.h"
#include "world/Tile.h"

#include "world/Chunk.h"

DECL_VG(class SpriteBatch);
DECL_VG(class TextureCache)

class Camera2D;
class ChunkRenderer;
class ChunkGenerator;
class EntityComponentSystem;

class World
{
public:
	World(b2World& physWorld, const i32v2& dims, vg::TextureCache& textureCache, EntityComponentSystem& ecs);
	~World();

	void draw(const Camera2D& camera);
	void updateWorldMousePos(const Camera2D& camera);

	const f32v2& getCurrentWorldMousePos() const { return mWorldMousePos; }
	int getTileHandleAtScreenPos(const f32v2& screenPos, const Camera2D& camera);

	std::vector<EntityDistSortKey> queryActorsInRadius(const f32v2& pos, float radius, ActorTypesMask includeMask, ActorTypesMask excludeMask, bool sorted, vecs::EntityID except = ENTITY_ID_NONE);
	std::vector<EntityDistSortKey> queryActorsInArc(const f32v2& pos, float radius, const f32v2& normal, float arcAngle, ActorTypesMask includeMask, ActorTypesMask excludeMask, bool sorted, int quadrants, vecs::EntityID except = ENTITY_ID_NONE);

	// Internal public interface
	Chunk* getChunkAtPosition(const f32v2& worldPos);
	Chunk* getChunkOrCreateAtPosition(const f32v2& worldPos);

	
private:
	// Resources
	const EntityComponentSystem& mEcs;
	b2World& mPhysWorld;
	std::unique_ptr<ChunkGenerator> mChunkGenerator;

	// Data
	f32v2 mAxis[2];
	f32v2 mWorldMousePos = f32v2(0.0f);
	bool mDirty = true;
	std::map<ChunkID, std::unique_ptr<Chunk> > mChunks;

	// Rendering
	std::unique_ptr<ChunkRenderer> mChunkRenderer;
};

