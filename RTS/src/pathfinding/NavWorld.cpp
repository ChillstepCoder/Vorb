#include "stdafx.h"
#include "NavWorld.h"

#include "world/IWorld.h"
#include "world/Chunk.h"
#include "world/IHeightmapGrid.h"
#include "debugging/DebugRenderer.h"
#include "options/DebugOptions.h"

#include "city/Building.h"

#include "debugging/VisualLogger.h"

#include "resources/TileRepository.h"

NavWorld* sNavWorld = nullptr;

// 0 or 1 for rendering debug
#define DEBUG_RENDER_NAV_NODES 1

// Distance threshold where we can still navigate across blocks
constexpr f32 FINE_NAV_HEIGHT_THRESHOLD = 3.0f / 4.0f + 0.05f;

inline f32v3 helperGet3DPoint(const IHeightmapGrid& heightGrid, const f32v2& pos2d) {
    return f32v3(pos2d.x, pos2d.y, heightGrid.tryComputeHeightAtPoint(pos2d));
}

inline f32v3 helperGet3DPoint(const IHeightmapGrid& heightGrid, const HeightmapPatchID& patchId, const f32* heightData, const f32v2& pos2d) {
    if (!heightData)
    {
        return f32v3(pos2d.x, pos2d.y, 4.0f);
    }
    return f32v3(pos2d.x, pos2d.y, heightGrid.computeHeightAtPoint(patchId, heightData, pos2d));
}


NavWorld::NavWorld()
{
    assert(!sNavWorld);
    sNavWorld = this;
    // TODO: This is arbitrary
    mNavGraphs.reserve(100);
    for (int i = 0; i < WorldData::WORLD_SIZE_CHUNKS; ++i) {
        mTerrainTileContainers[i] = INVALID_TILE_CONTAINER_ID;
    }
    initEventHandlers();
}

NavWorld::~NavWorld()
{
    sNavWorld = nullptr;
}

void NavWorld::updateNavThread()
{
    assert(IS_NAV_THREAD());

    constexpr int MAX_BULK_DEQUE_TASK_DATA = 32;
    NavGraphBuildTaskData taskDataBulk[MAX_BULK_DEQUE_TASK_DATA];

    while (size_t count = mFinishedNavGraphBuildTasks.try_dequeue_bulk(taskDataBulk, MAX_BULK_DEQUE_TASK_DATA)) {
        for (size_t i = 0; i < count; ++i) {
            finishNavGraphBuildTask(taskDataBulk[i]);
        }
    }

    // Update dirty tile containers
    {
        std::set<TileContainer*> dirtyContainers;
        {
            std::lock_guard lock(mDirtyTileContainersMutex);
            if (mDirtyTileContainers.size()) {
                // Take ownership and release the lock
                dirtyContainers.swap(mDirtyTileContainers);
            }
        }
        for (auto&& container : dirtyContainers) {
            // The decref will happen after the build
            LOG_DEBUG("NAV BEGIN {}", container->getId());
            Services::Threadpool::ref().addTask([this, container](ThreadPoolWorkerData* workerData) {
                buildNavGraphForContainer(*container);
            }, nullptr);
        }
    }

    // Destroy tile containers
    TileContainerToDestroy containersToDestroy[32];
    if (size_t count = mContainersToDestroy.try_dequeue_bulk(containersToDestroy, 32)) {
        for (size_t i = 0; i < count; ++i) {
            TileContainerToDestroy containerData = containersToDestroy[i];

            // Remove external edge dependencies on terrain
            if (!containerData.isTerrain) {
                // Manually compute chunk dependencies since we dont have the data here
                std::set<LiteChunkID> chunkDependencies;
                i32v2 worldXY = i32v2(containerData.worldPos.x, containerData.worldPos.y);
                chunkDependencies.insert(ChunkID::fromWorldI32v2(worldXY).id);
                worldXY = i32v2(containerData.worldPos.x + containerData.dims.x, containerData.worldPos.y);
                chunkDependencies.insert(ChunkID::fromWorldI32v2(worldXY).id);
                worldXY = i32v2(containerData.worldPos.x, containerData.worldPos.y + containerData.dims.y);
                chunkDependencies.insert(ChunkID::fromWorldI32v2(worldXY).id);
                worldXY = i32v2(containerData.worldPos.x + containerData.dims.x, containerData.worldPos.y + containerData.dims.y);
                chunkDependencies.insert(ChunkID::fromWorldI32v2(worldXY).id);

                for (LiteChunkID id : chunkDependencies) {
                    assert(id < WorldData::WORLD_SIZE_CHUNKS);
                    TileContainerID terrainContainerId = mTerrainTileContainers[id];
                    const auto& navGraphIt = mNavGraphs.find(terrainContainerId);
                    assert(navGraphIt != mNavGraphs.end()); // We must enforce that chunks always finish their first nav before any buildings are navved
                    ContainerNavData& chunkNavData = navGraphIt->second;
                    chunkNavData.terrainDependentEdges->erase(containerData.id);

                    LOG_WARN("TODO: When building container destroyed, flag terrain dirty");
                    //assert(false); // This is a race condition as if chunk is null, incref crashes
                   // sWorld->getChunk(id).incRef();
                   // markChunkContainerNavDirty(id);
                }
            }

            mNavGraphs.erase(containerData.id);

            // Remove from spatial lookup
            const i32v2 worldPos2D(containerData.worldPos.x, containerData.worldPos.y);
            if (containerData.isTerrain) {
                i32v3 worldPos = containerData.worldPos;
                ChunkID chunkID = ChunkID::fromWorldI32v2(worldPos2D);
                mTerrainTileContainers[chunkID.id] = INVALID_TILE_CONTAINER_ID;
            }
            else {
                NavBBox newBox(NavBoxPoint(worldPos2D.x, worldPos2D.y), NavBoxPoint(worldPos2D.x + containerData.dims.x, worldPos2D.y + containerData.dims.y));
                mSpatialLookup.remove(ContainerNavRegion{ newBox, containerData.id });
            }
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

constexpr TileFlags FORCE_COARSE_NAV_FLAGS[4] = {
    TileFlags::FORCE_EXTERNAL_EDGE_SOUTH, // South
    TileFlags::FORCE_EXTERNAL_EDGE_WEST,  // West
    TileFlags::FORCE_EXTERNAL_EDGE_EAST,  // East
    TileFlags::FORCE_EXTERNAL_EDGE_NORTH, // North
};

// toDir = south means we enter from the north
bool canEnterTileInDirection(const Tile& tile, const TileWalls& walls, Cartesian toDir) {
    if (!tile.hasFlagsMaskAny(IMPASSABLE_TILE_FLAGS_MASK)) {
        const Cartesian fromDir = CARTESIAN_OPPOSITES[e_cast(toDir)];
        if (!tile.canNavInDirection(CARTESIAN_TO_CARTESIAN8[e_cast(fromDir)])) return false;
        return walls.walls[e_cast(fromDir)].canNavThrough();
    }
    return false;
}
bool canEnterTileInDirectionDiagonal(const Tile& tile, const TileWalls& walls, Cartesian8 toDir) {
    if (!tile.hasFlagsMaskAny(IMPASSABLE_TILE_FLAGS_MASK)) {
        const CartesianPair fromDirs = CARTESIAN_DIAGONAL_OPPOSITES[e_cast(toDir)];
        if (!tile.canNavInDirection(CARTESIAN_TO_CARTESIAN8[e_cast(fromDirs.first)]) || !tile.canNavInDirection(CARTESIAN_TO_CARTESIAN8[e_cast(fromDirs.second)])) return false;
        return (walls.walls[e_cast(fromDirs.first)].canNavThrough() && walls.walls[e_cast(fromDirs.second)].canNavThrough());
    }
    return false;
}

// ALGORITHM DESCRIPTION
// 1. Build disjoint sets, with a specific grid size for a maximum disjoint set node width
// 2. Create nav nodes from distinct disjoint set nodes
// 3. Create edges between disjoint set nodes and each other, or "External" which means it connects to the outside world
void NavWorld::buildNavGraphForContainer(TileContainer& tileContainer) {
    // Worker thread only
    assert(!IS_GAME_THREAD());
    PROFILE_FUNCTION();
    PreciseTimer timer;

    NavGraphBuildTaskData taskData;
    taskData.container = &tileContainer;
    NavGraphTileDataToCopy& navTileData = taskData.navTileData;
    CoarseNavGraph& coarseNavGraph = taskData.navGraph;
    std::vector<TileFineNavData>& fineNavData = taskData.fineNavData;
    const f32 floorHeight = tileContainer.getFloorHeight();
    const bool isTerrain = tileContainer.isTerrain();
    // We only care about external edges for non terrain
    ExternalEdgeList* externalEdges = nullptr;
    if (!isTerrain) {
        externalEdges = &taskData.externalEdges;
        externalEdges->reserve(6);
    }

    //ScopedTimer timer("Built nav graph");
    const i32v3& dims = tileContainer.getDims();

    /* Chunk* chunk = nullptr;
     if (isTerrain) {
         chunk = &sWorld->getChunk(ChunkID(f32v2(tileContainer.getWorldPos2D())));
     }*/

    ui32 totalDjSets = 0;

    const ui32 GRID_WIDTH = SUBCHUNK_WIDTH;
    // Reserve nodes
    navTileData.djNodes.reserve(SQ(GRID_WIDTH) * dims.z);

    // ================= Create disjoint set and fine nav data =================
    ContainerNavDataCopy tileData;
    tileContainer.copyDataWorkerThread(tileData);
    const std::vector<Tile>& tiles = tileData.mTiles;
    const std::vector<TileWalls>& tileWalls = tileData.mWalls;
    fineNavData.resize(tiles.size());
    const BitArray& ownedTiles = tileData.mOwnedTiles;

    navTileData.tileDjNodeIDs.resize(tiles.size(), INVALID_DJ_NODE_ID);

    TileIndex index = 0;
    for (int tz = 0; tz < dims.z; ++tz) {
        for (int ty = 0; ty < dims.y; ++ty) {
            const int gridYOffset = ty % GRID_WIDTH;
            for (int tx = 0; tx < dims.x; ++tx) {
                // Determine if we own this tile
                const TileIndex index = tileContainer.getTileIndexFromXYZOffset(tx, ty, tz);

                TileFineNavData& tileFineNavData = fineNavData[index];
                tileFineNavData.reset();

                if (!tileContainer.isTileOwned(index)) {
                    continue;
                }
                // Impassible or empty tiles are not part of navgraph
                const Tile& tile = tiles[index];
                if (tile.hasFlagsMaskAny(IMPASSABLE_TILE_FLAGS_MASK)) {
                    continue;
                }
                // Only terrain tiles can be empty
                if (!isTerrain && tile.isEmpty()) {
                    continue;
                }
                tileFineNavData.isOwned = true;

                tileFineNavData.zPositionOffsetFromFloor = tile.getGroundZOffset();
                const TileWalls& walls = tileWalls[index];
                const f32 groundZPosition = tileFineNavData.zPositionOffsetFromFloor + tz * floorHeight;
                bool assigned = false;

                // ================= Fine Nav Data =================
                // TODO: which tile do we use for path weight?
                TileID groundId = tile.getLayers()[TILE_LAYER_GROUND];
                if (groundId != TILE_ID_NONE) {
                    tileFineNavData.pathWeight = TileRepository::getTileData(groundId).pathWeight;
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
                        adjIndex = TileContainer::getTileIndexFromXYZOffset(i32v3(tx, ty - 1, z), dims);
                    } else {
                        adjIndex = index - dims.x;
                    }
                    if (!trySetFineNavEdgeCartesian(index, adjIndex, Cartesian8::SOUTH, ty > 0 && !tile.hasFlag(TileFlags::FORCE_EXTERNAL_EDGE_SOUTH), dims, tiles, tileWalls, ownedTiles, groundZPosition, floorHeight, tileFineNavData, tz, externalEdges)) {
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
                    if (z != tz && z > 0 && z < tileContainer.getDims().z) {
                        adjIndex = TileContainer::getTileIndexFromXYZOffset(i32v3(tx - 1, ty, z), dims);
                    }
                    else {
                        adjIndex = index - 1;
                    }
                    if (!trySetFineNavEdgeCartesian(index, adjIndex, Cartesian8::WEST, tx > 0 && !tile.hasFlag(TileFlags::FORCE_EXTERNAL_EDGE_WEST), dims, tiles, tileWalls, ownedTiles, groundZPosition, floorHeight, tileFineNavData, tz, externalEdges)) {
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
                        adjIndex = TileContainer::getTileIndexFromXYZOffset(i32v3(tx + 1, ty, z), dims);
                    }
                    else {
                        adjIndex = index + 1;
                    }
                    if (!trySetFineNavEdgeCartesian(index, adjIndex, Cartesian8::EAST, tx < dims.x - 1 && !tile.hasFlag(TileFlags::FORCE_EXTERNAL_EDGE_EAST), dims, tiles, tileWalls, ownedTiles, groundZPosition, floorHeight, tileFineNavData, tz, externalEdges)) {
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
                        adjIndex = TileContainer::getTileIndexFromXYZOffset(i32v3(tx, ty + 1, z), dims);
                    }
                    else {
                        adjIndex = index + dims.x;
                    }
                    if (!trySetFineNavEdgeCartesian(index, adjIndex, Cartesian8::NORTH, ty < dims.y - 1 && !tile.hasFlag(TileFlags::FORCE_EXTERNAL_EDGE_NORTH), dims, tiles, tileWalls, ownedTiles, groundZPosition, floorHeight, tileFineNavData, tz, externalEdges)) {
                        canGoNorthWest = canGoNorthEast = false;
                    }
                }
                else {
                    canGoNorthWest = canGoNorthEast = false;
                }

                // South West
                if (canGoSouthWest && tile.canNavInDirection(Cartesian8::SOUTH_WEST)) {
                    trySetFineNavEdgeCartesianDiagonal(index - dims.x - 1, Cartesian8::SOUTH_WEST, ty > 0 && tx > 0, dims, tiles, tileWalls, ownedTiles, groundZPosition, tileFineNavData);
                }
                // South East
                if (canGoSouthEast && tile.canNavInDirection(Cartesian8::SOUTH_EAST)) {
                    trySetFineNavEdgeCartesianDiagonal(index - dims.x + 1, Cartesian8::SOUTH_EAST, ty > 0 && tx < dims.x - 1, dims, tiles, tileWalls, ownedTiles, groundZPosition, tileFineNavData);
                }
                // North West
                if (canGoNorthWest && tile.canNavInDirection(Cartesian8::NORTH_WEST)) {
                    trySetFineNavEdgeCartesianDiagonal(index + dims.x - 1, Cartesian8::NORTH_WEST, ty < dims.y - 1 && tx > 0, dims, tiles, tileWalls, ownedTiles, groundZPosition, tileFineNavData);
                }
                // North East
                if (canGoNorthEast && tile.canNavInDirection(Cartesian8::NORTH_EAST)) {
                    trySetFineNavEdgeCartesianDiagonal(index + dims.x + 1, Cartesian8::NORTH_EAST, ty < dims.y - 1 && tx < dims.x - 1, dims, tiles, tileWalls, ownedTiles, groundZPosition, tileFineNavData);
                }

                // ================= Disjoint Set =================
                const int gridXOffset = tx % GRID_WIDTH;
                if (gridXOffset != 0) {
                    if (tileFineNavData.canAccessDirection(Cartesian8::WEST)) {
                        navTileData.tileDjNodeIDs[index] = navTileData.tileDjNodeIDs[index - 1];
                        assigned = true;
                    }
                }
                if (gridYOffset != 0) {
                    const Tile& bottom = tiles[index - dims.x];
                    // Check if we can cross between
                    if (tileFineNavData.canAccessDirection(Cartesian8::SOUTH)) {
                        if (assigned) {
                            // If we already assigned to left, merge the sets
                            ui16 prevID = navTileData.tileDjNodeIDs[index];
                            ui16 botID = navTileData.tileDjNodeIDs[index - dims.x];
                            // TODO: why do we need this check?
                            if (prevID != INVALID_DJ_NODE_ID && botID != INVALID_DJ_NODE_ID) {
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
                    navTileData.tileDjNodeIDs[index] = totalDjSets;
                    navTileData.djNodes.push_back(totalDjSets);
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
            nodes[navTileData.djNodes[i]].insert(i);
        }
        // Compress
        totalDjSets = nodes.size();
        ui32 i = 0;
        for (auto&& it : nodes) {
            for (auto&& it2 : it.second) {
                navTileData.djNodes[it2] = i;
            }
            ++i;
        }

        // Allocate the graph
        coarseNavGraph.numNodes = totalDjSets;
        coarseNavGraph.nodes = std::unique_ptr<CoarseNavNode[]>(new CoarseNavNode[totalDjSets]);
        // Tell the nav nodes who they belong to
        for (ui32 i = 0; i < totalDjSets; ++i) {
            coarseNavGraph.nodes[i].tileContainerID = tileContainer.getId();
        }
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

    //std::cout << " B " << timer.stop() << std::endl;
    for (int tz = 0; tz < dims.z; ++tz) {
        for (int ty = 0; ty < dims.y; ++ty) {
            for (int tx = 0; tx < dims.x; ++tx) {
                // Determine if we own this tile
                const TileIndex index = tileContainer.getTileIndexFromXYZOffset(tx, ty, tz);
                const Tile& tile = tileContainer.getTileAt(index);
                const ui16 djNodeIndex = navTileData.tileDjNodeIDs[index];
                if (djNodeIndex == INVALID_NAV_NODE_INDEX) {
                    continue;
                }
                const ui16 navNodeIndex = navTileData.djNodes[djNodeIndex];
                // This must be part of a nav node
                if (navNodeIndex == INVALID_NAV_NODE_INDEX) {
                    continue;
                }

                const TileFineNavData& fineData = fineNavData[index];
                edgeCount += (int)tryBuildCoarseEdge(navTileData, fineData, index, index - 1, index - dims.x, dims, tiles, tileWalls, ownedTiles, navNodeIndex, tileEdgePointers, nodeEdges, Cartesian::SOUTH, ty == 0, tx > 0);
                edgeCount += (int)tryBuildCoarseEdge(navTileData, fineData, index, index - dims.x, index - 1, dims, tiles, tileWalls, ownedTiles, navNodeIndex, tileEdgePointers, nodeEdges, Cartesian::WEST, tx == 0, ty > 0);
                edgeCount += (int)tryBuildCoarseEdge(navTileData, fineData, index, index - dims.x, index + 1, dims, tiles, tileWalls, ownedTiles, navNodeIndex, tileEdgePointers, nodeEdges, Cartesian::EAST, tx == dims.x - 1, ty > 0);
                edgeCount += (int)tryBuildCoarseEdge(navTileData, fineData, index, index - 1, index + dims.x, dims, tiles, tileWalls, ownedTiles, navNodeIndex, tileEdgePointers, nodeEdges, Cartesian::NORTH, ty == dims.y - 1, tx > 0);
            }
        }
    }
    //std::cout << " c " << timer.stop() << std::endl;
    // ================= Assign and copy edges =================
    if (edgeCount) {
        coarseNavGraph.numEdges = edgeCount;
        // All edges reside in a single memory blob
        // TODO: pool allocate
        coarseNavGraph.edges = std::unique_ptr<CoarseNavNodeEdge[]>(new CoarseNavNodeEdge[edgeCount]);
        CoarseNavNodeEdge* edgePtr = coarseNavGraph.edges.get();
        for (size_t navNodeIndex = 0; navNodeIndex < nodeEdges.size(); ++navNodeIndex) {
            std::vector<CoarseNavNodeEdge>& edgesToCopy = nodeEdges[navNodeIndex];
            memcpy(edgePtr, edgesToCopy.data(), edgesToCopy.size() * sizeof(CoarseNavNodeEdge));
            CoarseNavNode& node = coarseNavGraph.nodes[navNodeIndex];
            node.edges = edgePtr;
            node.edgeCount = edgesToCopy.size();
            edgePtr += edgesToCopy.size();
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

    mFinishedNavGraphBuildTasks.enqueue(std::move(taskData));

    //std::cout << "Nav graph generated in " << timer.stop() << "ms with " << 0 << " total nodes checked" << std::endl;

}

void NavWorld::finishNavGraphBuildTask(NavGraphBuildTaskData& taskData) {
    const TileContainerID containerId = taskData.container->getId();
    LOG_DEBUG("NAV FINISHED {}", containerId);
    // Store nav data
    const auto& navGraphIt = mNavGraphs.find(containerId);
    bool isNewContainer = (navGraphIt == mNavGraphs.end());
    if (isNewContainer) {
        if (taskData.container->isTerrain()) {
            mNavGraphs.insert(std::make_pair(containerId, ContainerNavData{ std::make_unique<ContainerTerrainDependentEdges>(), std::move(taskData.navGraph), std::move(taskData.fineNavData), taskData.container->getWorldPos3D(), taskData.container->getDims(), taskData.container->getFloorHeight(), taskData.container->getId() }));
        }
        else {
            mNavGraphs.insert(std::make_pair(containerId, ContainerNavData{ nullptr, std::move(taskData.navGraph), std::move(taskData.fineNavData), taskData.container->getWorldPos3D(), taskData.container->getDims(), taskData.container->getFloorHeight(), taskData.container->getId() }));
        }
    }
    else {
        navGraphIt->second = ContainerNavData{ nullptr, std::move(taskData.navGraph), std::move(taskData.fineNavData), taskData.container->getWorldPos3D(), taskData.container->getDims(), taskData.container->getFloorHeight(), taskData.container->getId() };
    }

    // Store in spatial lookup
    if (isNewContainer) {
        if (taskData.container->isTerrain()) {
            i32v3 worldPos = taskData.container->getWorldPos3D();
            ChunkID chunkID = ChunkID::fromWorldI32v2(i32v2(worldPos.x, worldPos.y));
            mTerrainTileContainers[chunkID.id] = taskData.container->getId();
        }
        else {
            const i32v2& worldPos2D = taskData.container->getWorldPos2D();
            const i32v2 dims2D = taskData.container->getDims2D();
            NavBBox newBox(NavBoxPoint(worldPos2D.x, worldPos2D.y), NavBoxPoint(worldPos2D.x + dims2D.x, worldPos2D.y + dims2D.y));
            mSpatialLookup.insert(ContainerNavRegion{ newBox, taskData.container->getId() });
        }
    }
    else {
        // TODO: Implement container resizing
    }

    if (!taskData.container->isTerrain()) {
        // Tell chunks about our external edges
        const ExternalEdgeList& externalEdges = taskData.externalEdges;

        Structure* owner = taskData.container->getOwnerBuilding();
        assert(owner);
        for (int j = 0; j < 4; ++j) {
            LiteChunkID id = owner->getChunkDependencies()[j];
            if (id == INVALID_CHUNK_ID) {
                break;
            }
            
            TileContainerID terrainContainerId = mTerrainTileContainers[id];
            const auto& navGraphIt = mNavGraphs.find(terrainContainerId);
            assert(navGraphIt != mNavGraphs.end()); // We must enforce that chunks always finish their first nav before any buildings are navved
            ContainerNavData& chunkNavData = navGraphIt->second;
            // TODO: SharedPtr so we don't have up to 4 copies of this memory?
            chunkNavData.terrainDependentEdges->operator[](containerId) = taskData.externalEdges;
            markChunkContainerNavDirty(id);
        }
    }
    // Release resources
    taskData.container->setDidInitNav();
    taskData.container->decRef();
}

void NavWorld::initEventHandlers() {

    TileContainerRepository::registerTileContainerListeners(mTileContainerEventListeners);
    TileContainerRepository::addEditTileListener(mTileContainerEventListeners, [this](const TileContainerEvent& containerEvent) {

        constexpr ui8 EDIT_TYPES_MASK = 0xffui8;
        static_assert(e_cast(TileContainerEditEventType::TYPES) == 5, "Update handler");

        assert(IS_GAME_THREAD());
        if (e_cast(containerEvent.edit.type) & EDIT_TYPES_MASK) {
            LOG_DEBUG("NAV DIRTY {}",  containerEvent.container->getId());
            markContainerNavDirty(containerEvent.container);
        }
    });

    TileContainerRepository::addDestroyListener(mTileContainerEventListeners, [this](const TileContainerEvent& containerEvent) {
        assert(IS_GAME_THREAD());
        const TileContainer& container = *containerEvent.container;
        mContainersToDestroy.enqueue(TileContainerToDestroy{ container.getWorldPos3D(), container.getDims2D(), container.getId(), container.isTerrain() });
    });
}

bool NavWorld::trySetFineNavEdgeCartesian(TileIndex tileIndex, TileIndex adjacentIndex, Cartesian8 cartesian8, bool isInner, const i32v3& containerDims, const std::vector<Tile>& tiles, const std::vector<TileWalls>& tileWallsContainer, const BitArray& ownedTiles, const f32 groundZPosition, const f32 floorHeight, TileFineNavData& tileFineNavData, int prevZ, ExternalEdgeList* externalEdges) {

    const Cartesian cartesian = CARTESIAN8_TO_CARTESIAN[e_cast(cartesian8)];
    assert(cartesian != Cartesian::NONE);
    const int floorStride = containerDims.x * containerDims.y;
    assert(cartesian != Cartesian::NONE); // This must be a 4 cartesian
    if (isInner && TileContainer::isTileOwned(ownedTiles, adjacentIndex)) {
        // Interior edge
        const Tile* adjacent = &tiles[adjacentIndex];
        // Empty tiles, we go down a floor
        // TODO: Ground check only?
        if (adjacent->isEmpty()) {
            if (adjacentIndex >= floorStride) {
                adjacentIndex = adjacentIndex - floorStride;
                if (TileContainer::isTileOwned(ownedTiles, adjacentIndex)) {
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
        if (canEnterTileInDirection(*adjacent, tileWallsContainer[adjacentIndex], cartesian) &&
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

bool NavWorld::trySetFineNavEdgeCartesianDiagonal(TileIndex adjacentIndex, Cartesian8 cartesian8, bool isInner, const i32v3& containerDims, const std::vector<Tile>& tiles, const std::vector<TileWalls>& tileWallsContainer, const BitArray& ownedTiles, const f32 groundZPosition, TileFineNavData& fineNavData) {
    UNUSED(containerDims);
    if (isInner && TileContainer::isTileOwned(ownedTiles, adjacentIndex)) {
        // Interior edge
        const Tile& adjacent = tiles[adjacentIndex];
        if (canEnterTileInDirectionDiagonal(adjacent, tileWallsContainer[adjacentIndex], cartesian8) &&
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

void NavWorld::markChunkContainerNavDirty(LiteChunkID chunkId)
{
    assert(IS_NAV_THREAD());
    Chunk& chunk = sWorld->getChunk(chunkId);
    TileContainer* chunkTileContainer = chunk.getTileContainer();
    // Mark dirty again
    bool didAdd = false;
    {
        std::lock_guard lock(mDirtyTileContainersMutex);
        auto&& it = mDirtyTileContainers.find(chunkTileContainer);
        if (it == mDirtyTileContainers.end()) {
            didAdd = true;
            mDirtyTileContainers.insert(chunkTileContainer);
        }
    }
    // If we added, we dont decref as we will get dec-reffed after updating the container
    if (!didAdd) {
        chunk.decRef();
    }
    else {
        LOG_DEBUG("NAV FORCE DIRTY CHUNK {}", chunkId);
    }
}

bool NavWorld::tryBuildCoarseEdge(NavGraphTileDataToCopy& navTileData, const TileFineNavData& fineNavData, const TileIndex index, const TileIndex prevIndex, TileIndex outerIndex, const i32v3& containerDims, const std::vector<Tile>& tiles, const std::vector<TileWalls>& tileWallsContainer, const BitArray& ownedTiles, const ui16 navNodeIndex, std::vector<CoarseTileEdgePointer>& tileEdgePointers, std::vector<std::vector<CoarseNavNodeEdge>>& nodeEdges, const Cartesian dir, bool isBorder, bool canExtendPrevEdge)
{
    // We have an edge only if there is no wall
     // South edge
    bool needNewEdge = true;
    TileCoarseNavEdgeType coarseEdgeType = TileCoarseNavEdgeType::NONE;
    ui16 adjacentNodeIndex = INVALID_NAV_NODE_INDEX; // Signifies external edge
    if (fineNavData.canAccessDirection(CARTESIAN_TO_CARTESIAN8[e_cast(dir)])) {
        const Tile& tile = tiles[index];
        const bool hasFlag = tile.hasFlag(FORCE_COARSE_NAV_FLAGS[e_cast(dir)]);
        if (isBorder || !TileContainer::isTileOwned(ownedTiles, outerIndex) || hasFlag) {
            coarseEdgeType = TileCoarseNavEdgeType::EXTERIOR;
            // External edge
            if (canExtendPrevEdge) {
                const Tile& prevTile = tiles[prevIndex];
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
            const TileWalls& outerWalls = tileWallsContainer[outerIndex];
            if (canEnterTileInDirection(outerTile, outerWalls, dir)) {
                adjacentNodeIndex = outerNavNodeIndex;
                // If we can extend prev wall
                if (canExtendPrevEdge) {
                    const Tile& prevTile = tiles[prevIndex];
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

void NavWorld::debugDrawCoarseNavGraphForContainer(const TileContainer& tileContainer, OPT const f32* heightData, ui32 lifetime, int debugId /*= 0*/) const
{
    const color4 color1(0.0f, 1.0f, 1.0f, 0.75f);
    const color4 color2(1.0f, 0.0f, 0.0f, 0.75f);
    const color4 color3(1.0f, 1.0f, 1.0f, 0.75f);
    const color4 color4(1.0f, 0.0f, 1.0f, 0.75f);
    const IHeightmapGrid& heightGrid = sWorld->getHeightmapGrid();
    const TileContainerID containerId = tileContainer.getId();
    if (!tileContainer.isTerrain()) {
        heightData = nullptr;
    }
    auto&& it = mNavGraphs.find(containerId);
    if (it == mNavGraphs.end()) {
        std::cout << "Failed to find navgraph for container " << containerId << std::endl;
        return;
    }
    const CoarseNavGraph& graph = it->second.coarseNavGraph;
    // Draw edges and connections
    for (ui32 nodeIndex = 0; nodeIndex < graph.numNodes; ++nodeIndex) {
        const CoarseNavNode& node = graph.nodes[nodeIndex];
        const i32v3& containerPos = tileContainer.getWorldPos3D();
        const ui32 edgeCount = node.edgeCount;
        for (ui32 i = 0; i < edgeCount; ++i) {
            const CoarseNavNodeEdge& edge = node.edges[i];
            const Tile& tile = tileContainer.getTileAt(edge.startPos);
            i32v3 startOffset = tileContainer.getTileXYZOffsetWithZScale(edge.startPos);
            // TODO: Remove
            if (heightData) {
                HeightmapPatchID patchId(f32v2(tileContainer.getWorldPos2D()));
                startOffset.z = heightGrid.computeHeightAtPoint(patchId, heightData, f32v2(containerPos + startOffset));
            }
            i32v3 worldPos = containerPos + startOffset;
            // TODO: This is not thread safe!
            worldPos.z += tile.getGroundZOffset();

            if (edge.dir == Cartesian::EAST) worldPos.x += 1.0f;
            else if (edge.dir == Cartesian::NORTH) worldPos.y += 1.0f;
            const f32v2 offset = f32v2(CARTESIAN_EDGE_DIRS_ABS[e_cast(edge.dir)]) * (f32)(edge.edgeLength/* + 1.0f*/);
            f32v3 pointA = f32v3(worldPos) + f32v3(0.00f, 0.00f, 0.00f);
            f32v3 pointB = f32v3(worldPos) + f32v3(offset.x, offset.y, 0.0f);
            if (heightData) {
                HeightmapPatchID patchId(f32v2(tileContainer.getWorldPos2D()));
                pointA.z = heightGrid.computeHeightAtPoint(patchId, heightData, f32v2(pointA));
                pointB.z = heightGrid.computeHeightAtPoint(patchId, heightData, f32v2(pointB));
            }
            if (edge.isExternalEdge()) {
                DebugRenderer::drawLineBetweenPoints(pointA, pointB, color2, lifetime, debugId);
            }
            else {
                DebugRenderer::drawLineBetweenPoints(pointA, pointB, color1, lifetime, debugId);
            }
            f32v3 midpoint(worldPos.x + offset.x * 0.5f, worldPos.y + offset.y * 0.5f, (pointA.z + pointB.z) * 0.5f);
            if (heightData) {
                HeightmapPatchID patchId(f32v2(tileContainer.getWorldPos2D()));
                midpoint.z = heightGrid.computeHeightAtPoint(patchId, heightData, f32v2(midpoint));
            }
            f32v3 third(midpoint.x + CARTESIAN_NORMALS[e_cast(edge.dir)].x, midpoint.y + CARTESIAN_NORMALS[e_cast(edge.dir)].y, midpoint.z);
            // TODO: Combine above
            DebugRenderer::drawLineBetweenPoints(midpoint, third, color3, lifetime, debugId);

            for (ui32 j = i + 1; j < edgeCount; ++j) {
                const CoarseNavNodeEdge& edge2 = node.edges[j];
                i32v3 startOffset2 = tileContainer.getTileXYZOffsetWithZScale(edge2.startPos);
                if (heightData) {
                    HeightmapPatchID patchId(f32v2(tileContainer.getWorldPos2D()));
                    startOffset2.z = heightGrid.computeHeightAtPoint(patchId, heightData, f32v2(containerPos + startOffset2));
                }
                i32v3 worldPos2 = containerPos + startOffset2;
                if (edge2.dir == Cartesian::EAST) worldPos2.x += 1.0f;
                else if (edge2.dir == Cartesian::NORTH) worldPos2.y += 1.0f;
                const f32v2 offset2 = f32v2(CARTESIAN_EDGE_DIRS_ABS[e_cast(edge2.dir)]) * (f32)(edge2.edgeLength/* + 1.0f*/);
                f32v3 midpoint2(worldPos2.x + offset2.x * 0.5f, worldPos2.y + offset2.y * 0.5f, worldPos2.z);
                // TODO: Not thread safe!
                midpoint2.z += tileContainer.getTileAt(edge2.startPos).getGroundZOffset();
                if (heightData) {
                    HeightmapPatchID patchId(f32v2(tileContainer.getWorldPos2D()));
                    midpoint2.z = heightGrid.computeHeightAtPoint(patchId, heightData, f32v2(midpoint2));
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

    for (ui32 tileIndex = 0; tileIndex < fineNavData.size(); ++tileIndex) {
        if (tileContainer.isTileOwned(tileIndex)) {
            f32v3 worldPos = f32v3(tileContainer.getTileXYZOffsetWithZScale(tileIndex) + tileContainer.getWorldPos3D());
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

void NavWorld::debugDrawCoarseNavNode(const TileHandle& tileHandle, OPT const f32* heightData, ui32 lifetime, int debugId /*= 0*/) const
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
    const IHeightmapGrid& heightGrid = sWorld->getHeightmapGrid();
    const TileContainerID containerId = tileHandle.container->getId();
    if (!tileHandle.container->isTerrain()) {
        heightData = nullptr;
    }
    auto&& it = mNavGraphs.find(containerId);
    if (it == mNavGraphs.end()) {
        std::cout << "Failed to find navgraph for container " << containerId << std::endl;
        return;
    }
    const CoarseNavGraph& graph = it->second.coarseNavGraph;
    const CoarseNavNode& node = graph.nodes[coarseIndex];
    const i32v3& containerPos = tileHandle.container->getWorldPos3D();
    const ui32 edgeCount = node.edgeCount;
    for (ui32 i = 0; i < edgeCount; ++i) {
        const CoarseNavNodeEdge& edge = node.edges[i];
        i32v3 startOffset = tileHandle.container->getTileXYZOffsetWithZScale(edge.startPos);
        // TODO: Remove
        if (heightData) {
            HeightmapPatchID patchId(f32v2(tileHandle.container->getWorldPos2D()));
            startOffset.z = heightGrid.computeHeightAtPoint(patchId, heightData, f32v2(containerPos + startOffset));
        }
        f32v3 worldPos = containerPos + startOffset;
        // TODO: Not thread safe!
        worldPos.z += tileHandle.container->getTileAt(edge.startPos).getGroundZOffset();
        if (edge.dir == Cartesian::EAST) worldPos.x += 1.0f;
        else if (edge.dir == Cartesian::NORTH) worldPos.y += 1.0f;
        const f32v2 offset = f32v2(CARTESIAN_EDGE_DIRS_ABS[e_cast(edge.dir)]) * (f32)(edge.edgeLength/* + 1.0f*/);
        f32v3 pointA = f32v3(worldPos) + f32v3(0.00f, 0.00f, 0.00f);
        f32v3 pointB = f32v3(worldPos) + f32v3(offset.x, offset.y, 0.0f);
        if (heightData) {
            HeightmapPatchID patchId(f32v2(tileHandle.container->getWorldPos2D()));
            pointA.z = heightGrid.computeHeightAtPoint(patchId, heightData, f32v2(pointA));
            pointB.z = heightGrid.computeHeightAtPoint(patchId, heightData, f32v2(pointB));
        }
        if (edge.isExternalEdge()) {
            DebugRenderer::drawLineBetweenPoints(pointA, pointB, color2, lifetime, debugId);
        }
        else {
            DebugRenderer::drawLineBetweenPoints(pointA, pointB, color1, lifetime, debugId);
        }
        f32v3 midpoint(worldPos.x + offset.x * 0.5f, worldPos.y + offset.y * 0.5f, (pointA.z + pointB.z) * 0.5f);
        if (heightData) {
            HeightmapPatchID patchId(f32v2(tileHandle.container->getWorldPos2D()));
            midpoint.z = heightGrid.computeHeightAtPoint(patchId, heightData, f32v2(midpoint));
        }
        f32v3 third(midpoint.x + CARTESIAN_NORMALS[e_cast(edge.dir)].x, midpoint.y + CARTESIAN_NORMALS[e_cast(edge.dir)].y, midpoint.z);
        // TODO: Combine above
        DebugRenderer::drawLineBetweenPoints(midpoint, third, color3, lifetime, debugId);

        for (ui32 j = i + 1; j < edgeCount; ++j) {
            const CoarseNavNodeEdge& edge2 = node.edges[j];
            i32v3 startOffset2 = tileHandle.container->getTileXYZOffsetWithZScale(edge2.startPos);
            if (heightData) {
                HeightmapPatchID patchId(f32v2(tileHandle.container->getWorldPos2D()));
                startOffset2.z = heightGrid.computeHeightAtPoint(patchId, heightData, f32v2(containerPos + startOffset2));
            }
            i32v3 worldPos2 = containerPos + startOffset2;
            if (edge2.dir == Cartesian::EAST) worldPos2.x += 1.0f;
            else if (edge2.dir == Cartesian::NORTH) worldPos2.y += 1.0f;
            const f32v2 offset2 = f32v2(CARTESIAN_EDGE_DIRS_ABS[e_cast(edge2.dir)]) * (f32)(edge2.edgeLength/* + 1.0f*/);
            f32v3 midpoint2(worldPos2.x + offset2.x * 0.5f, worldPos2.y + offset2.y * 0.5f, worldPos2.z);
            // TODO: Not thread safe!
            midpoint2.z += tileHandle.container->getTileAt(edge2.startPos).getGroundZOffset();
            if (heightData) {
                HeightmapPatchID patchId(f32v2(tileHandle.container->getWorldPos2D()));
                midpoint2.z = heightGrid.computeHeightAtPoint(patchId, heightData, f32v2(midpoint2));
            }
            DebugRenderer::drawLineBetweenPoints(midpoint, midpoint2, color4, lifetime, debugId);
        }
    }
}

const CoarseNavGraph* NavWorld::tryGetCoarseNavGraph(TileContainerID containerId) const {
    assert(IS_NAV_THREAD());
    auto&& it = mNavGraphs.find(containerId);
    if (it == mNavGraphs.end()) return nullptr;
    return &it->second.coarseNavGraph;
}

const CoarseNavGraph& NavWorld::getCoarseNavGraph(TileContainerID containerId) const {
    assert(IS_NAV_THREAD());
    auto&& it = mNavGraphs.find(containerId);
    assert(it != mNavGraphs.end());
    return it->second.coarseNavGraph;
}


const CoarseNavNode* NavWorld::getCoarseNavNode(CoarseNavNodeIndexPair index) const {
    return getCoarseNavNode(index.tileContainerID, index.index);
}

const CoarseNavNode* NavWorld::getCoarseNavNode(TileContainerID containerId, ui16 navNodeIndex) const {
    assert(IS_NAV_THREAD());
    // TODO: what if invalid
    auto&& it = mNavGraphs.find(containerId);
    assert(it != mNavGraphs.end());
    const CoarseNavGraph& patch = it->second.coarseNavGraph;
    assert(navNodeIndex < patch.numNodes);
    return &patch.nodes[navNodeIndex];
}

TileFineNavData NavWorld::getFineNavData(TileContainerID containerId, TileIndex tileIndex) const {
    assert(IS_NAV_THREAD());
    // TODO: what if invalid
    auto&& it = mNavGraphs.find(containerId);
    assert(it != mNavGraphs.end());
    const std::vector<TileFineNavData>& fineNavGraph = it->second.fineNavGraph;
    assert(tileIndex < fineNavGraph.size());
    return fineNavGraph[tileIndex];
}

TileFineNavData NavWorld::getFineNavDataAndContainerDims(TileContainerID containerId, TileIndex tileIndex, OUT i32v3& containerDims, OUT i32& floorHeight) const {
    assert(IS_NAV_THREAD());
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
    assert(IS_NAV_THREAD());
    // TODO: what if invalid
    auto&& it = mNavGraphs.find(containerId);
    assert(it != mNavGraphs.end());
    return it->second;
}

LiteTileHandle NavWorld::getTileHandleAndNavDataAtWorldPos(const i32v3& worldPos, OUT const ContainerNavData** outNavData) const {
    assert(IS_NAV_THREAD());
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
        TileIndex tileIndex = TileContainer::getTileIndexFromXYZOffset(offset, navData.containerDims);
        if (navData.fineNavGraph[tileIndex].isOwned) {
            *outNavData = &navData;
            return LiteTileHandle(containerRegion.id, tileIndex);
        }
    }

    // If we find no container, return valid chunk container position at this point
    // WORLD ORIGIN MUST be 0
    const i32v2 chunkOffset(worldPos.x / CHUNK_WIDTH, worldPos.y / CHUNK_WIDTH);
    const GridIdType chunkId = chunkOffset.y * WorldData::WORLD_WIDTH_CHUNKS + chunkOffset.x;
    TileContainerID containerId = mTerrainTileContainers[chunkId];
    if (containerId != INVALID_TILE_CONTAINER_ID) {
        auto&& it = mNavGraphs.find(containerId);
        assert(it != mNavGraphs.end());
        const ContainerNavData& navData = it->second;
        const i32v2 offset = i32v2(worldPos.x - navData.worldPos.x, worldPos.y - navData.worldPos.y);
        TileIndex tileIndex = TileContainer::getBaseTileIndexFromXYOffset(offset, navData.containerDims);
        assert(tileIndex < navData.containerDims.x* navData.containerDims.y* navData.containerDims.z);
        *outNavData = &navData;
        return LiteTileHandle(containerId, tileIndex);
    }
    outNavData = nullptr;
    return LiteTileHandle();
}

void NavWorld::markContainerNavDirty(TileContainer* container) {
    assert(IS_GAME_THREAD());
    assert(container);
    bool didAdd = false;
    
    {
        std::lock_guard lock(mDirtyTileContainersMutex);
        auto&& it = mDirtyTileContainers.find(container);
        if (it == mDirtyTileContainers.end()) {
            didAdd = true;
            mDirtyTileContainers.insert(container);
        }
    }
    // Make sure we don't get deallocated while we are in the dirty list
    // TODO: Technically this is race condition if the nav thread and worker thread manage to finish their entire cycle before we get here.. but
    // this should be statistically impossible
    if (didAdd) {
        container->incRef();
        if (!container->isTerrain()) {
            Structure* owner = container->getOwnerBuilding();
            assert(owner);
            assert(!owner->hasUnloadedChunkDependencies());
            for (int i = 0; i < 4; ++i) {
                LiteChunkID id = owner->getChunkDependencies()[i];
                if (id == INVALID_CHUNK_ID) {
                    break;
                }
                // Make sure this chunk stays
                sWorld->getChunk(id).incRef();
            }
        }
    }
}
