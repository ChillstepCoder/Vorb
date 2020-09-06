#pragma once
#include <Vorb/graphics/Texture.h>
#include <Vorb/ecs/Entity.h>
#include <functional>
#include <optional>

#include "actor/ActorTypes.h"
#include "TileSet.h"
#include "world/Tile.h"

#include "world/Chunk.h"

DECL_VG(class SpriteBatch);

struct b2BodyDef;
class b2Body;
class b2World;
class Camera2D;
class ContactListener;
class ChunkRenderer;
class ChunkGenerator;
class EntityComponentSystem;
class ResourceManager;

class World
{
	friend class EntityComponentSystem;
public:
	World(ResourceManager& resourceManager);
	~World();


	void init(EntityComponentSystem& ecs);
	void draw(const Camera2D& camera);
	void update(float deltaTime, const f32v2& playerPos, const Camera2D& camera); // TODO: Multiplayer?
	void updateWorldMousePos(const Camera2D& camera);

	const f32v2& getCurrentWorldMousePos() const { return mWorldMousePos; }

	std::vector<EntityDistSortKey> queryActorsInRadius(const f32v2& pos, float radius, ActorTypesMask includeMask, ActorTypesMask excludeMask, bool sorted, vecs::EntityID except = ENTITY_ID_NONE);
	std::vector<EntityDistSortKey> queryActorsInArc(const f32v2& pos, float radius, const f32v2& normal, float arcAngle, ActorTypesMask includeMask, ActorTypesMask excludeMask, bool sorted, int quadrants, vecs::EntityID except = ENTITY_ID_NONE);

	b2Body* createPhysBody(const b2BodyDef* bodyDef);

	// Internal public interface
	Chunk* getChunkAtPosition(const f32v2& worldPos);
	Chunk* getChunkOrCreateAtPosition(const f32v2& worldPos);
	Chunk* getChunkOrCreateAtPosition(ChunkID chunkId);

	TileHandle getTileHandleAtScreenPos(const f32v2& screenPos, const Camera2D& camera);
	TileHandle getTileHandleAtWorldPos(const f32v2& worldPos);

	
private:

	// Returns true if should be removed
	bool updateChunk(Chunk& chunk);
	void updateChunkNeighbors(Chunk& chunk);
	bool isChunkInLoadDistance(ChunkID chunkId, float addOffset = 0.0f);
	void initChunk(Chunk& chunk, ChunkID chunkId);
	void generateChunk(Chunk& chunk);

	// Resources
	EntityComponentSystem* mEcs = nullptr;
	std::unique_ptr<b2World> mPhysWorld;
	std::unique_ptr<ContactListener> mContactListener;
	std::unique_ptr<ChunkGenerator> mChunkGenerator;

	// Data
	f32v2 mWorldMousePos = f32v2(0.0f);
	f32v2 mLoadRange = f32v2(0.0f);
	f32v2 mLoadCenter = f32v2(0.0f);
	bool mDirty = true;
	
	// TODO: Chunk paging for cache performance on updates
	std::map<ChunkID, std::unique_ptr<Chunk> > mChunks;

	// Rendering
	std::unique_ptr<ChunkRenderer> mChunkRenderer;
};

