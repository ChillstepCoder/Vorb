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

#include "world/WorldGrid.h"
#include "world/WorldData.h"

constexpr float SECONDS_PER_DAY = 1440.0f;
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
struct CityGraph;

class World
{
	friend class EntityComponentSystem;
	friend class WorldEditor;
public:
	World(ResourceManager& resourceManager);
	~World();

	void initPostLoad();
	void update(float deltaTime, const f32v2& playerPos, const Camera2D& camera);

	std::vector<EntityDistSortKey> queryActorsInRadius(const f32v2& pos, float radius, ActorTypesMask includeMask, ActorTypesMask excludeMask, bool sorted, entt::entity except = (entt::entity)0);
	std::vector<EntityDistSortKey> queryActorsInArc(const f32v2& pos, float radius, const f32v2& normal, float arcAngle, ActorTypesMask includeMask, ActorTypesMask excludeMask, bool sorted, int quadrants, entt::entity except = (entt::entity)0);

	entt::entity createEntity(const f32v2& pos, EntityType type);
	b2Body* createPhysBody(const b2BodyDef* bodyDef);
	void createCityAt(const ui32v2& worldPos);

	void setTileAt(const ui32v2& worldPos, Tile tile);

	// Internal public interface
    Chunk& getChunkAtPosition(const f32v2& worldPos);
    Chunk& getChunkAtPosition(ChunkID chunkId);
    const Chunk& getChunkAtPosition(ChunkID chunkId) const;

	TileHandle getTileHandleAtScreenPos(const f32v2& screenPos, const Camera2D& camera) const;
	TileHandle getTileHandleAtWorldPos(const f32v2& worldPos) const;

	const ClientECSData& getClientECSData() const { return mClientEcsData; }
	const ResourceManager& getResourceManager() const { return mResourceManager; }
	EntityComponentSystem& getECS() const { return *mEcs; }

    void enumVisibleChunks(const Camera2D& camera, std::function<void(const Chunk& chunk)> func) const;
    void enumVisibleRegions(const Camera2D& camera, std::function<void(const Region& chunk)> func) const;

	// TODO: Should camera exist in world? Is there a better way than "camera" to determine offset to mouse?
	void updateClientEcsData(const Camera2D& camera);

    void setTimeOfDay(float time);
	// [-1.0, 1.0]
    float getSunHeight() const { return mSunHeight; }
    float getSunPosition() const { return mSunPosition; }
    float getTimeOfDay() const { return mTimeOfDay; }
	const f32v3& getSunColor() const { return mSunColor; }
	const CityGraph& getCities() const { return *mCities; }
	
private:

	// TODO: Composition? WorldClock? idk
    void updateSun();
    /// Returns true if should be removed
	bool updateChunk(Chunk& chunk);
	void onChunkDataReady(Chunk& chunk);
	void dataReadyTryNotifyNeighbor(Chunk& chunk, const ChunkID& id);
	void tryCreateNeighbors(Chunk& chunk);
	void tryCreateNeighbor(Chunk& chunk, const ChunkID& id);
	bool isChunkInLoadDistance(const ChunkID& chunkId, float addOffset = 0.0f);

	void initChunk(Chunk& chunk);
	void generateChunkAsync(Chunk& chunk);

	// Editor functions
	void editorInvalidateWorldGen();

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

	// Cities
	std::unique_ptr<CityGraph> mCities;

	// Data
	f32v2 mViewRange = f32v2(0.0f);
    f32v2 mLoadCenter = f32v2(0.0f);
    f32   mLoadRangeSq = 0.0f;
	// Sunlight
	float mSunHeight = 1.0f;
	float mSunPosition = 0.0f; // [-1, 1]
	float mTimeOfDay = 0.0f; // span of 24:00
	f32v3 mSunColor = f32v3(1.0f);

	bool mDirty = true;
	// TODO: Chunk paging for tile data?
	WorldGrid mWorldGrid;
	std::vector<Chunk*> mActiveChunks;
};

