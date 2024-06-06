#include "stdafx.h"
#include "NavWorld.h"

#include "world/World.h"
#include "world/Chunk.h"
#include "world/IChunkGrid.h"
#include "world/IHeightmapGrid.h"
#include "debugging/DebugRenderer.h"
#include "options/DebugOptions.h"

#include "tile/TileContainerRepository.h"

#include "time/TimestepManager.h"

#include "building/building.h"

#include "debugging/VisualLogger.h"

#include "resources/TileRepository.h"

constexpr ui32 GRID_WIDTH = SUBCHUNK_WIDTH;
static_assert(SUBCHUNK_WIDTH == 16);

// 0 or 1 for rendering debug
#define DEBUG_RENDER_NAV_NODES 1

// Distance threshold where we can still navigate across blocks
constexpr f32 FINE_NAV_HEIGHT_THRESHOLD = 3.0f / 4.0f + 0.05f;

// Harvestable reservation
constexpr f64 RESERVE_DURATION_SEC = 10.0;

//inline f32v3 helperGet3DPoint(const IHeightmapGrid& heightGrid, const f32v2& pos2d) {
//    return f32v3(pos2d.x, pos2d.y, heightGrid.computeHeightAtPoint(pos2d));
//}

boost::container::flat_set<ChunkID> getChunkDependenciesForContainer(IChunkGrid& chunkGrid, const i32v2& pos, const i32v2& dims) {
    boost::container::flat_set<ChunkID> chunkDependencies;
    chunkDependencies.reserve(4);
    i32v2 worldXY = i32v2(pos.x, pos.y);
    chunkDependencies.insert(chunkGrid.getChunkIDFromWorldPos(worldXY));
    worldXY = i32v2(pos.x + dims.x, pos.y);
    chunkDependencies.insert(chunkGrid.getChunkIDFromWorldPos(worldXY));
    worldXY = i32v2(pos.x, pos.y + dims.y);
    chunkDependencies.insert(chunkGrid.getChunkIDFromWorldPos(worldXY));
    worldXY = i32v2(pos.x + dims.x, pos.y + dims.y);
    chunkDependencies.insert(chunkGrid.getChunkIDFromWorldPos(worldXY));
    return chunkDependencies;
}

NavWorld::NavWorld(World& world) : mWorld(world) {
    // TODO: This is arbitrary
    mNavGraphs.reserve(100);
    const ui32 totalChunks = mWorld.getChunkGrid().getTotalChunks();
    assert(totalChunks);
    mTerrainTileContainers = std::unique_ptr<TileContainerID[]>(new TileContainerID[totalChunks]);
    for (int i = 0; i < totalChunks; ++i) {
        mTerrainTileContainers[i] = INVALID_TILE_CONTAINER_ID;
    }
    mChunkBuildingEdges = std::make_unique<ChunkBuildingEdgeList[]>(totalChunks);
    mChunkPendingBuildingNavmeshCounts = std::make_unique<i32[]>(totalChunks);
    initEventHandlers();
}

NavWorld::~NavWorld()
{

}

void NavWorld::tickGameThread() {
    ASSERT_GAME_THREAD();
    mDirtyBuildingTileContainers.gameThreadCopyToWorkerThread();
    mDirtyChunkTileContainers.gameThreadCopyToWorkerThread();
    mContainersToDestroy.gameThreadCopyToWorkerThread();
}

void NavWorld::updateNavThread()
{
    PROFILE_FUNCTION();
    ASSERT_NAV_THREAD();

    constexpr int MAX_BULK_DEQUE_TASK_DATA = 32;
    NavGraphBuildTaskData taskDataBulk[MAX_BULK_DEQUE_TASK_DATA];

    while (size_t count = mFinishedNavGraphBuildTasks.try_dequeue_bulk(taskDataBulk, MAX_BULK_DEQUE_TASK_DATA)) {
        for (size_t i = 0; i < count; ++i) {
            finishNavGraphBuildTask(taskDataBulk[i]);
        }
    }

    // Update dirty tile containers
    {
        // Buildings first as chunks are dependant on buildings
        boost::container::flat_set<const TileContainer*> dirtyContainers;

        mDirtyBuildingTileContainers.workerThreadAquireAllDirtyObjects(dirtyContainers);
        for (auto&& container : dirtyContainers) {
            tryBeginNavmeshTaskForContainer(container);
        }

        mDirtyChunkTileContainers.workerThreadAquireAllDirtyObjects(dirtyContainers);
        for (auto&& container : dirtyContainers) {
            tryBeginNavmeshTaskForContainer(container);
        }
    }

    // Destroy tile containers 
    std::vector<TileContainerToDestroy> containersToDestroy;
    mContainersToDestroy.workerThreadAquireAllDirtyObjects(containersToDestroy);
    for (size_t i = 0; i < containersToDestroy.size(); ++i) {
        TileContainerToDestroy containerData = containersToDestroy[i];
        mNavGraphs.erase(containerData.id);

        if (!containerData.isTerrain) {
            const boost::container::flat_set<ChunkID> chunkDependencies = getChunkDependenciesForContainer(mWorld.getChunkGrid(), containerData.worldPos, containerData.dims); // TODO: boost::container::flat_set?
            for (ChunkID chunkId : chunkDependencies) {
                ChunkBuildingEdgeList& edgeList = mChunkBuildingEdges[chunkId];
                for (int j = edgeList.size() - 1; j >= 0; --j) {
                    if (edgeList[j].buildingContainerId == containerData.id) {
                        edgeList[j] = edgeList.back();
                        edgeList.pop_back();
                        // Note that we do not dirty the chunk here, this is because for the building to destroy, the chunk is also
                        // destroying
                        // TODO: This is not always true! If a building is leveled in full sim we will have a bug here
                    }
                }
            }
        }

        // Remove from spatial lookup
        const i32v2 worldPos2D(containerData.worldPos.x, containerData.worldPos.y);
        if (containerData.isTerrain) {
            ChunkID chunkID = mWorld.getChunkGrid().getChunkIDFromWorldPos(containerData.worldPos);
            mTerrainTileContainers[chunkID] = INVALID_TILE_CONTAINER_ID;
        }
        else {
            NavBBox newBox(NavBoxPoint(worldPos2D.x, worldPos2D.y), NavBoxPoint(worldPos2D.x + containerData.dims.x, worldPos2D.y + containerData.dims.y));
            mSpatialLookup.remove(ContainerNavRegion{ newBox, containerData.id });
        }
    }

    // Cleanup old harvestable reservations
    const f64 timeStampNow = Services::TimestepManager::ref().getCurrentTimeSec();
    for (auto it = mReservedHarvestables.begin(); it != mReservedHarvestables.end();) {
        if (it->second - timeStampNow > RESERVE_DURATION_SEC) {
            it = mReservedHarvestables.erase(it);
        }
        else {
            ++it;
        }
    }
}

typedef std::pair<Cartesian, Cartesian> CartesianPair;
constexpr CartesianPair CARTESIAN_DIAGONAL_OPPOSITES[8] = {
    CartesianPair(Cartesian::EAST, Cartesian::NORTH), //SOUTH_WEST
    CartesianPair(Cartesian::NONE, Cartesian::NONE), //SOUTH 
    CartesianPair(Cartesian::WEST, Cartesian::NORTH), //SOUTH_EAST
    CartesianPair(Cartesian::NONE, Cartesian::NONE), //WEST
    CartesianPair(Cartesian::NONE, Cartesian::NONE), //EAST
    CartesianPair(Cartesian::SOUTH, Cartesian::EAST), //NORTH_WEST
    CartesianPair(Cartesian::NONE, Cartesian::NONE), //NORTH
    CartesianPair(Cartesian::SOUTH, Cartesian::WEST), //NORTH_EAST
};

// toDir = south means we enter from the north
bool canEnterTileInDirection(const Tile& tile, Cartesian toDir) {
    if (!IsTileNavBlocked(tile.getFlags())) {
        const Cartesian fromDir = CARTESIAN_OPPOSITES[e_cast(toDir)];
        if (!tile.canNavInDirection(CARTESIAN_TO_CARTESIAN8[e_cast(fromDir)])) return false;
        return true;
    }
    return false;
}
bool canEnterTileInDirectionDiagonal(const Tile& tile, Cartesian8 toDir) {
    if (!IsTileNavBlocked(tile.getFlags())) {
        const CartesianPair fromDirs = CARTESIAN_DIAGONAL_OPPOSITES[e_cast(toDir)];
        if (!tile.canNavInDirection(CARTESIAN_TO_CARTESIAN8[e_cast(fromDirs.first)]) || !tile.canNavInDirection(CARTESIAN_TO_CARTESIAN8[e_cast(fromDirs.second)])) return false;
        return true;
    }
    return false;
}

// ALGORITHM DESCRIPTION
// 1. Build disjoint sets, with a specific grid size for a maximum disjoint set node width
// 2. Create nav nodes from distinct disjoint set nodes
// 3. Create edges between disjoint set nodes and each other, or "External" which means it connects to the outside world

bool tryGetNavNodeIndexForTile(const NavGraphTileDataToCopy& navTileData, const TileIndex index, DisjointSetNode& outNavNodeIndex) {
    const DisjointSetNode djNodeIndex = navTileData.tileDjNodeIDs[index];
    if (djNodeIndex == INVALID_NAV_NODE_INDEX) {
        return false;
    }

    outNavNodeIndex = navTileData.djNodes[djNodeIndex];
    // This must be part of a nav node
    if (outNavNodeIndex == INVALID_NAV_NODE_INDEX) {
        return false;
    }

    return true;
}

void NavWorld::buildNavGraphForContainer(const TileContainer& tileContainer, OPT TerrainExternalEdges* terrainExternalEdges) {
    // Worker thread only
    assert(!IS_GAME_THREAD());
    PROFILE_FUNCTION();

    TileRepository& tileRepo = TileRepository::get();
    NavGraphBuildTaskData taskData;
    taskData.container = &tileContainer;
    NavGraphTileDataToCopy& navTileData = taskData.navTileData;
    CoarseNavGraph& coarseNavGraph = taskData.navGraph;
    std::vector<TileFineNavData>& fineNavData = taskData.fineNavData;
    const i32v3 dims = tileContainer.getTileSpatialGrid().getDims();
    const f32 floorHeight = tileContainer.getTileSpatialGrid().getFloorHeight();
    const bool isTerrain = tileContainer.isTerrain();
    // We only care about external edges for non terrain
    ChunkBuildingEdgeListOutput* externalEdges = nullptr;
    ChunkBuildingEdgeListOutput localEdgeList;
    if (!isTerrain) {
        externalEdges = &localEdgeList;
        externalEdges->reserve(6);
        assert(!terrainExternalEdges);
    }
    else {
        assert(dims.x == CHUNK_WIDTH && dims.y == CHUNK_WIDTH && dims.z == 1);
    }

    //ScopedTimer timer("Built nav graph");

    /* Chunk* chunk = nullptr;
     if (isTerrain) {
         chunk = &mWorld.getChunk(ChunkID(f32v2(tileContainer.getWorldPos2D())));
     }*/

    ui32 totalDjSets = 0;

    // Reserve nodes
    navTileData.djNodes.reserve(SQ(GRID_WIDTH) * dims.z);

    // ================= Create disjoint set and fine nav data =================
    ContainerNavDataCopy tileData;
    tileContainer.copyDataWorkerThread(tileData);
    const std::vector<Tile>& tiles = tileData.tiles;
    const TileWallContainer& tileWalls = tileData.walls;
    const std::vector<HarvestableSubchunkRegistry>& harvestables = tileData.harvestables;
    fineNavData.resize(tiles.size());
    const BitArray& ownedDTiles = tileData.ownedDTiles;
    const ui32v2 dimsDTiles = ui32v2(tileData.spatialGrid.getDims().x >> 1, tileData.spatialGrid.getDims().y >> 1);

    navTileData.tileDjNodeIDs.resize(tiles.size(), INVALID_DJ_NODE_ID);

    TileIndex index = 0;
    for (int tz = 0; tz < dims.z; ++tz) {
        for (int ty = 0; ty < dims.y; ++ty) {
            const int gridYOffset = ty % GRID_WIDTH;
            for (int tx = 0; tx < dims.x; ++tx, ++index) {
                TileFineNavData& tileFineNavData = fineNavData[index];
                tileFineNavData.reset();

                // Determine if we own this tile
                if (!isInteriorTile(ownedDTiles, index, dimsDTiles, tiles)) {
                    tileFineNavData.pathWeight = 0;
                    continue;
                }
                const Tile& tile = tiles[index];
                tileFineNavData.zPositionOffsetFromFloor = tile.getGroundZOffset();
                // Impassible or empty tiles are not part of navgraph
                if (IsTileNavBlocked(tile.getFlags())) {
                    tileFineNavData.pathWeight = 0;
                    continue;
                }
                // Only terrain tiles can be empty
                if (!isTerrain && tile.isEmpty()) {
                    continue;
                }
                tileFineNavData.isOwned = true;

                TileWalls walls;
                tileWalls.getWallsAtTile(walls, index);
                const f32 groundZPosition = tileFineNavData.zPositionOffsetFromFloor + tz * floorHeight;
                bool assigned = false;

                // ================= Fine Nav Data =================
                // TODO: which tile do we use for path weight?
                const TileID groundId = tile.getGroundID();
                const TileID mainId = tile.getMainID();
                if (groundId != TILE_ID_NONE) {
                    tileFineNavData.pathWeight = tileRepo.getLoadedOrUnloadedAsset(groundId).pathWeight;
                }
                if (mainId != TILE_ID_NONE) {
                    // Floating point multiply
                    tileFineNavData.pathWeight = ui8(((tileFineNavData.pathWeight / 255.0f) * ((f32)tileRepo.getLoadedOrUnloadedAsset(mainId).pathWeight / 255.0f)) * 255.0f);
                }

                // Zero path weight means this is not navable
                if (tileFineNavData.pathWeight == 0) {
                    continue;
                }

                bool canGoSouthWest = true;
                bool canGoSouthEast = true;
                bool canGoNorthWest = true;
                bool canGoNorthEast = true;
                // South
                if (walls.south.canNavThrough() && tile.canNavInDirection(Cartesian8::SOUTH)) {
                    const f32 offset = tile.getEdgeHeightOffset(Cartesian::SOUTH);
                    int z = (groundZPosition + offset + 0.001f) / floorHeight;
                    TileIndex adjIndex;
                    if (z != tz && z > 0 && z < dims.z) {
                        adjIndex = TileSpatialGrid::getTileIndexFromXYZOffset(i32v3(tx, ty - 1, z), dims);
                    } else {
                        adjIndex = index - dims.x;
                    }
                    const bool isInner = ty > 0 && (!terrainExternalEdges || !terrainExternalEdges->isExternal(index, Cartesian::SOUTH));
                    if (!trySetFineNavEdgeCartesian(index, adjIndex, Cartesian8::SOUTH, isInner, dims, tiles, ownedDTiles, groundZPosition, floorHeight, tileFineNavData, tz, externalEdges)) {
                        canGoSouthWest = canGoSouthEast = false;
                    }
                }
                else {
                    canGoSouthWest = canGoSouthEast = false;
                }
                // West
                if (walls.west.canNavThrough() && tile.canNavInDirection(Cartesian8::WEST)) {
                    const f32 offset = tile.getEdgeHeightOffset(Cartesian::WEST);
                    int z = (groundZPosition + offset + 0.001f) / floorHeight;
                    TileIndex adjIndex;
                    if (z != tz && z > 0 && z < dims.z) {
                        adjIndex = TileSpatialGrid::getTileIndexFromXYZOffset(i32v3(tx - 1, ty, z), dims);
                    }
                    else {
                        adjIndex = index - 1;
                    }
                    const bool isInner = tx > 0 && (!terrainExternalEdges || !terrainExternalEdges->isExternal(index, Cartesian::WEST));
                    if (!trySetFineNavEdgeCartesian(index, adjIndex, Cartesian8::WEST, isInner, dims, tiles, ownedDTiles, groundZPosition, floorHeight, tileFineNavData, tz, externalEdges)) {
                        canGoSouthWest = canGoNorthWest = false;
                    }
                }
                else {
                    canGoSouthWest = canGoNorthWest = false;
                }
                // East
                if (walls.east.canNavThrough() && tile.canNavInDirection(Cartesian8::EAST)) {
                    const f32 offset = tile.getEdgeHeightOffset(Cartesian::EAST);
                    int z = (groundZPosition + offset + 0.001f) / floorHeight;
                    TileIndex adjIndex;
                    if (z != tz && z > 0 && z < dims.z) {
                        adjIndex = TileSpatialGrid::getTileIndexFromXYZOffset(i32v3(tx + 1, ty, z), dims);
                    }
                    else {
                        adjIndex = index + 1;
                    }
                    const bool isInner = tx < dims.x - 1 && (!terrainExternalEdges || !terrainExternalEdges->isExternal(index, Cartesian::EAST));
                    if (!trySetFineNavEdgeCartesian(index, adjIndex, Cartesian8::EAST, isInner, dims, tiles, ownedDTiles, groundZPosition, floorHeight, tileFineNavData, tz, externalEdges)) {
                        canGoSouthEast = canGoNorthEast = false;
                    }
                }
                else {
                    canGoSouthEast = canGoNorthEast = false;
                }
                // North
                if (walls.north.canNavThrough() && tile.canNavInDirection(Cartesian8::NORTH)) {
                    const f32 offset = tile.getEdgeHeightOffset(Cartesian::NORTH);
                    int z = (groundZPosition + offset + 0.001f) / floorHeight;
                    if (z < 0) z = 0;
                    TileIndex adjIndex;
                    if (z != tz && z > 0 && z < dims.z) {
                        adjIndex = TileSpatialGrid::getTileIndexFromXYZOffset(i32v3(tx, ty + 1, z), dims);
                    }
                    else {
                        adjIndex = index + dims.x;
                    }
                    const bool isInner = ty < dims.y - 1 && (!terrainExternalEdges || !terrainExternalEdges->isExternal(index, Cartesian::NORTH));
                    if (!trySetFineNavEdgeCartesian(index, adjIndex, Cartesian8::NORTH, isInner, dims, tiles, ownedDTiles, groundZPosition, floorHeight, tileFineNavData, tz, externalEdges)) {
                        canGoNorthWest = canGoNorthEast = false;
                    }
                }
                else {
                    canGoNorthWest = canGoNorthEast = false;
                }

                // South West
                if (canGoSouthWest && tile.canNavInDirection(Cartesian8::SOUTH_WEST)) {
                    trySetFineNavEdgeCartesianDiagonal(index - dims.x - 1, Cartesian8::SOUTH_WEST, ty > 0 && tx > 0, dims, tiles, ownedDTiles, groundZPosition, tileFineNavData);
                }
                // South East
                if (canGoSouthEast && tile.canNavInDirection(Cartesian8::SOUTH_EAST)) {
                    trySetFineNavEdgeCartesianDiagonal(index - dims.x + 1, Cartesian8::SOUTH_EAST, ty > 0 && tx < dims.x - 1, dims, tiles, ownedDTiles, groundZPosition, tileFineNavData);
                }
                // North West
                if (canGoNorthWest && tile.canNavInDirection(Cartesian8::NORTH_WEST)) {
                    trySetFineNavEdgeCartesianDiagonal(index + dims.x - 1, Cartesian8::NORTH_WEST, ty < dims.y - 1 && tx > 0, dims, tiles, ownedDTiles, groundZPosition, tileFineNavData);
                }
                // North East
                if (canGoNorthEast && tile.canNavInDirection(Cartesian8::NORTH_EAST)) {
                    trySetFineNavEdgeCartesianDiagonal(index + dims.x + 1, Cartesian8::NORTH_EAST, ty < dims.y - 1 && tx < dims.x - 1, dims, tiles, ownedDTiles, groundZPosition, tileFineNavData);
                }

                // ================= Disjoint Set =================
                const int gridXOffset = tx % GRID_WIDTH;
                if (gridXOffset != 0) {
                    if (tileFineNavData.canAccessDirection(Cartesian8::WEST) && (tileFineNavData.getEdgeType(Cartesian::WEST) != TileFineNavEdgeType::EXTERIOR)) {
                        navTileData.tileDjNodeIDs[index] = navTileData.tileDjNodeIDs[index - 1];
                        assigned = true;
                    }
                }
                if (gridYOffset != 0) {
                    const Tile& bottom = tiles[index - dims.x];
                    // Check if we can cross between
                    if (tileFineNavData.canAccessDirection(Cartesian8::SOUTH) && (tileFineNavData.getEdgeType(Cartesian::SOUTH) != TileFineNavEdgeType::EXTERIOR)) {
                        if (assigned) {
                            // If we already assigned to left, merge the sets
                            ui16 prevID = navTileData.tileDjNodeIDs[index];
                            ui16 botID = navTileData.tileDjNodeIDs[index - dims.x];
                            // TODO: why do we need this check?
                            if (prevID != INVALID_DJ_NODE_ID && botID != INVALID_DJ_NODE_ID) {
                                //assert(navTileData.subchunkIndices[prevID] == navTileData.subchunkIndices[botID]);
                                // "BUG HERE?"
                                navTileData.djNodes[prevID] = navTileData.djNodes[botID];
                            }
                        }
                        else {
                            navTileData.tileDjNodeIDs[index] = navTileData.tileDjNodeIDs[index - dims.x];
                            assigned = true;
                        }
                    }
                }

                // If we haven't been joined, make a new node
                if (!assigned) {
                    // TODO: I think this is a bug, tileDjNodeIDs isnt changed when we merge above ^^ see "BUG HERE?"
                    navTileData.tileDjNodeIDs[index] = totalDjSets;
                    navTileData.djNodes.emplace_back(totalDjSets);
                    ++totalDjSets;
                    if (totalDjSets == INVALID_DJ_NODE_ID) {
                        pError("Too many disjoint sets in tile container!");
                        assert(false);
                        return;
                    }
                }
            }
        }
    }
    assert(totalDjSets < UINT16_MAX && "If this fails we need to increase maximum nodes or delete the navmesh cause it too big");
    
    //std::cout << " A " << timer.stop() << std::endl;
    // ================= Allocate nav nodes =================
    if (totalDjSets) {
        // Compress DJ nodes to remove orphaned ones
        std::map<ui16 /*NavNodeIndices*/, std::set<ui16> /*DJPositions*/> nodes;
        for (ui32 i = 0; i < totalDjSets; ++i) {
            auto&& it = nodes.find(navTileData.djNodes[i]);
            if (it != nodes.end()) {
                it->second.insert(i);
            }
            else {
                nodes.insert(std::make_pair(navTileData.djNodes[i], std::set<ui16>({ (ui16)i })));
            }
        }
        // Compress
        totalDjSets = nodes.size();
        ui32 i = 0;
        for (auto&& it : nodes) {
            for (auto&& it2 : it.second) {
                navTileData.djNodes[it2] = i;
                //navTileData.subchunkIndices[it2] = it.second.second;
            }
            ++i;
        }

        // Allocate the graph
        coarseNavGraph.numNodes = totalDjSets;
        coarseNavGraph.nodes = std::unique_ptr<CoarseNavNode[]>(new CoarseNavNode[totalDjSets]);
        coarseNavGraph.harvestablesLookup.init(totalDjSets);
      
    }
    else {
        // This graph is completely un-navable
        coarseNavGraph.numNodes = 0;
        coarseNavGraph.nodes = nullptr;
        coarseNavGraph.tileCoarseNavIndices = nullptr;
        mFinishedNavGraphBuildTasks.enqueue(std::move(taskData));
        return;
    }
    
    assert(taskData.navGraph.numNodes);
    // ================= Build edges =================
    // TODO: Optimize allocate
    std::vector<std::vector<CoarseNavNodeEdge>> nodeEdges;
    nodeEdges.resize(coarseNavGraph.numNodes);
    ui32 edgeCount = 0;
    // Lets us know what edges are at a given tile
    std::vector<CoarseTileEdgePointer> tileEdgePointers;
    tileEdgePointers.resize(tiles.size());

    // Build coarse edges and track harvestables
#pragma region BUILD_COARSE_AND_TRACK_HARVESTABLES
    for (int tz = 0; tz < dims.z; ++tz) {
        for (int ty = 0; ty < dims.y; ++ty) {
            for (int tx = 0; tx < dims.x; ++tx) {
                // Determine if we own this tile
                const TileIndex index = tileContainer.getTileSpatialGrid().getTileIndexFromXYZOffset(tx, ty, tz);
                const TileFineNavData& fineData = fineNavData[index];

                // Non pathable tiles are not navable, but may represent resources
                if (fineData.pathWeight == 0) {
                    const SubchunkIndex subchunkIndex = tileContainer.getSubchunkIndexFromTileIndex(index);
                    const HarvestableSubchunkRegistry& harvestableRegistry = tileData.harvestables[subchunkIndex];
                    const auto& hit = harvestableRegistry.mHarvestablePositions.find(index);
                    if (hit != harvestableRegistry.mHarvestablePositions.end()) {
                        // This is a navmesh blocking resource such as a tree or boulder
                        // We should list it in all adjacent DJ nav nodes as an accessible resource
                        
                        // TODO: Large harvestable sends tendrils further and sets 8 spots instead of 4?
                        if (tiles[index].hasFlag(TileFlags::LARGE_BLOCKER)) {
                            // 8 neighbors past the 4 closest neighbors, which are blocked
                            // __O__
                            // _OxO_
                            // OxxxO
                            // _OxO_
                            // __O__
                            DisjointSetNode navNodeIndex;
                            if (ty > 0) {
                                if (ty > 1) { // SOUTH
                                    if (tryGetNavNodeIndexForTile(navTileData, index - dims.x - dims.x, navNodeIndex)) {
                                        coarseNavGraph.harvestablesLookup.setNodeHarvestable(navNodeIndex, hit->second, hit->first);
                                    }
                                }
                                if (tx > 0) { // SOUTH_WEST
                                    if (tryGetNavNodeIndexForTile(navTileData, index - 1 - dims.x, navNodeIndex)) {
                                        coarseNavGraph.harvestablesLookup.setNodeHarvestable(navNodeIndex, hit->second, hit->first);
                                    }
                                }
                                if (tx < dims.x - 1) { // SOUTH_EAST
                                    if (tryGetNavNodeIndexForTile(navTileData, index + 1 - dims.x, navNodeIndex)) {
                                        coarseNavGraph.harvestablesLookup.setNodeHarvestable(navNodeIndex, hit->second, hit->first);
                                    }
                                }
                            }
                            if (tx > 2) { // West
                                if (tryGetNavNodeIndexForTile(navTileData, index - 2, navNodeIndex)) {
                                    coarseNavGraph.harvestablesLookup.setNodeHarvestable(navNodeIndex, hit->second, hit->first);
                                }
                            }
                            if (tx < dims.x - 2) { // EAST
                                if (tryGetNavNodeIndexForTile(navTileData, index + 2, navNodeIndex)) {
                                    coarseNavGraph.harvestablesLookup.setNodeHarvestable(navNodeIndex, hit->second, hit->first);
                                }
                            }
                            if (ty < dims.y - 1) {
                                if (tx > 0) { // NORTH_WEST
                                    if (tryGetNavNodeIndexForTile(navTileData, index - 1 + dims.x, navNodeIndex)) {
                                        coarseNavGraph.harvestablesLookup.setNodeHarvestable(navNodeIndex, hit->second, hit->first);
                                    }
                                }
                                if (tx < dims.x - 1) { // NORTH_EAST
                                    if (tryGetNavNodeIndexForTile(navTileData, index + 1 + dims.x, navNodeIndex)) {
                                        coarseNavGraph.harvestablesLookup.setNodeHarvestable(navNodeIndex, hit->second, hit->first);
                                    }
                                }

                                if (ty < dims.y - 2) { // NORTH
                                    if (tryGetNavNodeIndexForTile(navTileData, index + dims.x + dims.x, navNodeIndex)) {
                                        coarseNavGraph.harvestablesLookup.setNodeHarvestable(navNodeIndex, hit->second, hit->first);
                                    }
                                }
                            }
                        }
                        else {
                            // 4 closest neighbors
                            // _____
                            // __O__
                            // _OxO_
                            // __O__
                            // _____
                            DisjointSetNode navNodeIndex;
                            if (ty > 0) { // SOUTH
                                if (tryGetNavNodeIndexForTile(navTileData, index - dims.x, navNodeIndex)) {
                                    coarseNavGraph.harvestablesLookup.setNodeHarvestable(navNodeIndex, hit->second, hit->first);
                                }
                            }
                            if (tx > 0) { // WEST
                                if (tryGetNavNodeIndexForTile(navTileData, index - 1, navNodeIndex)) {
                                    coarseNavGraph.harvestablesLookup.setNodeHarvestable(navNodeIndex, hit->second, hit->first);
                                }
                            }
                            if (tx < dims.x - 1) { // EAST
                                if (tryGetNavNodeIndexForTile(navTileData, index + 1, navNodeIndex)) {
                                    coarseNavGraph.harvestablesLookup.setNodeHarvestable(navNodeIndex, hit->second, hit->first);
                                }
                            }
                            if (ty < dims.y - 1) { // NORTH
                                if (tryGetNavNodeIndexForTile(navTileData, index + dims.x, navNodeIndex)) {
                                    coarseNavGraph.harvestablesLookup.setNodeHarvestable(navNodeIndex, hit->second, hit->first);
                                }
                            }
                        }
                    }
                    continue;
                }

                DisjointSetNode navNodeIndex;
                if (!tryGetNavNodeIndexForTile(navTileData, index, navNodeIndex)) {
                    continue;
                }
              
                //const SubchunkIndex subchunkIndex = navTileData.subchunkIndices[djNodeIndex];
                const SubchunkIndex subchunkIndex = tileContainer.getSubchunkIndexFromTileIndex(index);
                assert(subchunkIndex < tileData.harvestables.size());
                const HarvestableSubchunkRegistry& harvestableRegistry = tileData.harvestables[subchunkIndex];
                const auto& hit = harvestableRegistry.mHarvestablePositions.find(index);
                if (hit != harvestableRegistry.mHarvestablePositions.end()) {
                    coarseNavGraph.harvestablesLookup.setNodeHarvestable(navNodeIndex, hit->second, hit->first);
                    //DebugRenderer::drawWireQuadThreadSafe(tileContainer.getTileCenterWorldPosition(index), f32v2(0.5f), COLOR_RED, 10000 + count);
                }

                // TODO: Should fine nav data include resource info?
                edgeCount += (int)tryBuildCoarseEdge(navTileData, fineData, index, index - 1, index - dims.x, dims, tiles, ownedDTiles, navNodeIndex, tileEdgePointers, nodeEdges, Cartesian::SOUTH, ty == 0, tx > 0);
                edgeCount += (int)tryBuildCoarseEdge(navTileData, fineData, index, index - dims.x, index - 1, dims, tiles, ownedDTiles, navNodeIndex, tileEdgePointers, nodeEdges, Cartesian::WEST, tx == 0, ty > 0);
                edgeCount += (int)tryBuildCoarseEdge(navTileData, fineData, index, index - dims.x, index + 1, dims, tiles, ownedDTiles, navNodeIndex, tileEdgePointers, nodeEdges, Cartesian::EAST, tx == dims.x - 1, ty > 0);
                edgeCount += (int)tryBuildCoarseEdge(navTileData, fineData, index, index - 1, index + dims.x, dims, tiles, ownedDTiles, navNodeIndex, tileEdgePointers, nodeEdges, Cartesian::NORTH, ty == dims.y - 1, tx > 0);
            }
        }
    }
#pragma endregion

    //std::cout << " c " << timer.stop() << std::endl;
    // ================= Assign and copy edges =================
    if (edgeCount) {
        coarseNavGraph.numEdges = edgeCount;
        // All edges reside in a single memory blob
        // TODO: pool allocate
        coarseNavGraph.edges = std::unique_ptr<CoarseNavNodeEdge[]>(new CoarseNavNodeEdge[edgeCount]);
        ui32 edgesStart = 0;
        CoarseNavNodeEdge* edgePtr = coarseNavGraph.edges.get();
        for (size_t navNodeIndex = 0; navNodeIndex < nodeEdges.size(); ++navNodeIndex) {
            std::vector<CoarseNavNodeEdge>& edgesToCopy = nodeEdges[navNodeIndex];
            memcpy(edgePtr + edgesStart, edgesToCopy.data(), edgesToCopy.size() * sizeof(CoarseNavNodeEdge));
            CoarseNavNode& node = coarseNavGraph.nodes[navNodeIndex];
            node.edgesStart = edgesStart;
            node.edgeCount = edgesToCopy.size();
            edgesStart += edgesToCopy.size();
        }
    }
    else {
        coarseNavGraph.edges = nullptr;
        coarseNavGraph.numEdges = 0;
    }

    // Allocate per tile data
    taskData.navGraph.tileCoarseNavIndices = std::unique_ptr<CoarseNavNodeIndex[]>(new CoarseNavNodeIndex[fineNavData.size()]);

    // Set nav node indices
    assert(navTileData.tileDjNodeIDs.size() == fineNavData.size());
    for (size_t i = 0; i < navTileData.tileDjNodeIDs.size(); ++i) {
        const ui16 nodeId = navTileData.tileDjNodeIDs[i];
        if (nodeId == INVALID_DJ_NODE_ID) {
            taskData.navGraph.tileCoarseNavIndices[i] = INVALID_NAV_NODE_INDEX;
        }
        else {
            DisjointSetNode navIndex = navTileData.djNodes[nodeId];
            taskData.navGraph.tileCoarseNavIndices[i] = navIndex;
            assert(navIndex < taskData.navGraph.numNodes);
        }
    }

    // Convert external edge tile indices to chunk tile indices since it is used by terrain
    if (externalEdges) {
        for (auto& edge : *externalEdges) {
            // Get world position
            i32v2 worldPos = TileSpatialGrid::getTileXYOffset(edge.first, dims);
            worldPos += i32v2(tileContainer.getWorldPos());
            // Add cartesian offset since its an opposite tile
            worldPos += CARTESIAN_NORMALS_2D[e_cast(edge.second)];
            assert(worldPos.x >= 0 && worldPos.y >= 0);

            ChunkID chunkId = mWorld.getChunkGrid().getChunkIDFromWorldPos(worldPos);

            // Chunk relative
            worldPos %= CHUNK_WIDTH;
            // Convert to Chunk tileindex
            // Invert cartesian since its an opposite tile
            taskData.externalEdges[chunkId].emplace_back(std::make_pair(worldPos.y * CHUNK_WIDTH + worldPos.x, CARTESIAN_OPPOSITES[e_cast(edge.second)]));
        }
    }

    mFinishedNavGraphBuildTasks.enqueue(std::move(taskData));
}

void NavWorld::tryBeginNavmeshTaskForContainer(const TileContainer* container) {
    // The decref will happen after the build
    if (container->mIsGeneratingNavmesh) {
        tryAddContainerToPendingDirtyContainersList(container);
        // If we are generating, we already have a ref, so delete the one we added
        // when dirtying this container
        container->decRef();
    }
    else {
        if (container->isTerrain()) {
            const ChunkID chunkId = container->getOwnerChunk()->getChunkID();
            if (mChunkPendingBuildingNavmeshCounts[chunkId] == 0) {

                // Build external edges if needed
                // I couldn't' get unique_ptr to work with the lambda
                TerrainExternalEdges* externalEdgesPtr = nullptr;
                ChunkBuildingEdgeList& edgeList = mChunkBuildingEdges[chunkId];
                if (edgeList.size()) {
                    externalEdgesPtr = new TerrainExternalEdges();
                    for (ChunkBuildingEdge& edge : edgeList) {
                        externalEdgesPtr->setExternal(edge.chunkTileIndex, edge.cartesian);
                    }
                }

                // Run task
                container->mIsGeneratingNavmesh = true;
                Services::Threadpool::ref().addTask([this, container, externalEdgesPtr]() {
                    buildNavGraphForContainer(*container, externalEdgesPtr);
                    if (externalEdgesPtr) {
                        delete externalEdgesPtr;
                    }
                });
            }
            else {
                if (!tryAddContainerToPendingDirtyContainersList(container)) {
                    // We decref here since we have an additional incref that is no longer needed
                    // from when we first marked as dirty
                    container->decRef();
                }
            }
        }
        else {
            // Increment dirty counts
            const boost::container::flat_set<ChunkID> chunkDependencies = getChunkDependenciesForContainer(mWorld.getChunkGrid(), container->getWorldPos(), container->getDims());
            for (ChunkID id : chunkDependencies) {
                ++mChunkPendingBuildingNavmeshCounts[id];
            }
            container->mIsGeneratingNavmesh = true;
            Services::Threadpool::ref().addTask([this, container]() {
                buildNavGraphForContainer(*container, nullptr);
            });
        }
    }
}

bool NavWorld::tryAddContainerToPendingDirtyContainersList(const TileContainer* container) {
    auto&& it = mPendingDirtyTileContainers.find(container);
    if (it == mPendingDirtyTileContainers.end()) {
        mPendingDirtyTileContainers.insert(container);
        return true;
    }
    return false;
}

void NavWorld::finishNavGraphBuildTask(NavGraphBuildTaskData& taskData) {
    ASSERT_NAV_THREAD();

    const TileContainerID containerId = taskData.container->getId();
    const bool isTerrain = taskData.container->isTerrain();
    //LOG_DEBUG("NAV FINISHED {}", containerId);
    // Store nav data
    const auto& navGraphIt = mNavGraphs.find(containerId);
    bool isNewContainer = (navGraphIt == mNavGraphs.end());
    const TileSpatialGrid& spatialGrid = taskData.container->getTileSpatialGrid();
    if (isNewContainer) {
        mNavGraphs.insert(
            std::make_pair(
                containerId,
                ContainerNavData(std::move(taskData.navGraph), std::move(taskData.fineNavData), spatialGrid.getWorldPos(), spatialGrid.getDims(), spatialGrid.getFloorHeight(), taskData.container->getId())
            ));
    }
    else {
        navGraphIt->second = ContainerNavData{ std::move(taskData.navGraph), std::move(taskData.fineNavData), spatialGrid.getWorldPos(), spatialGrid.getDims(), spatialGrid.getFloorHeight(), taskData.container->getId() };
    }

    // Store in spatial lookup
    if (isNewContainer) {
        if (isTerrain) {
            i32v3 worldPos = spatialGrid.getWorldPos();
            ChunkID chunkID = mWorld.getChunkGrid().getChunkIDFromWorldPos(i32v2(worldPos.x, worldPos.y));
            mTerrainTileContainers[chunkID] = taskData.container->getId();
        }
        else {
            const i32v2 worldPos2D = spatialGrid.getWorldPos();
            const i32v2 dims2D = spatialGrid.getDims();
            NavBBox newBox(NavBoxPoint(worldPos2D.x, worldPos2D.y), NavBoxPoint(worldPos2D.x + dims2D.x, worldPos2D.y + dims2D.y));
            mSpatialLookup.insert(ContainerNavRegion{ newBox, taskData.container->getId() });
        }
    }
    else {
        // TODO: Implement container resizing
    }

    if (!isTerrain) {
        // Tell chunks about our external edges
        const ChunkBuildingExternalEdgeListOutput& externalEdges = taskData.externalEdges;

        // Append external edges to the chunk
        for (auto& [chunkId, output] : externalEdges) {
            ChunkBuildingEdgeList& list = mChunkBuildingEdges[chunkId];
            list.reserve(list.size() + output.size());
            for (i32 i = 0; i < output.size(); ++i) {
                assert(output[i].first <= UINT16_MAX);
                list.emplace_back(ChunkBuildingEdge{ containerId, (ui16)output[i].first, output[i].second });
            }
        }
    }
    // Mark done
    taskData.container->setDidInitNav();
    taskData.container->mIsGeneratingNavmesh = false;

    bool didRecreate = false;
    auto&& it = mPendingDirtyTileContainers.find(taskData.container);
    if (it != mPendingDirtyTileContainers.end()) {
        if (isTerrain) {
            const ChunkID chunkId = taskData.container->getOwnerChunk()->getChunkID();
            // Only renavmesh chunk if we aren't pending buildings on our chunk
            if (mChunkPendingBuildingNavmeshCounts[chunkId] == 0) {
                mPendingDirtyTileContainers.erase(it);
                didRecreate = true;
                tryBeginNavmeshTaskForContainer(taskData.container);
            }
        }
        else {
            // Buildings always renavmesh
            mPendingDirtyTileContainers.erase(it);
            didRecreate = true;
            tryBeginNavmeshTaskForContainer(taskData.container);
        }
    }

    // Notify chunk deps that this building finished
    if (!isTerrain) {
        const boost::container::flat_set<ChunkID> chunkDependencies = getChunkDependenciesForContainer(mWorld.getChunkGrid(), taskData.container->getWorldPos(), taskData.container->getDims());
        for (ChunkID id : chunkDependencies) {
            assert(mChunkPendingBuildingNavmeshCounts[id] > 0);
            // If this chunk no longer has dependencies and we aren't about to remesh and add dependency back, renavmesh it
            if (--mChunkPendingBuildingNavmeshCounts[id] == 0 && !didRecreate) {
                const Chunk& chunk = mWorld.getChunkGrid().getChunk(id);
                auto&& pit = mPendingDirtyTileContainers.find(chunk.getTileContainer());
                if (pit != mPendingDirtyTileContainers.end()) {
                    mPendingDirtyTileContainers.erase(pit);
                    // Only navmesh the chunk if it is in pending, as otherwise we can't
                    // guarentee that it is even valid
                    tryBeginNavmeshTaskForContainer(chunk.getTileContainer());
                }
            }
        }
    }

    // Release resources if needed
    if (!didRecreate) {
        taskData.container->decRef();
    }
}

void NavWorld::initEventHandlers() {
    TileContainerRepository& tileContainerRepository = mWorld.getTileContainerRepository();
    tileContainerRepository.registerTileContainerListeners(mTileContainerEventListeners);
    tileContainerRepository.addEditTilesListener(mTileContainerEventListeners, [this](const TileContainerEvent& containerEvent) {

        constexpr ui8 EDIT_TYPES_MASK = 0xffui8;
        static_assert(e_cast(TileContainerEditEventType::TYPES) == 5, "Update handler");

        ASSERT_GAME_THREAD();

        const TileContainerEditEvent& editEvent = std::get<TileContainerEditEvent>(containerEvent.varEvent);

        if (e_cast(editEvent.type) & EDIT_TYPES_MASK) {
            markContainerNavDirty(containerEvent.container);
        }
    });

    tileContainerRepository.addDestroyListener(mTileContainerEventListeners, [this](const TileContainerEvent& containerEvent) {
        ASSERT_GAME_THREAD();
        const TileContainer& container = *containerEvent.container;
        mContainersToDestroy.gameThreadDirtyObject(TileContainerToDestroy{ container.getWorldPos(), container.getDims(), container.getId(), container.isTerrain() });
    });
}

bool NavWorld::trySetFineNavEdgeCartesian(TileIndex tileIndex, TileIndex adjacentIndex, Cartesian8 cartesian8, bool isInner, i32v3 containerDims, const std::vector<Tile>& tiles, const BitArray& ownedDTiles, const f32 groundZPosition, const f32 floorHeight, TileFineNavData& tileFineNavData, int prevZ, ChunkBuildingEdgeListOutput* externalEdges) {

    const Cartesian cartesian = CARTESIAN8_TO_CARTESIAN[e_cast(cartesian8)];
    assert(cartesian != Cartesian::NONE);
    const int floorStride = containerDims.x * containerDims.y;
    const ui32v2 dimsDTiles = ui32v2(containerDims.x >> 1, containerDims.y >> 1);
    assert(cartesian != Cartesian::NONE); // This must be a 4 cartesian
    if (isInner && isInteriorTile(ownedDTiles, adjacentIndex, dimsDTiles, tiles)) {
        // Interior edge
        const Tile* adjacent = &tiles[adjacentIndex];
        // Empty tiles, we go down a floor
        // TODO: Ground check only?
        if (adjacent->isEmpty()) {
            if (adjacentIndex >= floorStride) {
                adjacentIndex = adjacentIndex - floorStride;
                if (isInteriorTile(ownedDTiles, adjacentIndex, dimsDTiles, tiles)) {
                    adjacent = &tiles[adjacentIndex];
                }
                else {
                    // Exterior edge
                    tileFineNavData.setCanAccessDirection(cartesian8, true);
                    tileFineNavData.setEdgeType(cartesian, TileFineNavEdgeType::EXTERIOR);
                    if (externalEdges) {
                        externalEdges->emplace_back(std::make_pair(tileIndex, cartesian));
                    }
                    return true;
                }
            }
        }
        if (canEnterTileInDirection(*adjacent, cartesian) &&
            abs(((adjacentIndex / floorStride) * floorHeight + adjacent->getGroundZOffset()) - groundZPosition) <= FINE_NAV_HEIGHT_THRESHOLD) {
            tileFineNavData.setCanAccessDirection(cartesian8, true);
            const int adjZ = adjacentIndex / floorStride;
            if (adjZ < prevZ) {
                tileFineNavData.setEdgeType(cartesian, TileFineNavEdgeType::DOWN);
            }
            else if (adjZ > prevZ) {
                tileFineNavData.setEdgeType(cartesian, TileFineNavEdgeType::UP);
            }
            return true;
        }
    }
    else {
        // Exterior edge
        tileFineNavData.setCanAccessDirection(cartesian8, true);
        tileFineNavData.setEdgeType(cartesian, TileFineNavEdgeType::EXTERIOR);
        if (externalEdges) {
            externalEdges->emplace_back(std::make_pair(tileIndex, cartesian));
        }
        return true;
    }
    return false;
}

bool NavWorld::trySetFineNavEdgeCartesianDiagonal(TileIndex adjacentIndex, Cartesian8 cartesian8, bool isInner, i32v3 containerDims, const std::vector<Tile>& tiles, const BitArray& ownedDTiles, const f32 groundZPosition, TileFineNavData& fineNavData) {
    const ui32v2 dimsDTiles = ui32v2(containerDims.x >> 1, containerDims.y >> 1);
    if (isInner && isInteriorTile(ownedDTiles, adjacentIndex, dimsDTiles, tiles)) {
        // Interior edge
        const Tile& adjacent = tiles[adjacentIndex];
        if (canEnterTileInDirectionDiagonal(adjacent, cartesian8) &&
            abs(adjacent.getGroundZOffset() - groundZPosition) <= FINE_NAV_HEIGHT_THRESHOLD) {
            fineNavData.setCanAccessDirection(cartesian8, true);
            // TODO: Handle stairs
            //fineNavData.setEdgeType(Cartesian::SOUTH, TileFineNavEdgeType::UP);
            return true;
        }
    }
    else {
        // TODO: Allow diagonal exterior edges?
        //fineNavData.setCanAccessDirection(cartesian8, false);
       // fineNavData.setEdgeType(cartesian8, TileFineNavEdgeType::EXTERIOR);
        // return true;
    }
    return false;
}

bool NavWorld::isInteriorTile(const BitArray& ownedDTiles, TileIndex index2d, ui32v2 containerDimsDTiles, const std::vector<Tile>& tiles) const {
    if (!TileContainer::isTileOwned(ownedDTiles, index2d, containerDimsDTiles)) {
        return false;
    }
    return !tiles[index2d].isBuildingExterior();
}

bool NavWorld::tryBuildCoarseEdge(NavGraphTileDataToCopy& navTileData, const TileFineNavData& fineNavData, const TileIndex index, const TileIndex prevIndex, TileIndex outerIndex, i32v3 containerDims, const std::vector<Tile>& tiles, const BitArray& ownedDTiles, const ui16 navNodeIndex, std::vector<CoarseTileEdgePointer>& tileEdgePointers, std::vector<std::vector<CoarseNavNodeEdge>>& nodeEdges, const Cartesian dir, bool isBorder, bool canExtendPrevEdge)
{
    // We have an edge only if there is no wall
     // South edge
    bool needNewEdge = true;
    TileCoarseNavEdgeType coarseEdgeType = TileCoarseNavEdgeType::NONE;
    ui16 adjacentNodeIndex = INVALID_NAV_NODE_INDEX; // Signifies external edge
    const ui32v2 dimsDTiles = ui32v2(containerDims.x >> 1, containerDims.y >> 1);
    if (fineNavData.canAccessDirection(CARTESIAN_TO_CARTESIAN8[e_cast(dir)])) {
        if (isBorder || !TileContainer::isTileOwned(ownedDTiles, outerIndex, dimsDTiles) || fineNavData.getEdgeType(dir) == TileFineNavEdgeType::EXTERIOR) {
            coarseEdgeType = TileCoarseNavEdgeType::EXTERIOR;
            // External edge
            if (canExtendPrevEdge) {
                const ui16 prevDjNodeIndex = navTileData.tileDjNodeIDs[prevIndex];
                if (prevDjNodeIndex != INVALID_NAV_NODE_INDEX && navTileData.djNodes[prevDjNodeIndex] == navNodeIndex) {
                    CoarseTileEdgePointer& prevEdgePointer = tileEdgePointers[prevIndex];
                    std::vector<CoarseNavNodeEdge>& edges = nodeEdges[navNodeIndex];
                    ui32 prevEdgeID = prevEdgePointer.edges[e_cast(dir)];
                    if (prevEdgeID < edges.size()) {
                        CoarseNavNodeEdge& prevEdge = edges[prevEdgeID];
                        tileEdgePointers[index].edges[e_cast(dir)] = prevEdgePointer.edges[e_cast(dir)];
                        ++prevEdge.edgeLength;
                        needNewEdge = false;
                    }
                }
            }
        }
        else {
            // Internal edge
            TileFineNavEdgeType edgeType = fineNavData.getEdgeType(dir);
            if (edgeType == TileFineNavEdgeType::DOWN) {
                outerIndex -= containerDims.x * containerDims.y;
                canExtendPrevEdge = false;
                coarseEdgeType = TileCoarseNavEdgeType::DOWN;
            }
            else if (edgeType == TileFineNavEdgeType::UP) {
                outerIndex += containerDims.x * containerDims.y;
                canExtendPrevEdge = false;
                coarseEdgeType = TileCoarseNavEdgeType::UP;
            }
            const Tile& outerTile = tiles[outerIndex];
            const ui16 outerDjNodeIndex = navTileData.tileDjNodeIDs[outerIndex];
            // No edge because outer has no nav
            if (outerDjNodeIndex == INVALID_DJ_NODE_ID) {
                return false;
            }
            const ui16 outerNavNodeIndex = navTileData.djNodes[outerDjNodeIndex];
            // No edge becase we are the same nav node
            if (outerNavNodeIndex == navNodeIndex) {
                return false;
            }
            if (canEnterTileInDirection(outerTile, dir)) {
                adjacentNodeIndex = outerNavNodeIndex;
                // If we can extend prev wall
                if (canExtendPrevEdge) {
                    const ui16 prevDjNodeIndex = navTileData.tileDjNodeIDs[prevIndex];
                    if (prevDjNodeIndex != INVALID_DJ_NODE_ID && navTileData.djNodes[prevDjNodeIndex] == navNodeIndex) {
                        CoarseTileEdgePointer& prevEdgePointer = tileEdgePointers[prevIndex];
                        std::vector<CoarseNavNodeEdge>& edges = nodeEdges[navNodeIndex];
                        ui32 prevEdgeID = prevEdgePointer.edges[e_cast(dir)];
                        if (prevEdgeID < edges.size()) {
                            CoarseNavNodeEdge& prevEdge = edges[prevEdgeID];
                            // Extend only if we are connecting the same DJ nodes
                            if (prevEdge.adjacentNodeIndex == adjacentNodeIndex) {
                                tileEdgePointers[index].edges[e_cast(dir)] = prevEdgePointer.edges[e_cast(dir)];
                                ++prevEdge.edgeLength;
                                needNewEdge = false;
                            }
                        }
                    }
                }
            }
            else {
                // We cant enter south tile, so no new edge and no extend
                needNewEdge = false;
            }
        }
    }
    else {
        // We cant enter south tile due to wall, so no new edge and no extend
        needNewEdge = false;
    }

    if (needNewEdge) {
        // Make new edge
        CoarseTileEdgePointer& edgePointer = tileEdgePointers[index];
        // Edge list for this node
        std::vector<CoarseNavNodeEdge>& edges = nodeEdges[navNodeIndex];
        edgePointer.edges[e_cast(dir)] = edges.size();
        CoarseNavNodeEdge& newEdge = edges.emplace_back();
        newEdge.startPos = index;
        newEdge.edgeLength = 1;
        newEdge.dir = dir;
        newEdge.adjacentNodeIndex = adjacentNodeIndex;
        newEdge.edgeType = coarseEdgeType;
        return true;
    }
    return false;
}

void NavWorld::debugDrawCoarseNavGraphForContainer(const TileContainer& tileContainer, ui32 lifetime, int debugId /*= 0*/) const
{
    const color4 color1(0.0f, 1.0f, 1.0f, 0.75f);
    const color4 color2(1.0f, 0.0f, 0.0f, 0.75f);
    const color4 color3(1.0f, 1.0f, 1.0f, 0.75f);
    const color4 color4(1.0f, 0.0f, 1.0f, 0.75f);
    const IHeightmapGrid& heightGrid = mWorld.getHeightmapGrid();
    const TileContainerID containerId = tileContainer.getId();
    const bool isTerrain = tileContainer.isTerrain();
    auto&& it = mNavGraphs.find(containerId);
    if (it == mNavGraphs.end()) {
        std::cout << "Failed to find navgraph for container " << containerId << std::endl;
        return;
    }
    const CoarseNavGraph& graph = it->second.coarseNavGraph;
    // Draw edges and connections
    for (ui32 nodeIndex = 0; nodeIndex < graph.numNodes; ++nodeIndex) {
        const CoarseNavNode& node = graph.nodes[nodeIndex];
        i32v3 containerPos = tileContainer.getTileSpatialGrid().getWorldPos();
        const ui32 edgeCount = node.edgeCount;
        for (ui32 i = 0; i < edgeCount; ++i) {
            const CoarseNavNodeEdge& edge = graph.edges[node.edgesStart + i];
            const Tile& tile = tileContainer.getTileAt(edge.startPos); // TODO: RACE CONDITION
            i32v3 startOffset = tileContainer.getTileSpatialGrid().getTileXYZOffsetWithZScale(edge.startPos);
            if (isTerrain) {
                startOffset.z = heightGrid.computeHeightAtPoint<true>(f32v2(containerPos + startOffset));
            }
            i32v3 worldPos = containerPos + startOffset;
            // TODO: This is not thread safe!
            worldPos.z += tile.getGroundZOffset();

            if (edge.dir == Cartesian::EAST) worldPos.x += 1.0f;
            else if (edge.dir == Cartesian::NORTH) worldPos.y += 1.0f;
            const f32v2 offset = f32v2(CARTESIAN_TANGENTS_ABS_2D[e_cast(edge.dir)]) * (f32)(edge.edgeLength/* + 1.0f*/);
            f32v3 pointA = f32v3(worldPos) + f32v3(0.00f, 0.00f, 0.00f);
            f32v3 pointB = f32v3(worldPos) + f32v3(offset.x, offset.y, 0.0f);
            if (isTerrain) {
                pointA.z = heightGrid.computeHeightAtPoint<true>(f32v2(pointA));
                pointB.z = heightGrid.computeHeightAtPoint<true>(f32v2(pointB));
            }
            if (edge.isExternalEdge()) {
                DebugRenderer::drawLineBetweenPoints(pointA, pointB, color2, lifetime, debugId);
            }
            else {
                DebugRenderer::drawLineBetweenPoints(pointA, pointB, color1, lifetime, debugId);
            }
            f32v3 midpoint(worldPos.x + offset.x * 0.5f, worldPos.y + offset.y * 0.5f, (pointA.z + pointB.z) * 0.5f);
            if (isTerrain) {
                HeightmapPatchID patchId = heightGrid.getSpatialGrid2D().getIDAtWorldPos(tileContainer.getWorldPos());
                midpoint.z = heightGrid.computeHeightAtPoint<true>(f32v2(midpoint));
            }
            f32v3 third(midpoint.x + CARTESIAN_NORMALS_2D[e_cast(edge.dir)].x, midpoint.y + CARTESIAN_NORMALS_2D[e_cast(edge.dir)].y, midpoint.z);
            // TODO: Combine above
            DebugRenderer::drawLineBetweenPoints(midpoint, third, color3, lifetime, debugId);

            for (ui32 j = i + 1; j < edgeCount; ++j) {
                const CoarseNavNodeEdge& edge2 = graph.edges[node.edgesStart + j];
                i32v3 startOffset2 = tileContainer.getTileSpatialGrid().getTileXYZOffsetWithZScale(edge2.startPos);
                if (isTerrain) {
                    HeightmapPatchID patchId = heightGrid.getSpatialGrid2D().getIDAtWorldPos(tileContainer.getWorldPos());
                    startOffset2.z = heightGrid.computeHeightAtPoint<true>(f32v2(containerPos + startOffset2));
                }
                i32v3 worldPos2 = containerPos + startOffset2;
                if (edge2.dir == Cartesian::EAST) worldPos2.x += 1.0f;
                else if (edge2.dir == Cartesian::NORTH) worldPos2.y += 1.0f;
                const f32v2 offset2 = f32v2(CARTESIAN_TANGENTS_ABS_2D[e_cast(edge2.dir)]) * (f32)(edge2.edgeLength/* + 1.0f*/);
                f32v3 midpoint2(worldPos2.x + offset2.x * 0.5f, worldPos2.y + offset2.y * 0.5f, worldPos2.z);
                // TODO: Not thread safe!
                midpoint2.z += tileContainer.getTileAt(edge2.startPos).getGroundZOffset();
                if (isTerrain) {
                    HeightmapPatchID patchId(heightGrid.getSpatialGrid2D().getIDAtWorldPos(tileContainer.getWorldPos()));
                    midpoint2.z = heightGrid.computeHeightAtPoint<true>(f32v2(midpoint2));
                }
                DebugRenderer::drawLineBetweenPoints(midpoint, midpoint2, color4, lifetime, debugId);
            }
        }
    }
}

const f32v3 FINE_EDGE_OFFSETS[8] = {
    f32v3(0.0f, 0.0f, 0.0f), //SOUTH_WEST
    f32v3(0.5f, 0.0f, 0.0f), //SOUTH 
    f32v3(1.0f, 0.0f, 0.0f), //SOUTH_EAST
    f32v3(0.0f, 0.5f, 0.0f), //WEST
    f32v3(1.0f, 0.5f, 0.0f), //EAST
    f32v3(0.0f, 1.0f, 0.0f), //NORTH_WEST
    f32v3(0.5f, 1.0f, 0.0f), //NORTH
    f32v3(1.0f, 1.0f, 0.0f), //NORTH_EAST
};

constexpr ui32 COARSE_NAV_COLOR_COUNT = 9;
constexpr f32 COARSE_NAV_COLOR_ALPHA = 0.25f;
color4 COARSE_NAV_COLORS[COARSE_NAV_COLOR_COUNT] = {
    color4(1.0f, 1.0f, 1.0f, 0.35f),
    color4(1.0f, 0.0f, 0.0f, COARSE_NAV_COLOR_ALPHA),
    color4(1.0f, 1.0f, 0.0f, COARSE_NAV_COLOR_ALPHA),
    color4(1.0f, 0.0f, 1.0f, COARSE_NAV_COLOR_ALPHA),
    color4(0.0f, 1.0f, 1.0f, COARSE_NAV_COLOR_ALPHA),
    color4(0.0f, 1.0f, 0.0f, COARSE_NAV_COLOR_ALPHA),
    color4(0.0f, 0.0f, 1.0f, COARSE_NAV_COLOR_ALPHA),
    color4(0.4f, 0.1f, 0.7f, COARSE_NAV_COLOR_ALPHA),
    color4(0.1f, 0.6f, 0.4f, COARSE_NAV_COLOR_ALPHA),
};

void NavWorld::debugDrawFineNavGraphForContainer(const TileContainer& tileContainer, ui32 lifetime, int debugId /*= 0*/) const
{
    // TODO: Mutex lock? This is a race condition
    auto&& it = mNavGraphs.find(tileContainer.getId());
    if (it == mNavGraphs.end()) {
        return;
    }

    constexpr f32 EDGE_SIZE = 0.2f;
    ui32 tileIndex = 0;
    const auto& coarseNavData = it->second.coarseNavGraph;
    const auto& fineNavData = it->second.fineNavGraph;
    const color4 interiorColor = color4(0.0f, 1.0f, 0.0f, 0.7f);
    const color4 downColor = color4(0.0f, 0.0f, 1.0f, 0.7f);
    const color4 upColor = color4(0.0f, 1.0f, 1.0f, 0.7f);
    const color4 exteriorColor = color4(1.0f, 0.0f, 0.0f, 0.7f);
    const color4 whiteColor(1.0f, 1.0f, 1.0f, 0.3f);

    // TODO: not sure how this happens yet
    if (!coarseNavData.tileCoarseNavIndices) {
        return;
    }

    for (ui32 tileIndex = 0; tileIndex < fineNavData.size(); ++tileIndex) {
        if (tileContainer.isTileOwned(tileIndex)) {
            f32v3 worldPos = f32v3(tileContainer.getTileSpatialGrid().getTileXYZOffsetWithZScale(tileIndex) + tileContainer.getTileSpatialGrid().getWorldPos());
            worldPos.z += tileContainer.getTileAt(tileIndex).getGroundZOffset();
            const f32v3 centerPos = worldPos + f32v3(0.5f, 0.5f, 0.0f);
            const TileFineNavData& navData = fineNavData[tileIndex];
            const CoarseNavNodeIndex navNodeIndex = coarseNavData.tileCoarseNavIndices[tileIndex];
            const ui32 colorIndex = (ui32)navNodeIndex % COARSE_NAV_COLOR_COUNT;
            for (int dir = 0; dir < 8; ++dir) {
                if (navData.canAccessDirection(Cartesian8(dir))) {
                    f32v3 edgePos = worldPos + FINE_EDGE_OFFSETS[dir];
                    f32v3 offsetPos = edgePos + glm::normalize(centerPos - edgePos) * EDGE_SIZE;
                    const Cartesian cart4 = CARTESIAN8_TO_CARTESIAN[dir];
                    TileFineNavEdgeType edgeType = navData.getEdgeType(cart4);
                    if (cart4 == Cartesian::NONE || edgeType != TileFineNavEdgeType::EXTERIOR) {
                        if (edgeType == TileFineNavEdgeType::DOWN) {
                            DebugRenderer::drawLineBetweenPoints(edgePos, offsetPos, downColor, lifetime, debugId);
                        }
                        else if (edgeType == TileFineNavEdgeType::UP) {
                            DebugRenderer::drawLineBetweenPoints(edgePos, offsetPos, upColor, lifetime, debugId);
                        }
                        else {
                            DebugRenderer::drawLineBetweenPoints(edgePos, offsetPos, interiorColor, lifetime, debugId);
                        }
                    }
                    else {
                        DebugRenderer::drawLineBetweenPoints(edgePos, offsetPos, exteriorColor, lifetime, debugId);
                    }
                }
            }
            // Edges
            f32v3 worldPos2 = worldPos + f32v3(1.0f, 0.0f, 0.0f);
            f32v3 worldPos3 = worldPos + f32v3(1.0f, 1.0f, 0.0f);
            f32v3 worldPos4 = worldPos + f32v3(0.0f, 1.0f, 0.0f);
            if (navNodeIndex != INVALID_NAV_NODE_INDEX) {
                DebugRenderer::drawFilledQuad(worldPos, f32v2(1.0f), COARSE_NAV_COLORS[colorIndex], lifetime, debugId);
            }
            DebugRenderer::drawLineBetweenPoints(worldPos, worldPos2, whiteColor, lifetime, debugId);
            DebugRenderer::drawLineBetweenPoints(worldPos2, worldPos3, whiteColor, lifetime, debugId);
            DebugRenderer::drawLineBetweenPoints(worldPos3, worldPos4, whiteColor, lifetime, debugId);
            DebugRenderer::drawLineBetweenPoints(worldPos4, worldPos, whiteColor, lifetime, debugId);
        }
    }
}

void NavWorld::debugDrawCoarseNavNode(const TileHandle& tileHandle, ui32 lifetime, int debugId /*= 0*/) const
{
    // TODO: Mutex lock? This is a race condition

    if (!tileHandle.isValid()) {
        return;
    }

    auto&& cit = mNavGraphs.find(tileHandle.container->getId());
    if (cit == mNavGraphs.end()) {
        return;
    }
    const auto& coarseNavData = cit->second.coarseNavGraph;
    const CoarseNavNodeIndex coarseIndex = coarseNavData.tileCoarseNavIndices[tileHandle.tileIndex];
    if (coarseIndex == INVALID_NAV_NODE_INDEX) {
        return;
    }

    const color4 color1(0.0f, 1.0f, 1.0f, 0.75f);
    const color4 color2(1.0f, 0.0f, 0.0f, 0.75f);
    const color4 color3(1.0f, 1.0f, 1.0f, 0.75f);
    const color4 color4(1.0f, 0.0f, 1.0f, 0.75f);
    const IHeightmapGrid& heightGrid = mWorld.getHeightmapGrid();
    const TileContainerID containerId = tileHandle.container->getId();
    const bool isTerrain = tileHandle.container->isTerrain();
    auto&& it = mNavGraphs.find(containerId);
    if (it == mNavGraphs.end()) {
        std::cout << "Failed to find navgraph for container " << containerId << std::endl;
        return;
    }
    const CoarseNavGraph& graph = it->second.coarseNavGraph;
    const CoarseNavNode& node = graph.nodes[coarseIndex];
    i32v3 containerPos = tileHandle.container->getTileSpatialGrid().getWorldPos();
    const ui32 edgeCount = node.edgeCount;
    for (ui32 i = 0; i < edgeCount; ++i) {
        const CoarseNavNodeEdge& edge = graph.edges[node.edgesStart + i];
        i32v3 startOffset = tileHandle.container->getTileSpatialGrid().getTileXYZOffsetWithZScale(edge.startPos);
        // TODO: Remove
        if (isTerrain) {
            startOffset.z = heightGrid.computeHeightAtPoint<true>(f32v2(containerPos + startOffset));
        }
        f32v3 worldPos = containerPos + startOffset;
        // TODO: Not thread safe!
        worldPos.z += tileHandle.container->getTileAt(edge.startPos).getGroundZOffset();
        if (edge.dir == Cartesian::EAST) worldPos.x += 1.0f;
        else if (edge.dir == Cartesian::NORTH) worldPos.y += 1.0f;
        const f32v2 offset = f32v2(CARTESIAN_TANGENTS_ABS_2D[e_cast(edge.dir)]) * (f32)(edge.edgeLength/* + 1.0f*/);
        f32v3 pointA = f32v3(worldPos) + f32v3(0.00f, 0.00f, 0.00f);
        f32v3 pointB = f32v3(worldPos) + f32v3(offset.x, offset.y, 0.0f);
        if (isTerrain) {
            pointA.z = heightGrid.computeHeightAtPoint<true>(f32v2(pointA));
            pointB.z = heightGrid.computeHeightAtPoint<true>(f32v2(pointB));
        }
        if (edge.isExternalEdge()) {
            DebugRenderer::drawLineBetweenPoints(pointA, pointB, color2, lifetime, debugId);
        }
        else {
            DebugRenderer::drawLineBetweenPoints(pointA, pointB, color1, lifetime, debugId);
        }
        f32v3 midpoint(worldPos.x + offset.x * 0.5f, worldPos.y + offset.y * 0.5f, (pointA.z + pointB.z) * 0.5f);
        if (isTerrain) {
            midpoint.z = heightGrid.computeHeightAtPoint<true>(f32v2(midpoint));
        }
        f32v3 third(midpoint.x + CARTESIAN_NORMALS_2D[e_cast(edge.dir)].x, midpoint.y + CARTESIAN_NORMALS_2D[e_cast(edge.dir)].y, midpoint.z);
        // TODO: Combine above
        DebugRenderer::drawLineBetweenPoints(midpoint, third, color3, lifetime, debugId);

        for (ui32 j = i + 1; j < edgeCount; ++j) {
            const CoarseNavNodeEdge& edge2 = graph.edges[node.edgesStart + j];
            i32v3 startOffset2 = tileHandle.container->getTileSpatialGrid().getTileXYZOffsetWithZScale(edge2.startPos);
            if (isTerrain) {
                startOffset2.z = heightGrid.computeHeightAtPoint<true>(f32v2(containerPos + startOffset2));
            }
            i32v3 worldPos2 = containerPos + startOffset2;
            if (edge2.dir == Cartesian::EAST) worldPos2.x += 1.0f;
            else if (edge2.dir == Cartesian::NORTH) worldPos2.y += 1.0f;
            const f32v2 offset2 = f32v2(CARTESIAN_TANGENTS_ABS_2D[e_cast(edge2.dir)]) * (f32)(edge2.edgeLength/* + 1.0f*/);
            f32v3 midpoint2(worldPos2.x + offset2.x * 0.5f, worldPos2.y + offset2.y * 0.5f, worldPos2.z);
            // TODO: Not thread safe!
            midpoint2.z += tileHandle.container->getTileAt(edge2.startPos).getGroundZOffset();
            if (isTerrain) {
                HeightmapPatchID patchId = heightGrid.getSpatialGrid2D().getIDAtWorldPos(tileHandle.container->getWorldPos());
                midpoint2.z = heightGrid.computeHeightAtPoint<true>(f32v2(midpoint2));
            }
            DebugRenderer::drawLineBetweenPoints(midpoint, midpoint2, color4, lifetime, debugId);
        }
    }
}

const CoarseNavGraph* NavWorld::tryGetCoarseNavGraph(TileContainerID containerId) const {
    ASSERT_NAV_THREAD();
    auto&& it = mNavGraphs.find(containerId);
    if (it == mNavGraphs.end()) return nullptr;
    return &it->second.coarseNavGraph;
}

const CoarseNavGraph& NavWorld::getCoarseNavGraph(TileContainerID containerId) const {
    ASSERT_NAV_THREAD();
    auto&& it = mNavGraphs.find(containerId);
    assert(it != mNavGraphs.end());
    return it->second.coarseNavGraph;
}


const CoarseNavNode* NavWorld::getCoarseNavNode(CoarseNavNodeIndexPair index) const {
    return getCoarseNavNode(index.tileContainerID, index.index);
}

const CoarseNavNode* NavWorld::getCoarseNavNode(TileContainerID containerId, ui16 navNodeIndex) const {
    ASSERT_NAV_THREAD();
    // TODO: what if invalid
    auto&& it = mNavGraphs.find(containerId);
    assert(it != mNavGraphs.end());
    const CoarseNavGraph& patch = it->second.coarseNavGraph;
    assert(navNodeIndex < patch.numNodes);
    return &patch.nodes[navNodeIndex];
}

TileFineNavData NavWorld::getFineNavData(TileContainerID containerId, TileIndex tileIndex) const {
    ASSERT_NAV_THREAD();
    // TODO: what if invalid
    auto&& it = mNavGraphs.find(containerId);
    assert(it != mNavGraphs.end());
    const std::vector<TileFineNavData>& fineNavGraph = it->second.fineNavGraph;
    assert(tileIndex < fineNavGraph.size());
    return fineNavGraph[tileIndex];
}

TileFineNavData NavWorld::getFineNavDataAndContainerDims(TileContainerID containerId, TileIndex tileIndex, OUT i32v3& containerDims, OUT i32& floorHeight) const {
    ASSERT_NAV_THREAD();
    // TODO: what if invalid
    auto&& it = mNavGraphs.find(containerId);
    assert(it != mNavGraphs.end());
    const std::vector<TileFineNavData>& fineNavGraph = it->second.fineNavGraph;
    assert(tileIndex < fineNavGraph.size());
    containerDims = it->second.containerDims;
    floorHeight = it->second.floorHeight;
    return fineNavGraph[tileIndex];
}

const ContainerNavData& NavWorld::getNavDataForContainer(TileContainerID containerId) const {
    ASSERT_NAV_THREAD();
    // TODO: what if invalid
    auto&& it = mNavGraphs.find(containerId);
    assert(it != mNavGraphs.end());
    return it->second;
}

LiteTileHandle NavWorld::getTileHandleAndNavDataAtWorldPos(i32v3 worldPos, OUT const ContainerNavData** outNavData) const {
    ASSERT_NAV_THREAD();
    // TODO: Stack memory?
    std::vector<ContainerNavRegion> overlappingContainers;
    overlappingContainers.reserve(4);
    // https://valelab4.ucsf.edu/svn/3rdpartypublic/boost-versions/boost_1_55_0/libs/geometry/doc/html/geometry/spatial_indexes/queries.html
    const size_t overlapCount = mSpatialLookup.query(boost::geometry::index::intersects(NavBoxPoint(worldPos.x, worldPos.y)), std::back_inserter(overlappingContainers));

    // Find the first structure whos tile is included in this point
    // Structures are AABBs
    // TODO: Iterator instead of back_inserter 
    // https://stackoverflow.com/questions/64179718/storing-or-accessing-objects-in-boost-r-tree
    for (auto&& containerRegion : overlappingContainers) {
        auto&& it = mNavGraphs.find(containerRegion.id);
        assert(it != mNavGraphs.end());
        const ContainerNavData& navData = it->second;
        i32v3 offset = worldPos - navData.worldPos;
        offset.x = glm::clamp(offset.x, 0, navData.containerDims.x);
        offset.y = glm::clamp(offset.y, 0, navData.containerDims.y);
        offset.z = glm::clamp(offset.z, 0, navData.containerDims.z * navData.floorHeight);
        offset.z /= navData.floorHeight;
        TileIndex tileIndex = TileSpatialGrid::getTileIndexFromXYZOffset(offset, navData.containerDims);
        if (navData.fineNavGraph[tileIndex].isOwned) {
            *outNavData = &navData;
            return LiteTileHandle(containerRegion.id, tileIndex);
        }
    }

    // If we find no container, return valid chunk container position at this point
    // WORLD ORIGIN MUST be 0
    const ui32 worldWidthChunks = mWorld.getChunkGrid().getWidthChunks();
    const i32v2 chunkOffset(worldPos.x / CHUNK_WIDTH, worldPos.y / CHUNK_WIDTH);
    const ChunkID chunkId = chunkOffset.y * worldWidthChunks + chunkOffset.x;
    TileContainerID containerId = mTerrainTileContainers[chunkId];
    if (containerId != INVALID_TILE_CONTAINER_ID) {
        auto&& it = mNavGraphs.find(containerId);
        assert(it != mNavGraphs.end());
        const ContainerNavData& navData = it->second;
        const i32v2 offset = i32v2(worldPos.x - navData.worldPos.x, worldPos.y - navData.worldPos.y);
        TileIndex tileIndex = TileSpatialGrid::getBaseTileIndexFromXYOffset(offset, navData.containerDims);
        assert(tileIndex < navData.containerDims.x* navData.containerDims.y* navData.containerDims.z);
        *outNavData = &navData;
        return LiteTileHandle(containerId, tileIndex);
    }
    outNavData = nullptr;
    return LiteTileHandle();
}

i32 NavWorld::getWidthChunks() const {
    return mWorld.getWidthChunks();
}

void NavWorld::markContainerNavDirty(TileContainer* container) {
    ASSERT_GAME_THREAD();
    assert(container);
    bool didAdd = false;
    if (container->isTerrain()) {
        didAdd = mDirtyChunkTileContainers.gameThreadTryDirtyObject(container);
    }
    else {
        const boost::container::flat_set<ChunkID> chunkDependencies = getChunkDependenciesForContainer(mWorld.getChunkGrid(), container->getWorldPos(), container->getDims()); // TODO: boost::container::flat_set?
        for (ChunkID chunkId : chunkDependencies) {
            const Chunk& chunk = mWorld.getChunkGrid().getChunk(chunkId);
            // Valid will get marked dirty as well, as they depend on our external edges
            if (chunk.getState() >= ChunkState::CAN_GENERATE_NAV) {
                if (mDirtyChunkTileContainers.gameThreadTryDirtyObject(chunk.getTileContainer())) {
                    chunk.getTileContainer()->incRef();
                }
            }
        }

        didAdd = mDirtyBuildingTileContainers.gameThreadTryDirtyObject(container);
    }
    // Make sure we don't get deallocated while we are in the dirty list
    // TODO: Technically this is race condition if the nav thread and worker thread manage to finish their entire cycle before we get here.. but
    // this should be statistically impossible
    if (didAdd) {
        container->incRef();
    }
}

bool NavWorld::navThreadTryReserveHarvestable(LiteTileHandle position) const {
    ASSERT_NAV_THREAD();
    auto&& it = mReservedHarvestables.find(position);
    if (it == mReservedHarvestables.end()) {
        mReservedHarvestables.insert(std::make_pair(position, Services::TimestepManager::ref().getCurrentTimeSec()));
        return true;
    }
    const f64 timeStampNow = Services::TimestepManager::ref().getCurrentTimeSec();
    const f64 lifetime = timeStampNow - it->second;
    if (lifetime >= RESERVE_DURATION_SEC) {
        it->second = timeStampNow;
        return true;
    }
    return false;
}
