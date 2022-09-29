#pragma once
#include <functional>
#include <optional>

#include "actor/ActorTypes.h"
#include "tile/Tile.h"
#include "ecs/factory/EntityType.h"

#include "world/WorldData.h"

#include "util/IntersectionHit.h"

#include "IWorld.h"

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
class CityGraph;

class World : public IWorld
{
	friend class EntityComponentSystem;
	friend class WorldEditor;
private:
	World();


public:
    ~World();
	World(World const&) = delete;
	void operator=(World const&) = delete;

	void createCityAt(const ui32v2& worldPos);

	bool tileHasHarvestableResource(const ui32v2& worldPos, TileResource resource, TileLayer* outLayer);

	const CoarseNavNode* tryGetNavNodeAtWorldPos(const ui32v2& worldPos) const;

	void efficientEnumTileAABB(const ui32AABB2& aabb, std::function<void(Chunk&, TileIndex)> func);
	

private:


};