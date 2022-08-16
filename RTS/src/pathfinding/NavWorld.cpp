#include "stdafx.h"
#include "NavWorld.h"

#include "World.h"
#include "world/Chunk.h"
#include "DebugRenderer.h"
#include "options/DebugOptions.h"

#include "debugging/VisualLogger.h"

#include "resources/TileRepository.h"

// 0 or 1 for rendering debug
#define DEBUG_RENDER_NAV_NODES 1


inline f32v3 helperGet3DPoint(const WorldGrid& worldGrid, const f32v2& pos2d) {
    return f32v3(pos2d.x, pos2d.y, worldGrid.tryComputeHeightAtPoint(pos2d));
}

inline f32v3 helperGet3DPoint(const WorldGrid& worldGrid, const HeightmapPatchID& patchId, const f32* heightData, const f32v2& pos2d) {
    if (!heightData)
    {
        return f32v3(pos2d.x, pos2d.y, 4.0f);
    }
    return f32v3(pos2d.x, pos2d.y, worldGrid.computeHeightAtPoint(patchId, heightData, pos2d));
}


NavWorld::NavWorld(World& world) : mWorld(world)
{

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
bool canEnterTileInDirection(const Tile& tile, const TileWalls& walls, Cartesian toDir) {
    if (!tile.hasFlagThreadSafe(TileFlags::TILE_FLAG_IS_IMPASSABLE)) {
        const Cartesian fromDir = CARTESIAN_OPPOSITES[e_cast(toDir)];
        return walls.walls[e_cast(fromDir)].canNavThrough();
    }
    return false;
}
bool canEnterTileInDirectionDiagonal(const Tile& tile, const TileWalls& walls, Cartesian8 toDir) {
    if (!tile.hasFlagThreadSafe(TileFlags::TILE_FLAG_IS_IMPASSABLE)) {
        const CartesianPair fromDirs = CARTESIAN_DIAGONAL_OPPOSITES[e_cast(toDir)];
        return (walls.walls[e_cast(fromDirs.first)].canNavThrough() && walls.walls[e_cast(fromDirs.second)].canNavThrough());
    }
    return false;
}

// ALGORITHM DESCRIPTION
// 1. Build disjoint sets, with a specific grid size for a maximum disjoint set node width
// 2. Create nav nodes from distinct disjoint set nodes
// 3. Create edges between disjoint set nodes and each other, or "External" which means it connects to the outside world
void NavWorld::buildNavGraphForContainer(TileContainer& tileContainer, OUT CoarseNavGraph& navGraph, OUT NavGraphTileDataToCopy& navTileData) {

    PreciseTimer timer;

    // Nav thread only
    assert(!IS_MAIN_THREAD());

    //ScopedTimer timer("Built nav graph");
    const i32v3& dims = tileContainer.getDims();

    Chunk* chunk = nullptr;
    if (tileContainer.isTerrain()) {
        chunk = &mWorld.getWorldGrid().getChunk(ChunkID(f32v2(tileContainer.getWorldPos2D())));
    }


    ui32 totalDjSets = 0;


    const ui32 GRID_WIDTH = SUBCHUNK_WIDTH;
    // Reserve nodes
    navTileData.djNodes.reserve(SQ(GRID_WIDTH) * dims.z);

    // ================= Create disjoint set and fine nav data =================
    const std::vector<Tile>& tiles = tileContainer.getTiles();
    const std::vector<TileWallContainer>& tileWallContainers = tileContainer.getTileWallContainers();
    std::vector<TileFineNavData>& fineNavDataArray = tileContainer.mFineNavData;

    navTileData.tileDjNodeIDs.resize(tiles.size(), INVALID_DJ_NODE_ID);

    TileIndex index = 0;
    for (int tz = 0; tz < dims.z; ++tz) {
        for (int ty = 0; ty < dims.y; ++ty) {
            const int gridYOffset = ty % GRID_WIDTH;
            for (int tx = 0; tx < dims.x; ++tx) {
                // Determine if we own this tile
                const TileIndex index = tileContainer.getTileIndexFromXYZOffset(tx, ty, tz);
                if (!tileContainer.isTileOwned(index)) {
                    continue;
                }

                const Tile& tile = tiles[index];
                const TileWalls& walls = tileWallContainers[index].wallsThreadSafe;
                // Impassible tiles are not part of navgraph
               /* if (tile.hasFlagThreadSafe(TileFlags::TILE_FLAG_IS_IMPASSABLE)) {
                    continue;
                }*/


                const f32 groundZPosition = tile.getGroundZPositionUncompressedThreadSafe();
                bool assigned = false;

                // ================= Fine Nav Data =================
                TileFineNavData& fineNavData = fineNavDataArray[index];
                fineNavData.reset();
                // TODO: which tile do we use for path weight?
                TileID groundId = tile.getLayersThreadSafe()[TILE_LAYER_GROUND];
                if (groundId != TILE_ID_NONE) {
                    fineNavData.pathWeight = TileRepository::getTileData(groundId).pathWeight;
                }
                bool canGoSouthWest = true;
                bool canGoSouthEast = true;
                bool canGoNorthWest = true;
                bool canGoNorthEast = true;
                // South
                if (walls.south.canNavThrough()) {
                    setFineNavEdgeCartesian(index - dims.x, Cartesian8::SOUTH, ty > 0, tileContainer, groundZPosition, fineNavData);
                }
                else {
                    canGoSouthWest = canGoSouthEast = false;
                }
                // West
                if (walls.west.canNavThrough()) {
                    setFineNavEdgeCartesian(index - 1, Cartesian8::WEST, tx > 0, tileContainer, groundZPosition, fineNavData);
                }
                else {
                    canGoSouthWest = canGoNorthWest = false;
                }
                // East
                if (walls.east.canNavThrough()) {
                    setFineNavEdgeCartesian(index + 1, Cartesian8::EAST, tx < dims.x - 1, tileContainer, groundZPosition, fineNavData);
                }
                else {
                    canGoSouthEast = canGoNorthEast = false;
                }
                // North
                if (walls.north.canNavThrough()) {
                    setFineNavEdgeCartesian(index + dims.x, Cartesian8::NORTH, ty < dims.y - 1, tileContainer, groundZPosition, fineNavData);
                }
                else {
                    canGoNorthWest = canGoNorthEast = false;
                }

                // South West
                if (canGoSouthWest) {
                    setFineNavEdgeCartesianDiagonal(index - dims.x - 1, Cartesian8::SOUTH_WEST, ty > 0 && tx > 0, tileContainer, groundZPosition, fineNavData);
                }
                // South East
                if (canGoSouthEast) {
                    setFineNavEdgeCartesianDiagonal(index - dims.x + 1, Cartesian8::SOUTH_EAST, ty > 0 && tx < dims.x - 1, tileContainer, groundZPosition, fineNavData);
                }
                // North West
                if (canGoNorthWest) {
                    setFineNavEdgeCartesianDiagonal(index + dims.x - 1, Cartesian8::NORTH_WEST, ty < dims.y - 1 && tx > 0, tileContainer, groundZPosition, fineNavData);
                }
                // North East
                if (canGoNorthEast) {
                    setFineNavEdgeCartesianDiagonal(index + dims.x + 1, Cartesian8::NORTH_EAST, ty < dims.y - 1 && tx < dims.x - 1, tileContainer, groundZPosition, fineNavData);
                }

                // ================= Disjoint Set =================
                const int gridXOffset = tx % GRID_WIDTH;
                if (gridXOffset != 0) {
                    if (fineNavData.canAccessDirection(Cartesian8::WEST)) {
                        navTileData.tileDjNodeIDs[index] = navTileData.tileDjNodeIDs[index - 1];
                        assigned = true;
                    }
                }
                if (gridYOffset != 0) {
                    const Tile& bottom = tiles[index - dims.x];
                    // Check if we can cross between
                    if (fineNavData.canAccessDirection(Cartesian8::SOUTH)) {
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
        navGraph.numNodes = totalDjSets;
        navGraph.nodes = std::unique_ptr<CoarseNavNode[]>(new CoarseNavNode[totalDjSets]);
        // Tell the nav nodes who they belong to
        for (ui32 i = 0; i < totalDjSets; ++i) {
            navGraph.nodes[i].tileContainerID = tileContainer.getId();
        }
    }
    else {
        navGraph.numNodes = 0;
        navGraph.nodes = nullptr;
        for (ui32 i = 0; i < tiles.size(); ++i) {
            tiles[i].setNavNodeIndex(INVALID_NAV_NODE_INDEX);
        }
        return;
    }
    
    // ================= Build edges =================
    // TODO: Optimize allocate
    std::vector<std::vector<CoarseNavNodeEdge>> nodeEdges;
    nodeEdges.resize(navGraph.numNodes);
    ui32 edgeCount = 0;
    // Lets us know what edges are at a given tile
    std::vector<TileEdgePointer> tileEdgePointers;
    tileEdgePointers.resize(tiles.size());

    //std::cout << " B " << timer.stop() << std::endl;
    for (int tz = 0; tz < dims.z; ++tz) {
        for (int ty = 0; ty < dims.y; ++ty) {
            const int gridYOffset = ty % GRID_WIDTH;
            for (int tx = 0; tx < dims.x; ++tx) {
                // Determine if we own this tile
                const TileIndex index = tileContainer.getTileIndexFromXYZOffset(tx, ty, tz);
                const Tile& tile = tileContainer.getTileAt(index);
                const ui16 navNodeIndex = navTileData.tileDjNodeIDs[index];
                // This must be part of a nav node
                if (navNodeIndex == INVALID_NAV_NODE_INDEX) {
                    continue;
                }
                
                const TileWalls& walls = tileContainer.getWallsThreadSafe(index);

                edgeCount += (int)tryBuildEdge(navTileData, walls, index, index - 1, index - dims.x, tileContainer, navNodeIndex, tileEdgePointers, nodeEdges, Cartesian::SOUTH, ty == 0, tx > 0);
                edgeCount += (int)tryBuildEdge(navTileData, walls, index, index - dims.x, index - 1, tileContainer, navNodeIndex, tileEdgePointers, nodeEdges, Cartesian::WEST, tx == 0, ty > 0);
                edgeCount += (int)tryBuildEdge(navTileData, walls, index, index - dims.x, index + 1, tileContainer, navNodeIndex, tileEdgePointers, nodeEdges, Cartesian::EAST, tx == dims.x - 1, ty > 0);
                edgeCount += (int)tryBuildEdge(navTileData, walls, index, index - 1, index + dims.x, tileContainer, navNodeIndex, tileEdgePointers, nodeEdges, Cartesian::NORTH, ty == dims.y - 1, tx > 0);
            }
        }
    }
    //std::cout << " c " << timer.stop() << std::endl;
    // ================= Assign and copy edges =================
    if (edgeCount) {
        navGraph.numEdges = edgeCount;
        // All edges reside in a single memory blob
        // TODO: pool allocate
        navGraph.edges = std::unique_ptr<CoarseNavNodeEdge[]>(new CoarseNavNodeEdge[edgeCount]);
        CoarseNavNodeEdge* edgePtr = navGraph.edges.get();
        for (size_t navNodeIndex = 0; navNodeIndex < nodeEdges.size(); ++navNodeIndex) {
            std::vector<CoarseNavNodeEdge>& edgesToCopy = nodeEdges[navNodeIndex];
            memcpy(edgePtr, edgesToCopy.data(), edgesToCopy.size() * sizeof(CoarseNavNodeEdge));
            CoarseNavNode& node = navGraph.nodes[navNodeIndex];
            node.edges = edgePtr;
            node.edgeCount = edgesToCopy.size();
            edgePtr += edgesToCopy.size();
        }
    }
    else {
        navGraph.edges = nullptr;
        navGraph.numEdges = 0;
    }


    std::cout << "Nav graph generated in " << timer.stop() << "ms with " << 0 << " total nodes checked" << std::endl;

}

void NavWorld::setFineNavEdgeCartesian(const TileIndex adjacentIndex, Cartesian8 cartesian8, bool isInner, TileContainer& tileContainer, const f32 groundZPosition, TileFineNavData& fineNavData) {
    const i32v3& dims = tileContainer.getDims();
    const std::vector<Tile>& tiles = tileContainer.getTiles();
    const std::vector<TileWallContainer>& tileWallContainers = tileContainer.getTileWallContainers();
    const Cartesian cartesian = CARTESIAN8_TO_CARTESIAN[e_cast(cartesian8)];
    assert(cartesian != Cartesian::NONE); // This must be a 4 cartesian
    if (isInner && tileContainer.isTileOwned(adjacentIndex)) {
        // Interior edge
        const Tile& adjacent = tiles[adjacentIndex];
        if (canEnterTileInDirection(adjacent, tileWallContainers[adjacentIndex].wallsThreadSafe, cartesian) /*&&
            abs(adjacent.getGroundZPositionUncompressedThreadSafe() - groundZPosition) < 2.0f*/) {
            fineNavData.setCanAccessDirection(cartesian8, true);
            // TODO: Handle stairs
            //fineNavData.setEdgeType(Cartesian::SOUTH, TileFineNavEdgeType::UP);
        }
    }
    else {
        // Exterior edge
        fineNavData.setCanAccessDirection(cartesian8, true);
        fineNavData.setEdgeType(cartesian, TileFineNavEdgeType::EXTERIOR);
    }
}

void NavWorld::setFineNavEdgeCartesianDiagonal(const TileIndex adjacentIndex, Cartesian8 cartesian8, bool isInner, TileContainer& tileContainer, const f32 groundZPosition, TileFineNavData& fineNavData) {
    const i32v3& dims = tileContainer.getDims();
    const std::vector<Tile>& tiles = tileContainer.getTiles();
    const std::vector<TileWallContainer>& tileWallContainers = tileContainer.getTileWallContainers();
    if (isInner && tileContainer.isTileOwned(adjacentIndex)) {
        // Interior edge
        const Tile& adjacent = tiles[adjacentIndex];
        if (canEnterTileInDirectionDiagonal(adjacent, tileWallContainers[adjacentIndex].wallsThreadSafe, cartesian8) /*&&
            abs(adjacent.getGroundZPositionUncompressedThreadSafe() - groundZPosition) < 2.0f*/) {
            fineNavData.setCanAccessDirection(cartesian8, true);
            // TODO: Handle stairs
            //fineNavData.setEdgeType(Cartesian::SOUTH, TileFineNavEdgeType::UP);
        }
    }
    else {
        // TODO: Allow diagonal exterior edges
        //fineNavData.setCanAccessDirection(cartesian8, false);
       // fineNavData.setEdgeType(cartesian8, TileFineNavEdgeType::EXTERIOR);
    }
}

bool NavWorld::tryBuildEdge(NavGraphTileDataToCopy& navTileData, const TileWalls& walls, const TileIndex index, const TileIndex prevIndex, const TileIndex outerIndex, TileContainer& tileContainer, const ui16 navNodeIndex, std::vector<TileEdgePointer>& tileEdgePointers, std::vector<std::vector<CoarseNavNodeEdge>>& nodeEdges, const Cartesian dir, bool isBorder, bool canExtendPrevEdge)
{
    // We have an edge only if there is no wall
     // South edge
    bool needNewEdge = true;
    ui16 adjacentNodeIndex = INVALID_NAV_NODE_INDEX;
    if (walls.walls[e_cast(dir)].canNavThrough()) {
        if (isBorder || !tileContainer.isTileOwned(outerIndex)) {
            // External edge
            if (canExtendPrevEdge) {
                const Tile& prevTile = tileContainer.getTileAt(prevIndex);
                if (navTileData.tileDjNodeIDs[prevIndex] == navNodeIndex) {
                    TileEdgePointer& prevEdgePointer = tileEdgePointers[prevIndex];
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
            const Tile& outerTile = tileContainer.getTileAt(outerIndex);
            const ui32 outerNavNodeIndex = navTileData.tileDjNodeIDs[outerIndex];
            // No edge becase we are the same nav node
            if (outerNavNodeIndex == navNodeIndex) {
                return false;
            }
            const TileWalls& outerWalls = tileContainer.getWallsThreadSafe(outerIndex);
            if (canEnterTileInDirection(outerTile, outerWalls, dir)) {
                adjacentNodeIndex = outerNavNodeIndex;
                // If we can extend prev wall
                if (canExtendPrevEdge) {
                    const Tile& prevTile = tileContainer.getTileAt(prevIndex);
                    if (navTileData.tileDjNodeIDs[prevIndex] == navNodeIndex) {
                        TileEdgePointer& prevEdgePointer = tileEdgePointers[prevIndex];
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
        TileEdgePointer& edgePointer = tileEdgePointers[index];
        // Edge list for this node
        std::vector<CoarseNavNodeEdge>& edges = nodeEdges[navNodeIndex];
        edgePointer.edges[e_cast(dir)] = edges.size();
        CoarseNavNodeEdge& newEdge = edges.emplace_back();
        newEdge.startPos = index;
        newEdge.edgeLength = 1;
        newEdge.dir = dir;
        newEdge.adjacentNodeIndex = adjacentNodeIndex;
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
    const WorldGrid& worldGrid = mWorld.getWorldGrid();
    const TileContainerID containerId = tileContainer.getId();
    const f32* heightData = nullptr;
    HeightmapPatchID patchId(f32v2(tileContainer.getWorldPos2D()));
    if (tileContainer.isTerrain()) {
        heightData = worldGrid.getHeightDataAt(patchId)->data;
    }
    auto&& it = mNavGraphs.find(containerId);
    if (it == mNavGraphs.end()) {
        std::cout << "Failed to find navgraph for container " << containerId << std::endl;
        return;
    }
    const CoarseNavGraph& graph = it->second;
    // Draw edges and connections
    for (ui32 nodeIndex = 0; nodeIndex < graph.numNodes; ++nodeIndex) {
        const CoarseNavNode& node = graph.nodes[nodeIndex];
        const i32v3& containerPos = tileContainer.getWorldPos3D();
        const ui32 edgeCount = node.edgeCount;
        for (ui32 i = 0; i < edgeCount; ++i) {
            const CoarseNavNodeEdge& edge = node.edges[i];
            i32v3 startOffset = tileContainer.getTileXYZOffsetWithZScale(edge.startPos);
            // TODO: Remove
            if (heightData) {
                startOffset.z = worldGrid.computeHeightAtPoint(patchId, heightData, f32v2(containerPos + startOffset));
            }
            i32v3 worldPos = containerPos + startOffset;
            if (edge.dir == Cartesian::EAST) worldPos.x += 1.0f;
            else if (edge.dir == Cartesian::NORTH) worldPos.y += 1.0f;
            const f32v2 offset = f32v2(CARTESIAN_EDGE_DIRS_ABS[e_cast(edge.dir)]) * (f32)(edge.edgeLength/* + 1.0f*/);
            f32v3 pointA = f32v3(worldPos) + f32v3(0.00f, 0.00f, 0.05f);
            f32v3 pointB = f32v3(worldPos) + f32v3(offset.x, offset.y, 0.0f);
            if (heightData) {
                pointA.z = worldGrid.computeHeightAtPoint(patchId, heightData, f32v2(pointA));
                pointB.z = worldGrid.computeHeightAtPoint(patchId, heightData, f32v2(pointB));
            }
            if (edge.isExternalEdge()) {
                DebugRenderer::drawLineBetweenPoints(pointA, pointB, color2, lifetime, debugId);
            }
            else {
                DebugRenderer::drawLineBetweenPoints(pointA, pointB, color1, lifetime, debugId);
            }
            f32v3 midpoint(worldPos.x + offset.x * 0.5f, worldPos.y + offset.y * 0.5f, (pointA.z + pointB.z) * 0.5f);
            if (heightData) {
                midpoint.z = worldGrid.computeHeightAtPoint(patchId, heightData, f32v2(midpoint));
            }
            f32v3 third(midpoint.x + CARTESIAN_NORMALS[e_cast(edge.dir)].x, midpoint.y + CARTESIAN_NORMALS[e_cast(edge.dir)].y, midpoint.z);
            // TODO: Combine above
            DebugRenderer::drawLineBetweenPoints(midpoint, third, color3, lifetime, debugId);

            for (ui32 j = i + 1; j < edgeCount; ++j) {
                const CoarseNavNodeEdge& edge2 = node.edges[j];
                i32v3 startOffset2 = tileContainer.getTileXYZOffsetWithZScale(edge2.startPos);
                if (heightData) {
                    startOffset2.z = worldGrid.computeHeightAtPoint(patchId, heightData, f32v2(containerPos + startOffset2));
                }
                i32v3 worldPos2 = containerPos + startOffset2;
                if (edge2.dir == Cartesian::EAST) worldPos2.x += 1.0f;
                else if (edge2.dir == Cartesian::NORTH) worldPos2.y += 1.0f;
                const f32v2 offset2 = f32v2(CARTESIAN_EDGE_DIRS_ABS[e_cast(edge2.dir)]) * (f32)(edge2.edgeLength/* + 1.0f*/);
                f32v3 midpoint2(worldPos2.x + offset2.x * 0.5f, worldPos2.y + offset2.y * 0.5f, worldPos2.z);
                if (heightData) {
                    midpoint2.z = worldGrid.computeHeightAtPoint(patchId, heightData, f32v2(midpoint2));
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

constexpr ui32 COARSE_NAV_COLOR_COUNT = 16;
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
    color4(0.0f, 0.0f, 0.0f, COARSE_NAV_COLOR_ALPHA),
    color4(0.0f, 0.0f, 0.0f, COARSE_NAV_COLOR_ALPHA),
    color4(0.0f, 0.0f, 0.0f, COARSE_NAV_COLOR_ALPHA),
    color4(0.0f, 0.0f, 0.0f, COARSE_NAV_COLOR_ALPHA),
    color4(0.0f, 0.0f, 0.0f, COARSE_NAV_COLOR_ALPHA),
    color4(0.0f, 0.0f, 0.0f, COARSE_NAV_COLOR_ALPHA),
    color4(0.0f, 0.0f, 0.0f, COARSE_NAV_COLOR_ALPHA),
    color4(0.0f, 0.0f, 0.0f, COARSE_NAV_COLOR_ALPHA),
};

void NavWorld::debugDrawFineNavGraphForContainer(const TileContainer& tileContainer, ui32 lifetime, int debugId /*= 0*/) const
{
    const WorldGrid& worldGrid = mWorld.getWorldGrid();
    const f32* heightData = nullptr;
    HeightmapPatchID patchId(f32v2(tileContainer.getWorldPos2D()));
    if (tileContainer.isTerrain()) {
        heightData = worldGrid.getHeightDataAt(patchId)->data;
    }
    constexpr f32 EDGE_SIZE = 0.2f;
    ui32 tileIndex = 0;
    const auto& tiles = tileContainer.getTiles();
    const auto& fineNavData = tileContainer.getFineNavData();
    const color4 interiorColor = color4(0.0f, 1.0f, 0.0f, 0.7f);
    const color4 exteriorColor = color4(1.0f, 0.0f, 0.0f, 0.7f);
    const color4 whiteColor(1.0f, 1.0f, 1.0f, 0.3f);

    for (ui32 tileIndex = 0; tileIndex < tiles.size(); ++tileIndex) {
        if (tileContainer.isTileOwned(tileIndex)) {
            f32v3 worldPos = f32v3(tileContainer.getTileXYZOffsetWithZScale(tileIndex) + tileContainer.getWorldPos3D());
            const f32v3 centerPos = worldPos + f32v3(0.5f, 0.5f, 0.0f);
            const TileFineNavData& navData = fineNavData[tileIndex];
            const ui32 colorIndex = tiles[tileIndex].getNavNodeIndex_DEBUG_MAIN_THREAD() % COARSE_NAV_COLOR_COUNT;
            DebugRenderer::drawFilledQuad(worldPos, f32v2(1.0f), COARSE_NAV_COLORS[colorIndex], lifetime, debugId);
            for (int dir = 0; dir < 8; ++dir) {
                if (navData.canAccessDirection(Cartesian8(dir))) {
                    f32v3 edgePos = worldPos + FINE_EDGE_OFFSETS[dir];
                    f32v3 offsetPos = edgePos + glm::normalize(centerPos - edgePos) * EDGE_SIZE;
                    if (heightData) {
                        edgePos.z = worldGrid.computeHeightAtPoint(patchId, heightData, f32v2(edgePos));
                        offsetPos.z = worldGrid.computeHeightAtPoint(patchId, heightData, f32v2(offsetPos));
                    }
                    const Cartesian cart4 = CARTESIAN8_TO_CARTESIAN[dir];
                    if (cart4 == Cartesian::NONE || navData.getEdgeType(cart4) != TileFineNavEdgeType::EXTERIOR) {
                        DebugRenderer::drawLineBetweenPoints(edgePos, offsetPos, interiorColor, lifetime, debugId);
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
            if (heightData) {
                worldPos.z = worldGrid.computeHeightAtPoint(patchId, heightData, f32v2(worldPos));
                worldPos2.z = worldGrid.computeHeightAtPoint(patchId, heightData, f32v2(worldPos2));
                worldPos3.z = worldGrid.computeHeightAtPoint(patchId, heightData, f32v2(worldPos3));
                worldPos4.z = worldGrid.computeHeightAtPoint(patchId, heightData, f32v2(worldPos4));
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
    if (!tileHandle.isValid() || tileHandle.tile->getNavNodeIndex_DEBUG_MAIN_THREAD() == INVALID_NAV_NODE_INDEX) {
        return;
    }
    const color4 color1(0.0f, 1.0f, 1.0f, 0.75f);
    const color4 color2(1.0f, 0.0f, 0.0f, 0.75f);
    const color4 color3(1.0f, 1.0f, 1.0f, 0.75f);
    const color4 color4(1.0f, 0.0f, 1.0f, 0.75f);
    const WorldGrid& worldGrid = mWorld.getWorldGrid();
    const TileContainerID containerId = tileHandle.container->getId();
    const f32* heightData = nullptr;
    HeightmapPatchID patchId(f32v2(tileHandle.container->getWorldPos2D()));
    if (tileHandle.container->isTerrain()) {
        heightData = worldGrid.getHeightDataAt(patchId)->data;
    }
    auto&& it = mNavGraphs.find(containerId);
    if (it == mNavGraphs.end()) {
        std::cout << "Failed to find navgraph for container " << containerId << std::endl;
        return;
    }
    const CoarseNavGraph& graph = it->second;
    const CoarseNavNode& node = graph.nodes[tileHandle.tile->getNavNodeIndex_DEBUG_MAIN_THREAD()];
    const i32v3& containerPos = tileHandle.container->getWorldPos3D();
    const ui32 edgeCount = node.edgeCount;
    for (ui32 i = 0; i < edgeCount; ++i) {
        const CoarseNavNodeEdge& edge = node.edges[i];
        i32v3 startOffset = tileHandle.container->getTileXYZOffsetWithZScale(edge.startPos);
        // TODO: Remove
        if (heightData) {
            startOffset.z = worldGrid.computeHeightAtPoint(patchId, heightData, f32v2(containerPos + startOffset));
        }
        i32v3 worldPos = containerPos + startOffset;
        if (edge.dir == Cartesian::EAST) worldPos.x += 1.0f;
        else if (edge.dir == Cartesian::NORTH) worldPos.y += 1.0f;
        const f32v2 offset = f32v2(CARTESIAN_EDGE_DIRS_ABS[e_cast(edge.dir)]) * (f32)(edge.edgeLength/* + 1.0f*/);
        f32v3 pointA = f32v3(worldPos) + f32v3(0.00f, 0.00f, 0.05f);
        f32v3 pointB = f32v3(worldPos) + f32v3(offset.x, offset.y, 0.0f);
        if (heightData) {
            pointA.z = worldGrid.computeHeightAtPoint(patchId, heightData, f32v2(pointA));
            pointB.z = worldGrid.computeHeightAtPoint(patchId, heightData, f32v2(pointB));
        }
        if (edge.isExternalEdge()) {
            DebugRenderer::drawLineBetweenPoints(pointA, pointB, color2, lifetime, debugId);
        }
        else {
            DebugRenderer::drawLineBetweenPoints(pointA, pointB, color1, lifetime, debugId);
        }
        f32v3 midpoint(worldPos.x + offset.x * 0.5f, worldPos.y + offset.y * 0.5f, (pointA.z + pointB.z) * 0.5f);
        if (heightData) {
            midpoint.z = worldGrid.computeHeightAtPoint(patchId, heightData, f32v2(midpoint));
        }
        f32v3 third(midpoint.x + CARTESIAN_NORMALS[e_cast(edge.dir)].x, midpoint.y + CARTESIAN_NORMALS[e_cast(edge.dir)].y, midpoint.z);
        // TODO: Combine above
        DebugRenderer::drawLineBetweenPoints(midpoint, third, color3, lifetime, debugId);

        for (ui32 j = i + 1; j < edgeCount; ++j) {
            const CoarseNavNodeEdge& edge2 = node.edges[j];
            i32v3 startOffset2 = tileHandle.container->getTileXYZOffsetWithZScale(edge2.startPos);
            if (heightData) {
                startOffset2.z = worldGrid.computeHeightAtPoint(patchId, heightData, f32v2(containerPos + startOffset2));
            }
            i32v3 worldPos2 = containerPos + startOffset2;
            if (edge2.dir == Cartesian::EAST) worldPos2.x += 1.0f;
            else if (edge2.dir == Cartesian::NORTH) worldPos2.y += 1.0f;
            const f32v2 offset2 = f32v2(CARTESIAN_EDGE_DIRS_ABS[e_cast(edge2.dir)]) * (f32)(edge2.edgeLength/* + 1.0f*/);
            f32v3 midpoint2(worldPos2.x + offset2.x * 0.5f, worldPos2.y + offset2.y * 0.5f, worldPos2.z);
            if (heightData) {
                midpoint2.z = worldGrid.computeHeightAtPoint(patchId, heightData, f32v2(midpoint2));
            }
            DebugRenderer::drawLineBetweenPoints(midpoint, midpoint2, color4, lifetime, debugId);
        }
    }
}

//
//void NavWorld::buildEdges(TileContainer& tileContainer, const int cornerX, const int cornerY, const int zPos, const i32v2& subchunkDims, TileIndex cornerIndex, DisjointSetNode* djNodes, ui32* djNodeIDs, CoarseNavNodeIndex* navNodeIdTable, std::vector<CoarseNavNode>& navNodes, Cartesian dir)
//{
//    const std::vector<Tile>& tiles = tileContainer.getTiles();
//    ui32 currNodeId;
//    i32v2 start(0);
//    int length = 0;
//    i32v2 subChunkRelativePos = CARTESIAN_EDGE_INDEX_OFFSET_MULTS[e_cast(dir)] * (subchunkDims - 1);
//    ui32 prevNodeId = djNodes[subChunkRelativePos.y * SUBCHUNK_WIDTH + subChunkRelativePos.x].id;
//    i32v2 containerRelativePos(cornerX, cornerY);
//    i32v2 adjWorldPos = tileContainer.getWorldPos2D() + i32v2(cornerX + CARTESIAN_NORMALS[e_cast(dir)].x, cornerY + CARTESIAN_NORMALS[e_cast(dir)].y);
//
//    ui16 edgeBits = 0;
//    int axis = CARTESIAN_EDGEWALK_AXIS[e_cast(dir)];
//    for (int i = 0; i < subchunkDims[axis]; ++i) {
//        TileIndex index = tileContainer.getTileIndexFromXYZOffset(containerRelativePos.x, containerRelativePos.y, zPos);
//        const Tile& tile = tiles[index];
//        const ui32 djIndex = subChunkRelativePos.y * SUBCHUNK_WIDTH + subChunkRelativePos.x;
//        currNodeId = djNodes[djNodeIDs[djIndex]].id;
//        // Check if we have an edge break
//        if (currNodeId != prevNodeId) {
//            // Finish edge
//            if (length != 0) {
//                addNodeEdge(tileContainer, navNodeIdTable, prevNodeId, navNodes, cornerIndex, tileContainer.getTileIndexFromXYZOffset(start.x, start.y, zPos), length, dir);
//                length = 0;
//            }
//            prevNodeId = currNodeId;
//        }
//        const bool isImpassable = tile.hasFlagThreadSafe(TileFlags::TILE_FLAG_IS_IMPASSABLE);
//        if (!isImpassable) {
//            edgeBits |= 1 << i;
//        }
//
//        if (true/*if we dont have an edge break*/) {
//            // Start new edge
//            if (length == 0) {
//                start = containerRelativePos;
//            }
//            // Extend edge length
//            ++length;
//        }
//        else if (length != 0) {
//            addNodeEdge(tileContainer, navNodeIdTable, currNodeId, navNodes, cornerIndex, tileContainer.getTileIndexFromXYZOffset(start.x, start.y, zPos), length, dir);
//            length = 0;
//        }
//        const i32v2& edgeDir = CARTESIAN_EDGE_DIRS_ABS[e_cast(dir)];
//        adjWorldPos += edgeDir;
//        containerRelativePos += edgeDir;
//        subChunkRelativePos += edgeDir;
//    }
//    // Add final edge if we reached end
//    if (length != 0) {
//        addNodeEdge(tileContainer, navNodeIdTable, currNodeId, navNodes, cornerIndex, tileContainer.getTileIndexFromXYZOffset(start.x, start.y, zPos), length, dir);
//    }
//}
//
//void NavWorld::addNodeEdge(TileContainer& tileContainer, CoarseNavNodeIndex* navNodeIdTable, const ui32 djIndex, std::vector<CoarseNavNode>& navNodes, TileIndex corner, TileIndex start, int length, Cartesian dir) {
//    CoarseNavNode* currNavNode;
//    // Add nav node if it doesnt exist yet
//    ui16& navNodeId = navNodeIdTable[djIndex];
//    if (navNodeId == INVALID_NAV_NODE_INDEX) {
//        navNodeId = (ui16)navNodes.size();
//        currNavNode = &navNodes.emplace_back();
//        currNavNode->tileContainerID = tileContainer.getId();
//        currNavNode->cornerPos = corner;
//        currNavNode->numSouth = 0;
//        currNavNode->numWest = 0;
//        currNavNode->numEast = 0;
//        currNavNode->numNorth = 0;
//        currNavNode->isClosed = false;
//        currNavNode->width = SUBCHUNK_WIDTH;
//        currNavNode->depth = SUBCHUNK_WIDTH;
//    }
//    else {
//        currNavNode = &navNodes[navNodeId];
//        assert(corner == currNavNode->cornerPos);
//    }
//    ui8& currCount = currNavNode->counts[e_cast(dir)];
//    assert(currCount <= 8);
//    assert(length > 0 && length <= 16);
//
//    // Add edge
//    LiteCoarseNavNodeEdge& edge = currNavNode->edges[e_cast(dir)][currCount++];
//    edge.lengthMinusOne = length - 1;
//
//    // Because dir is separated into separate arrays, and is always along the subchunk boundary, we can encode where the start is along a 0-15 integer (4 byte)
//    const ui32v2 cornerXYOffset = tileContainer.getTileXYOffset(corner);
//    const ui32v2 startXYOffset = tileContainer.getTileXYOffset(start);
//    switch (dir) {
//        case Cartesian::WEST:
//        case Cartesian::EAST: {
//            assert(startXYOffset.y >= cornerXYOffset.y);
//            int offsety = startXYOffset.y - cornerXYOffset.y;
//            assert(offsety < 16);
//            edge.start = offsety;
//        }
//        break;
//        case Cartesian::SOUTH:
//        case Cartesian::NORTH: {
//            assert(startXYOffset.x >= cornerXYOffset.x);
//            int offsetX = startXYOffset.x - cornerXYOffset.x;
//            assert(offsetX < 16);
//            edge.start = offsetX;
//        }
//        break;
//    }
//}