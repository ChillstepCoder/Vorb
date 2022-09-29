#include "stdafx.h"
#include "world/World.h"

#include "ecs/EntityComponentSystem.h"
#include "debugging/DebugRenderer.h"
#include "world/ChunkGenerator.h"
#include "resources/TileRepository.h"
#include "weather/CloudManager.h"
#include "item/ItemStockpileRegistry.h"
#include "structure/StructureManager.h"

#include "physics/PhysicsWorld.h"

#include "ecs/factory/EntityFactory.h"

#include <Vorb/ui/InputDispatcher.h>
#include <Vorb/graphics/SpriteBatch.h>
#include <Vorb/graphics/TextureCache.h>
#include <Vorb/math/VectorMath.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/transform.hpp>

#include "rendering/ChunkMesher.h"
#include "rendering/ChunkGrassQuadtree.h"

#include "physics/PhysHitResult.h"

#include "ui/UIContext.h"

#include "util/Utils.h"

#include "city/City.h"
#include "camera/Camera3D.h"

#include "generation/WorldGeneration.h"
#include "pathfinding/NavWorld.h"
#include "pathfinding/NavThread.h"

// TODO: remove?
#include "resources/ResourceManager.h"
#include "particles/ParticleSystemManager.h"

#include "tile/TileUtil.h"

#include "options/DebugOptions.h"


World::World() :
	IWorld::IWorld()
{

	// Cities
	mCities = std::make_unique<CityGraph>();

	// Structures
	mStructureManager = std::make_unique<StructureManager>(*this);

	// Stockpiles
	mItemStockpileRegistry = std::make_unique<ItemStockpileRegistry>(*this);

	// Nav graph
	mNavWorld = std::make_unique<NavWorld>(*this);

    // Weather (Init post load because it contains rendering and requires render context to be initialized, TODO: Fix this)
    mCloudManager = std::make_unique<CloudManager>(*this);


	// Activate the nav thread
	Services::NavThread::ref().init(*this);

}

World::~World() {
	IS_SHUTTING_DOWN = true;
}

const CoarseNavNode* World::tryGetNavNodeAtWorldPos(const ui32v2& worldPos) const {
	const Chunk& chunk = getChunkAtPosition(worldPos); // TODO: Stop casting??
	if (!chunk.isDataReady()) return nullptr;
    ui32 x = (ui32)worldPos.x & (CHUNK_WIDTH - 1); // Fast modulus
    ui32 y = (ui32)worldPos.y & (CHUNK_WIDTH - 1); // Fast modulus
	const Tile& tile = chunk.getTileContainer()->getTileAt(x, y, 0);
	ui16 navNodeIndex = tile.getNavNodeIndex();
	if (navNodeIndex == INVALID_NAV_NODE_INDEX) return nullptr;
	return mNavWorld->getCoarseNavNode({ chunk.getTileContainer()->getId(), navNodeIndex });
}



void World::efficientEnumTileAABB(const ui32AABB2& aabb, std::function<void(Chunk&, TileIndex)> func) {
	assert(IS_MAIN_THREAD());
	// TODO: handle this without asserts
	// Start at bottom left
	ui32v2 worldPos;
	ui32 spanX = CHUNK_WIDTH; // Logically these initial values wont actually be used, but need to please compiler
	ui32 spanY = CHUNK_WIDTH;
	for (worldPos.y = aabb.y; worldPos.y < aabb.y + aabb.depth;) {
        for (worldPos.x = aabb.x; worldPos.x < aabb.x + aabb.depth;) {
            TileHandle cornerHandle = getTerrainTileHandleAtWorldPos(worldPos);
            assert(cornerHandle.container);
            Chunk& chunk = sWorld->getChunk(ChunkID::fromWorldUI32v2(cornerHandle.getWorldPos2D()));
			ui32v3 offset = cornerHandle.getContainerOffset();
			const ui32 distFromRightEdge = CHUNK_WIDTH - offset.x;
            const ui32 distFromTopEdge = CHUNK_WIDTH - offset.y;
            spanX = std::min(distFromRightEdge, aabb.width);
            spanY = std::min(distFromTopEdge, aabb.depth); // TODO: Prob clever way to move this up a loop
			for (ui32 dy = 0; dy < spanY; ++dy) {
				for (ui32 dx = 0; dx < spanX; ++dx) {
					func(chunk, chunk.getTileContainer()->getTileIndexFromXYZOffset(offset.x + dx, offset.y + dy, 0));
				}
			}
			worldPos.x += spanX;
		}
		worldPos.y += spanY;
	}
}

void World::createCityAt(const ui32v2& worldPos) {
	std::unique_ptr<City> newCity = std::make_unique<City>(worldPos, *this);
	mCities->mNodes.emplace_back(std::move(newCity));
}

bool World::tileHasHarvestableResource(const ui32v2& worldPos, TileResource resource, TileLayer* outLayer) {
	TileHandle handle = getTerrainTileHandleAtWorldPos(worldPos);
	if (handle.isValid()) {
		return handle.tile->hasHarvestableResource(resource, outLayer);
	}
	return false;
}
