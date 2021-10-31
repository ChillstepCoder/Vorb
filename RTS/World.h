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

#include "util/IntersectionHit.h"

constexpr float SECONDS_PER_DAY = 1440.0f;
constexpr float HOURS_PER_DAY = 24.0f;
constexpr float SECONDS_PER_HOUR = SECONDS_PER_DAY / HOURS_PER_DAY;

DECL_VG(class SpriteBatch);

struct b2BodyDef;
class b2Body;
class b2World;
class ICamera;
class Camera2D;
class City;
class ContactListener;
class ContactFilter;
class ChunkGenerator;
class ItemStockpileRegistry;
class EntityComponentSystem;
class ResourceManager;
class EntityFactory;
class NavGraph;
struct CityGraph;

class World
{
	friend class EntityComponentSystem;
	friend class WorldEditor;
public:
	World(ResourceManager& resourceManager);
	~World();

	void initPostLoad();
	void update(const f32v2& playerPos, const ICamera& camera);

	std::vector<EntityDistSortKey> queryActorsInRadius(const f32v2& pos, float radius, ActorTypesMask includeMask, ActorTypesMask excludeMask, bool sorted, entt::entity except = INVALID_ENTITY);
	std::vector<EntityDistSortKey> queryActorsInArc(const f32v2& pos, float radius, const f32v2& normal, float arcAngle, ActorTypesMask includeMask, ActorTypesMask excludeMask, bool sorted, int quadrants, entt::entity except = INVALID_ENTITY);

	entt::entity createEntity(const f32v2& pos, const nString& typeName);
	b2Body* createPhysBody(const b2BodyDef* bodyDef);
	void createCityAt(const ui32v2& worldPos);

	void setTileAt(const ui32v2& worldPos, Tile tile);
    void setTileLayerAt(const ui32v2& worldPos, TileID id, TileLayer layer);
    void setTileLayerAt(TileHandle& handle, TileID id, TileLayer layer);
    void setTileFlagAt(const ui32v2& worldPos, TileFlags flag);
    void setTileCollisionNavFlagAt(const ui32v2& worldPos, TileCollisionNavFlags flag);

	bool tileHasHarvestableResource(const ui32v2& worldPos, TileResource resource, TileLayer* outLayer);

	// Internal public interface
    Chunk& getChunkAtPosition(const f32v2& worldPos);
    Chunk& getChunkAtPosition(const ui32v2& worldPos);
    Chunk& getChunkAtPosition(ChunkID chunkId);
    const Chunk& getChunkAtPosition(ChunkID chunkId) const;

    TileHandle getTileFromCameraPickVector(const ICamera& camera, const f32v3& rayDir) const;
    TileHandle getTileHandleAtWorldPos(const f32v2& worldPos) const;
    TileHandle getTileHandleAtWorldPos(const ui32v2& worldPos) const;
    TileCollision getTileCollisionAtWorldPos(const f32v2& worldPos) const;
    TileCollision getTileCollisionAtWorldPos(const ui32v2& worldPos) const;

	const NavNode* tryGetNavNodeAtWorldPos(const ui32v2& worldPos) const;

	const ClientECSData& getClientECSData() const { return mClientEcsData; }
	const ResourceManager& getResourceManager() const { return mResourceManager; }
	EntityComponentSystem& getECS() const { return *mEcs; }
	ItemStockpileRegistry& getItemStockpileRegistry() const { return *mItemStockpileRegistry; }
	const WorldGrid& getWorldGrid() const { return mWorldGrid; }
	const NavGraph& getNavGraph() const { return *mNavGraph; }

    void enumVisibleChunks(std::function<void(const Chunk&)> func) const;
    void enumVisibleRegions(const ICamera& camera, std::function<void(const Region&)> func) const;
	void efficientEnumTileAABB(const ui32AABB2& aabb, std::function<void(Chunk&, Tile&)> func);

	// TODO: Should camera exist in world? Is there a better way than "camera" to determine offset to mouse?
	void updateClientEcsData(Cartesian worldLookCardinalDirection);

    void setTimeOfDay(float time);
	// [-1.0, 1.0]
    float getSunHeight() const { return mSunHeight; }
    const f32v3& getSunPosition() const { return mSunPosition; }
    float getTimeOfDay() const { return mTimeOfDay; }
	const f32v3& getSunColor() const { return mSunColor; }
	const f32m4& getSkyRotMatrix() const { return mSkyRotMatrix; }

	const CityGraph& getCities() const { return *mCities; }
	City* getClosestCityToPoint(const f32v2& pos) const;

    IntersectionHit2D tryGetRaycastIntersect2D(const f32v2& start, const f32v2& end, f32 zPos);
	
private:

	// TODO: Composition? WorldClock? idk
    void updateSun(const ICamera& camera);
    /// Returns true if should be removed
	bool updateChunk(Chunk& chunk);
	void onChunkDataReady(Chunk& chunk);
	void onChunkAllNeighborsDataReady(Chunk& chunk);
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
    std::unique_ptr<ContactFilter> mContactFilter;

	// Generation
    std::unique_ptr<ChunkGenerator> mChunkGenerator;

	// Factories
	std::unique_ptr<EntityFactory> mEntityFactory;

	// Resource handle
	ResourceManager& mResourceManager;

	// Cities
	std::unique_ptr<CityGraph> mCities;

	// Stockpiles
	std::unique_ptr<ItemStockpileRegistry> mItemStockpileRegistry;

	// Nav graph
	std::unique_ptr<NavGraph> mNavGraph;

	// Data
    f32v2 mLoadCenter = f32v2(0.0f);
    f32   mLoadRangeSq = 0.0f;
	// Sunlight
	float mSunHeight = 1.0f;
	f32v3 mSunPosition = f32v3(0.0f, 0.0f, 1.0f);
	float mTimeOfDay = 0.0f; // span of 24:00
	f32v3 mSunColor = f32v3(1.0f);
	f32m4 mSkyRotMatrix = f32m4(1.0f);

	bool mDirty = true;
	// TODO: Chunk paging for tile data?
	WorldGrid mWorldGrid;
    std::vector<Chunk*> mActiveChunks;
    std::vector<Chunk*> mVisibleChunks;
};

