#pragma once
#include <functional>
#include <optional>

#include "actor/ActorTypes.h"
#include "tile/Tile.h"
#include "ecs/factory/EntityType.h"

#include "world/WorldGrid.h"
#include "world/WorldData.h"

#include "util/IntersectionHit.h"

constexpr float SECONDS_PER_DAY = 1440.0f;
constexpr float HOURS_PER_DAY = 24.0f;
constexpr float SECONDS_PER_HOUR = SECONDS_PER_DAY / HOURS_PER_DAY;

class Camera3D;
class City;
class ContactListener;
class ContactFilter;
class ChunkGenerator;
class CloudManager;
class ItemStockpileRegistry;
class EntityComponentSystem;
class EntityFactory;
class NavWorld;
class WorldEditor;
class ChunkMesher;
class HeightmapTerrainQuadtree;
class StructureManager;
class PhysicsWorld;
struct CoarseNavNode;
struct CityGraph;

class World
{
	friend class EntityComponentSystem;
	friend class WorldEditor;
private:
	World();


public:
    ~World();
	World(World const&) = delete;
	void operator=(World const&) = delete;

	static World& getInstance();

	void updateTaskQueues();
	void updateActiveDynamicTiles();

	void tick(const f32v2& playerPos);
	void frameUpdate(const Camera3D& camera, f32 elapsedSec);

	void initPostLoad();

	entt::entity createEntity(const f32v3& pos, const nString& typeName);
	void createCityAt(const ui32v2& worldPos);

	bool tileHasHarvestableResource(const ui32v2& worldPos, TileResource resource, TileLayer* outLayer);

    // Internal public interface
    Chunk& getChunkAtChunkCoords(const ui32v2& worldPos);
    Chunk& getChunkAtPosition(const f32v2& worldPos);
    const Chunk& getChunkAtPosition(const f32v2& worldPos) const;
    Chunk& getChunkAtPosition(const ui32v2& worldPos);
    const Chunk& getChunkAtPosition(const ui32v2& worldPos) const;
    Chunk& getChunkAtPosition(const ui16v2& worldPos);
    const Chunk& getChunkAtPosition(const ui16v2& worldPos) const;
    Chunk& getChunk(ChunkID chunkId);
    const Chunk& getChunk(ChunkID chunkId) const;
    Chunk& getChunk(ui32 chunkId);
    const Chunk& getChunk(ui32 chunkId) const;

    TileHandle getTileHandleAtWorldPosThreadSafe(const i32v3& worldPos) const;
	TileHandle getTileHandleAtWorldPos(const i32v3& worldPos) const;
	TileHandle getTileHandleAtWorldPos(const f32v3& worldPos) const;
	TileHandle getTerrainTileHandleAtWorldPos(const f32v3& worldPos) const { return getTerrainTileHandleAtWorldPos(f32v2(worldPos.x, worldPos.y)); }
    TileHandle getTerrainTileHandleAtWorldPos(const f32v2& worldPos) const;
    TileHandle getTerrainTileHandleAtWorldPos(const ui32v2& worldPos) const;
	StructureArrayPtr tryGetStructuresAtWorldPos(const ui32v2& worldPos) const;
    const f32v2& getLoadCenter() const { return mWorldGrid.mLoadCenter; }

	const CoarseNavNode* tryGetNavNodeAtWorldPos(const ui32v2& worldPos) const;

	EntityComponentSystem& getECS() const { return *mEcs; }
	ItemStockpileRegistry& getItemStockpileRegistry() const { return *mItemStockpileRegistry; }

    WorldGrid& getWorldGrid() { return mWorldGrid; }
    const WorldGrid& getWorldGrid() const { return mWorldGrid; }

    NavWorld& getNavWorld() { return *mNavWorld; }
    const NavWorld& getNavWorld() const { return *mNavWorld; }

    const CloudManager& getCloudManager() const { return *mCloudManager; }

    StructureManager& getStructureManager() { return *mStructureManager; }
    const StructureManager& getStructureManager() const { return *mStructureManager; }

    PhysicsWorld& getPhysicsWorld() { return *mPhysWorld; }
    const PhysicsWorld& getPhysicsWorld() const { return *mPhysWorld; }


    size_t getNumVisibleChunks() const { return mVisibleChunks.size(); }
    size_t getNumActiveChunks() const { return mWorldGrid.getActiveChunks().size(); }
	const std::vector<Chunk*>& getActiveChunks() const { return mWorldGrid.getActiveChunks(); }
    void enumVisibleChunks(std::function<void(const Chunk&)> func) const;
    void enumActiveChunks(std::function<void(const Chunk&)> func) const;
	void efficientEnumTileAABB(const ui32AABB2& aabb, std::function<void(Chunk&, TileIndex)> func);

	void dirtyTerrainFromBrush(const f32v2& pos, f32 brushRadius);

	// TODO: Should camera exist in world? Is there a better way than "camera" to determine offset to mouse?

    void setTimeOfDay(float time);
	// [-1.0, 1.0]
    float getSunHeight() const { return mSunHeight; }
    const f32v3& getSunPosition() const { return mSunPosition; }
    float getTimeOfDay() const { return mTimeOfDay; }
	const f32v3& getSunColor() const { return mSunColor; }
	const f32m4& getSkyRotMatrix() const { return mSkyRotMatrix; }

	const CityGraph& getCities() const { return *mCities; }
	City* getClosestCityToPoint(const f32v2& pos) const;

private:

	// TODO: Composition? WorldClock? idk
    void updateSun();

	// Editor functions
	void editorInvalidateWorldGen();
	void debugRefreshWorldGeneration();

    // ECS
    std::unique_ptr<EntityComponentSystem> mEcs;

	// Factories
	std::unique_ptr<EntityFactory> mEntityFactory;

	// Cities
	std::unique_ptr<CityGraph> mCities;

	// Structures
	std::unique_ptr<StructureManager> mStructureManager;

	// Stockpiles
	std::unique_ptr<ItemStockpileRegistry> mItemStockpileRegistry;

	// Nav graph
	std::unique_ptr<NavWorld> mNavWorld;

	// Clouds
	std::unique_ptr<CloudManager> mCloudManager;

	// Physics
	std::unique_ptr<PhysicsWorld> mPhysWorld;


	// Sunlight
	float mSunHeight = 1.0f;
	f32v3 mSunPosition = f32v3(0.0f, 0.0f, 1.0f);
	float mTimeOfDay = 0.0f; // span of 24:00
	f32v3 mSunColor = f32v3(1.0f);
	f32m4 mSkyRotMatrix = f32m4(1.0f);

	WorldGrid mWorldGrid;
    std::vector<Chunk*> mVisibleChunks; // Client only
};