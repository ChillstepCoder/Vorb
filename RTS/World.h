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

struct b2BodyDef;
class b2Body;
class b2World;
class Camera3D;
class City;
class ContactListener;
class ContactFilter;
class ChunkGenerator;
class CloudManager;
class ItemStockpileRegistry;
class EntityComponentSystem;
class EntityFactory;
class NavGraph;
class WorldEditor;
class ChunkMesher;
class HeightmapTerrainQuadtree;
class StructureManager;
class PhysicsWorld;
struct TerrainNavNode;
struct CityGraph;

class World
{
	friend class EntityComponentSystem;
	friend class WorldEditor;
public:
	World();
	~World();

	void initPostLoad(ChunkMesher& chunkMesher);
	void updateTaskQueues();
	void updateActiveDynamicTiles();

	void tick(const f32v2& playerPos);
	void frameUpdate(const Camera3D& camera, f32 elapsedSec);

	void lazyInit();

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

    TileHandle getTileFromCameraPickVector(const Camera3D& camera, const f32v3& rayDir) const;
    TileHandle getTileHandleAtWorldPos(const f32v2& worldPos) const;
    TileHandle getTileHandleAtWorldPos(const ui32v2& worldPos) const;
    TileHandle getTileHandle(ui32 chunkId, TileIndex tileIndex) const;
    const Tile& getTileAtWorldPos(const f32v2& worldPos) const;
    const Tile* tryGetTileAtWorldPos(const f32v2& worldPos) const;
    const Tile* tryGetTileAtWorldPos(const ui32v2& worldPos) const;
    const Tile* tryGetTileAtWorldPos(const ui16v2& worldPos) const;
	StructureArrayPtr tryGetStructuresAtWorldPos(const ui32v2& worldPos) const;
    const f32v2& getLoadCenter() const { return mLoadCenter; }

	const TerrainNavNode* tryGetNavNodeAtWorldPos(const ui32v2& worldPos) const;

	EntityComponentSystem& getECS() const { return *mEcs; }
	ItemStockpileRegistry& getItemStockpileRegistry() const { return *mItemStockpileRegistry; }

    WorldGrid& getWorldGrid() { return mWorldGrid; }
    const WorldGrid& getWorldGrid() const { return mWorldGrid; }

    NavGraph& getNavGraph() { return *mNavGraph; }
    const NavGraph& getNavGraph() const { return *mNavGraph; }

    const CloudManager& getCloudManager() const { return *mCloudManager; }

	const std::vector<HeightmapTerrainQuadtree>& getTerrainQuadtrees() const { return mTerrainTrees; }

    StructureManager& getStructureManager() { return *mStructureManager; }
    const StructureManager& getStructureManager() const { return *mStructureManager; }

    PhysicsWorld& getPhysicsWorld() { return *mPhysWorld; }
    const PhysicsWorld& getPhysicsWorld() const { return *mPhysWorld; }


    size_t getNumVisibleChunks() const { return mVisibleChunks.size(); }
    size_t getNumActiveChunks() const { return mActiveChunks.size(); }
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
	void debugRefreshWorldGeneration();

    // ECS
    std::unique_ptr<EntityComponentSystem> mEcs;

	// Generation
    std::unique_ptr<ChunkGenerator> mChunkGenerator;

	// Factories
	std::unique_ptr<EntityFactory> mEntityFactory;

	// Cities
	std::unique_ptr<CityGraph> mCities;

	// Structures
	std::unique_ptr<StructureManager> mStructureManager;

	// Stockpiles
	std::unique_ptr<ItemStockpileRegistry> mItemStockpileRegistry;

	// Nav graph
	std::unique_ptr<NavGraph> mNavGraph;

	// Clouds
	std::unique_ptr<CloudManager> mCloudManager;

	// Physics
	std::unique_ptr<PhysicsWorld> mPhysWorld;

	// Meshing
	ChunkMesher* mChunkMesher = nullptr;

	// Data
    f32v2 mLoadCenter = f32v2(0.0f);
	// Sunlight
	float mSunHeight = 1.0f;
	f32v3 mSunPosition = f32v3(0.0f, 0.0f, 1.0f);
	float mTimeOfDay = 0.0f; // span of 24:00
	f32v3 mSunColor = f32v3(1.0f);
	f32m4 mSkyRotMatrix = f32m4(1.0f);

	bool mNeedsLazyInit = true;
	bool mDirty = true;

	WorldGrid mWorldGrid;
    std::vector<Chunk*> mActiveChunks;
    std::vector<Chunk*> mVisibleChunks; // Client only
	std::vector<HeightmapTerrainQuadtree> mTerrainTrees;
};