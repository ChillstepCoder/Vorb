#pragma once
#include <Vorb/graphics/Texture.h>
#include <functional>
#include <optional>

#include "actor/ActorTypes.h"
#include "TileSet.h"
#include "world/Tile.h"
#include "ecs/ClientEcsData.h"
#include "ecs/factory/EntityType.h"
#include "services/Services.h"

#include "world/Chunk.h"

constexpr float SECONDS_PER_DAY = 720.0f;
constexpr float HOURS_PER_DAY = 24.0f;
constexpr float SECONDS_PER_HOUR = SECONDS_PER_DAY / HOURS_PER_DAY;

DECL_VG(class SpriteBatch);

struct b2BodyDef;
class b2Body;
class b2World;
class Camera2D;
class ContactListener;
class ChunkGenerator;
class EntityComponentSystem;
class ResourceManager;
class EntityFactory;

class World
{
	friend class EntityComponentSystem;
public:
	World(ResourceManager& resourceManager);
	~World();

	void update(float deltaTime, const f32v2& playerPos, const Camera2D& camera);

	std::vector<EntityDistSortKey> queryActorsInRadius(const f32v2& pos, float radius, ActorTypesMask includeMask, ActorTypesMask excludeMask, bool sorted, entt::entity except = (entt::entity)0);
	std::vector<EntityDistSortKey> queryActorsInArc(const f32v2& pos, float radius, const f32v2& normal, float arcAngle, ActorTypesMask includeMask, ActorTypesMask excludeMask, bool sorted, int quadrants, entt::entity except = (entt::entity)0);

	entt::entity createEntity(const f32v2& pos, EntityType type);
	b2Body* createPhysBody(const b2BodyDef* bodyDef);

	// Internal public interface
    Chunk* tryGetChunkAtPosition(const f32v2& worldPos);
    Chunk* tryGetChunkAtPosition(ChunkID chunkId);
    const Chunk* tryGetChunkAtPosition(ChunkID chunkId) const;
	Chunk* getChunkOrCreateAtPosition(const f32v2& worldPos);
	Chunk* getChunkOrCreateAtPosition(ChunkID chunkId);

	TileHandle getTileHandleAtScreenPos(const f32v2& screenPos, const Camera2D& camera);
	TileHandle getTileHandleAtWorldPos(const f32v2& worldPos);

	const ClientECSData& getClientECSData() const { return mClientEcsData; }
	const ResourceManager& getResourceManager() const { return mResourceManager; }
	EntityComponentSystem& getECS() const { return *mEcs; }

    void enumVisibleChunks(const Camera2D& camera, std::function<void(const Chunk& chunk)> func) const;
    void enumVisibleChunksSpiral(const Camera2D& camera, std::function<void(const Chunk& chunk)> func) const;

	// TODO: Should camera exist in world? Is there a better way than "camera" to determine offset to mouse?
	void updateClientEcsData(const Camera2D& camera);

    void setTimeOfDay(float time);
	// [-1.0, 1.0]
    float getSunHeight() const { return mSunHeight; }
    float getSunPosition() const { return mSunPosition; }
    float getTimeOfDay() const { return mTimeOfDay; }
	const f32v3& getSunColor() const { return mSunColor; }
	
private:

	// TODO: Composition? WorldClock? idk
    void updateSun();
    /// Returns true if should be removed
	bool updateChunk(Chunk& chunk);
	void updateChunkNeighbors(Chunk& chunk);
	bool isChunkInLoadDistance(ChunkID chunkId, float addOffset = 0.0f);
	void initChunk(Chunk& chunk, ChunkID chunkId);
	void generateChunkAsync(Chunk& chunk);

    // ECS
    ClientECSData mClientEcsData;
    std::unique_ptr<EntityComponentSystem> mEcs;

	// Physics
	std::unique_ptr<b2World> mPhysWorld;
	std::unique_ptr<ContactListener> mContactListener;

	// Generation
    std::unique_ptr<ChunkGenerator> mChunkGenerator;

	// Factories
	std::unique_ptr<EntityFactory> mEntityFactory;

	// Resource handle
	ResourceManager& mResourceManager;

	// Data
	f32v2 mViewRange = f32v2(0.0f);
	f32v2 mLoadRange = f32v2(0.0f);
	f32v2 mLoadCenter = f32v2(0.0f);
	// Sunlight
	float mSunHeight = 1.0f;
	float mSunPosition = 0.0f; // [-1, 1]
	float mTimeOfDay = 0.0f; // span of 24:00
	f32v3 mSunColor = f32v3(1.0f);

	bool mDirty = true;
	// TODO: Chunk paging for cache performance on updates?
	std::map<ChunkID, std::unique_ptr<Chunk> > mChunks;
};

