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

constexpr ui32 INVALID_DJ_NODE_ID = UINT32_MAX;

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


// toDir = south means we enter from the north
bool canEnterTileInDirection(const Tile& tile, const TileWalls& walls, Cartesian toDir) {
    if (!tile.hasFlagThreadSafe(TileFlags::TILE_FLAG_IS_IMPASSABLE)) {
        const Cartesian fromDir = CARTESIAN_OPPOSITES[e_cast(toDir)];
        return !walls.walls[e_cast(fromDir)].isValid();
    }
    return false;
}

// ALGORITHM DESCRIPTION
// 1. Build disjoint sets, with a specific grid size for a maximum disjoint set node width
// 2. Create nav nodes from distinct disjoint set nodes
// 3. Create edges between disjoint set nodes and each other, or "External" which means it connects to the outside world
void NavWorld::buildNavGraphForContainer(TileContainer& tileContainer) {

    VisualLog* visLog = VisualLogger::tryGetNewVisualLog("Nav Graph");

    // Nav thread only
    assert(IS_NAV_THREAD());

    //ScopedTimer timer("Built nav graph");
    const i32v3& dims = tileContainer.getDims();

    Chunk* chunk = nullptr;
    if (tileContainer.isTerrain()) {
        chunk = &mWorld.getWorldGrid().getChunk(ChunkID(f32v2(tileContainer.getWorldPos2D())));
    }

    std::vector<DisjointSetNode> djNodes;
    std::vector<ui32> tileDjNodeIDs;
    ui32 totalDjSets = 0;

    const ui32 GRID_WIDTH = SUBCHUNK_WIDTH;

    // ================= Create disjoint set and fine nav data =================
    const std::vector<Tile>& tiles = tileContainer.getTiles();
    const std::vector<TileWallContainer>& tileWallContainers = tileContainer.getTileWallContainers();
    std::vector<TileFineNavData>& fineNavDataArray = tileContainer.mFineNavData;

    tileDjNodeIDs.resize(tiles.size(), INVALID_DJ_NODE_ID);
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
                if (tile.hasFlagThreadSafe(TileFlags::TILE_FLAG_IS_IMPASSABLE)) {
                    continue;
                }

                const int gridXOffset = tx % GRID_WIDTH;

                const f32 groundZPosition = tile.getGroundZPositionUncompressedThreadSafe();
                bool assigned = false;

                // ================= Fine Nav Data =================
                TileFineNavData& fineNavData = fineNavDataArray[index];
                fineNavData.reset();
                // TODO: which tile do we use for path weight?
                fineNavData.pathWeight = TileRepository::getTileData(tile.getLayersThreadSafe()[TILE_LAYER_GROUND]).pathWeight;
                // South
                if (!walls.south.isValid()) {
                    setFineNavEdgeCartesian(index, Cartesian8::SOUTH, ty > 0, tileContainer, groundZPosition, fineNavData);
                }
                // West
                if (!walls.west.isValid()) {
                    setFineNavEdgeCartesian(index, Cartesian8::WEST, tx > 0, tileContainer, groundZPosition, fineNavData);
                }
                // East
                if (!walls.east.isValid()) {
                    setFineNavEdgeCartesian(index, Cartesian8::EAST, tx < dims.x - 1, tileContainer, groundZPosition, fineNavData);
                }
                // North
                if (!walls.north.isValid()) {
                    setFineNavEdgeCartesian(index, Cartesian8::NORTH, ty < dims.y - 1, tileContainer, groundZPosition, fineNavData);
                }

                // TODO: DIAGONALS
                // South West
                // South East
                // North West
                // North East

                // ================= Disjoint Set =================
                if (gridXOffset != 0) {
                    if (fineNavData.canAccessDirection(Cartesian8::WEST)) {
                        tileDjNodeIDs[index] = tileDjNodeIDs[index - 1];
                        assigned = true;
                    }
                }
                if (gridYOffset != 0) {
                    const Tile& bottom = tiles[index - dims.x];
                    // Check if we can cross between
                    if (fineNavData.canAccessDirection(Cartesian8::SOUTH)) {
                        if (assigned) {
                            // If we already assigned to left, merge the sets
                            ui32 prevID = tileDjNodeIDs[index];
                            ui32 botID = tileDjNodeIDs[index - dims.x];
                            djNodes[prevID] = djNodes[botID];
                        }
                        else {
                            tileDjNodeIDs[index] = tileDjNodeIDs[index - dims.x];
                            assigned = true;
                        }
                    }
                }

                // If we haven't been joined, make a new node
                if (!assigned) {
                    tileDjNodeIDs[index] = totalDjSets;
                    djNodes[totalDjSets] = totalDjSets;
                    ++totalDjSets;
                }
            }
        }
    }
    assert(totalDjSets < UINT16_MAX && "If this fails we need to increase maximum nodes or delete the navmesh cause it too big");

    // ================= Allocate nav nodes =================
    CoarseNavGraph& navGraph = mNavGraphs[tileContainer.getId()];
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


    // ================= Assign nav nodes =================
    for (ui32 i = 0; i < tiles.size(); ++i) {
        const ui32 nodeId = tileDjNodeIDs[i];
        tiles[i].setNavNodeIndex(djNodes[nodeId]);
    }
    
    // ================= Build edges =================
    // TODO: Optimize allocate
    std::vector<std::vector<CoarseNavNodeEdge>> nodeEdges;
    nodeEdges.resize(navGraph.numNodes);
    ui32 edgeCount = 0;
    // Lets us know what edges are at a given tile
    std::vector<TileEdgePointer> tileEdgePointers;
    tileEdgePointers.resize(tiles.size(), {});

    for (int tz = 0; tz < dims.z; ++tz) {
        for (int ty = 0; ty < dims.y; ++ty) {
            const int gridYOffset = ty % GRID_WIDTH;
            for (int tx = 0; tx < dims.x; ++tx) {
                // Determine if we own this tile
                const TileIndex index = tileContainer.getTileIndexFromXYZOffset(tx, ty, tz);
                const Tile& tile = tileContainer.getTileAt(index);
                const ui16 navNodeIndex = tile.getNavNodeIndex();
                // This must be part of a nav node
                if (navNodeIndex == INVALID_NAV_NODE_INDEX) {
                    continue;
                }
                
                const TileWalls& walls = tileContainer.getWallsThreadSafe(index);

                edgeCount += (int)tryBuildEdge(walls, index, index - 1, index - dims.x, tileContainer, navNodeIndex, tileEdgePointers, nodeEdges, Cartesian::SOUTH, ty == 0, tx > 0);
                edgeCount += (int)tryBuildEdge(walls, index, index - dims.x, index - 1, tileContainer, navNodeIndex, tileEdgePointers, nodeEdges, Cartesian::WEST, tx == 0, ty > 0);
                edgeCount += (int)tryBuildEdge(walls, index, index - dims.x, index + 1, tileContainer, navNodeIndex, tileEdgePointers, nodeEdges, Cartesian::EAST, tx == dims.x - 1, ty > 0);
                edgeCount += (int)tryBuildEdge(walls, index, index - 1, index + dims.x, tileContainer, navNodeIndex, tileEdgePointers, nodeEdges, Cartesian::NORTH, ty == dims.y - 1, tx > 0);
            }
        }
    }

    // ================= Assign and copy edges =================
    if (edgeCount) {
        navGraph.numEdges = edgeCount;
        // All edges reside in a single memory blob
        // TODO: pool allocate
        navGraph.edges = std::unique_ptr<CoarseNavNodeEdge[]>(new CoarseNavNodeEdge[edgeCount]);
        CoarseNavNodeEdge* edgePtr = navGraph.edges.get();
        for (size_t navNodeIndex = 0; navNodeIndex < nodeEdges.size(); ++navNodeIndex) {
            std::vector<CoarseNavNodeEdge>& edgesToCopy = nodeEdges[navNodeIndex];
            memcpy(edgePtr, edgesToCopy.data(), edgesToCopy.size());
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

}

void NavWorld::setFineNavEdgeCartesian(const TileIndex adjacentIndex, Cartesian8 cartesian8, bool isInner, TileContainer& tileContainer, const f32 groundZPosition, TileFineNavData& fineNavData) {
    const i32v3& dims = tileContainer.getDims();
    const std::vector<Tile>& tiles = tileContainer.getTiles();
    const std::vector<TileWallContainer>& tileWallContainers = tileContainer.getTileWallContainers();
    const Cartesian cartesian = CARTESIAN8_TO_CARTESIAN[e_cast(cartesian8)];
    if (isInner && tileContainer.isTileOwned(adjacentIndex)) {
        // Interior edge
        const Tile& adjacent = tiles[adjacentIndex];
        if (canEnterTileInDirection(adjacent, tileWallContainers[adjacentIndex].wallsThreadSafe, cartesian) &&
            abs(adjacent.getGroundZPositionUncompressedThreadSafe() - groundZPosition) < 2.0f) {
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

bool NavWorld::tryBuildEdge(const TileWalls& walls, const TileIndex index, const TileIndex prevIndex, const TileIndex outerIndex, TileContainer& tileContainer, const ui16 navNodeIndex, std::vector<TileEdgePointer>& tileEdgePointers, std::vector<std::vector<CoarseNavNodeEdge>>& nodeEdges, const Cartesian dir, bool isBorder, bool canExtendPrevEdge)
{
    // We have an edge only if there is no wall
     // South edge
    bool needNewEdge = true;
    ui16 adjacentNodeIndex = INVALID_NAV_NODE_INDEX;
    if (!walls.walls[e_cast(dir)].isValid()) {
        if (isBorder || !tileContainer.isTileOwned(outerIndex)) {
            // External edge
            if (canExtendPrevEdge) {
                const Tile& prevTile = tileContainer.getTileAt(prevIndex);
                if (prevTile.getNavNodeIndex() == navNodeIndex) {
                    TileEdgePointer& prevEdgePointer = tileEdgePointers[prevIndex];
                    CoarseNavNodeEdge& edge = nodeEdges[navNodeIndex][prevEdgePointer.edges[e_cast(dir)]];
                    ++edge.edgeLength;
                    needNewEdge = false;
                }
            }
        }
        else {
            // Internal edge
            const Tile& outerTile = tileContainer.getTileAt(outerIndex);
            // No edge becase we are the same nav node
            if (outerTile.getNavNodeIndex() == navNodeIndex) {
                return false;
            }
            const TileWalls& outerWalls = tileContainer.getWallsThreadSafe(outerIndex);
            if (canEnterTileInDirection(outerTile, outerWalls, dir)) {
                adjacentNodeIndex = outerTile.getNavNodeIndex();
                // If we can extend prev wall
                if (canExtendPrevEdge) {
                    const Tile& prevTile = tileContainer.getTileAt(prevIndex);
                    if (prevTile.getNavNodeIndex() == navNodeIndex) {
                        TileEdgePointer& prevEdgePointer = tileEdgePointers[prevIndex];
                        CoarseNavNodeEdge& edge = nodeEdges[navNodeIndex][prevEdgePointer.edges[e_cast(dir)]];
                        // Extend only if we are connecting the same DJ nodes
                        if (edge.adjacentNodeIndex == adjacentNodeIndex) {
                            ++edge.edgeLength;
                            needNewEdge = false;
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

void NavWorld::debugDrawNavGraphForContainer(const TileContainer& tileContainer, ui32 lifetime, int debugId /*= 0*/) const
{
    //const color4 color1(0.0f, 1.0f, 1.0f, 0.75f);
    //const color4 color2(1.0f, 0.0f, 0.0f, 0.75f);
    //const WorldGrid& worldGrid = mWorld.getWorldGrid();
    //const TileContainerID containerId = tileContainer.getId();
    //const f32* heightData = nullptr;
    //HeightmapPatchID patchId(f32v2(tileContainer.getWorldPos2D()));
    //if (tileContainer.isTerrain()) {
    //    heightData = worldGrid.getHeightDataAt(patchId)->data;
    //}
    //auto&& it = mNavGraphs.find(containerId);
    //if (it == mNavGraphs.end()) {
    //    std::cout << "Failed to find navgraph for container " << containerId << std::endl;
    //    return;
    //}
    //const CoarseNavGraph& patch = it->second;
    //// Draw edges
    //for (ui32 nodeIndex = 0; nodeIndex < patch.size; ++nodeIndex) {
    //    const CoarseNavNode& node = patch.nodes[nodeIndex];
    //    const f32v2 cornerWorldPos = f32v2(tileContainer.getWorldPos2D()) + f32v2(tileContainer.getTileXYOffset(node.cornerPos));
    //    for (ui32 cartesian = 0; cartesian < 4; ++cartesian) {
    //        const ui32 edgeCount = node.counts[cartesian];
    //        for (ui32 i = 0; i < edgeCount; ++i) {
    //            const LiteCoarseNavNodeEdge& edge = node.edges[cartesian][i];
    //            const f32v2 edgeOffset = f32v2(CARTESIAN_EDGE_DIRS_ABS[cartesian]) * (f32)edge.start + f32v2(NAV_NODE_EDGE_OFFSETS[cartesian]);
    //            f32v2 cornerPos = cornerWorldPos + edgeOffset;
    //            if (cartesian == (ui32)Cartesian::EAST) cornerPos.x += 1.0f;
    //            else if (cartesian == (ui32)Cartesian::NORTH) cornerPos.y += 1.0f;
    //            const f32v2 offset = f32v2(CARTESIAN_EDGE_DIRS_ABS[cartesian]) * (f32)(edge.lengthMinusOne + 1.0f);
    //            const f32v3 pointA = helperGet3DPoint(worldGrid, patchId, heightData, cornerPos);
    //            const f32v3 pointB = helperGet3DPoint(worldGrid, patchId, heightData, cornerPos + offset);
    //            DebugRenderer::drawLineBetweenPoints(pointA, pointB, color1, lifetime, debugId);
    //            const f32v3 second(cornerPos.x + offset.x * 0.5f, cornerPos.y + offset.y * 0.5f, (pointA.z + pointB.z) * 0.5f);
    //            const f32v3 third(second.x + CARTESIAN_NORMALS[cartesian].x, second.y + CARTESIAN_NORMALS[cartesian].y, second.z);
    //            DebugRenderer::drawLineBetweenPoints(second, third, color1, lifetime, debugId);
    //        }
    //    }
    //}
    //// Draw connections between edges
    //for (int nodeIndex = 0; nodeIndex < patch.size; ++nodeIndex) {
    //    const CoarseNavNode& node = patch.nodes[nodeIndex];
    //    const f32v2 cornerWorldPos = f32v2(tileContainer.getWorldPos2D()) + f32v2(tileContainer.getTileXYOffset(node.cornerPos));
    //    for (ui32 cartesian = 0; cartesian < 4; ++cartesian) {
    //        const ui32 edgeCount = node.counts[cartesian];
    //        for (ui32 i = 0; i < edgeCount; ++i) {
    //            const LiteCoarseNavNodeEdge& edge1 = node.edges[cartesian][i];
    //            const f32v2 edgeOffset1 = f32v2(CARTESIAN_EDGE_DIRS_ABS[cartesian]) * (f32)edge1.start + f32v2(NAV_NODE_EDGE_OFFSETS[cartesian]);
    //            f32v2 cornerPos1 = cornerWorldPos + edgeOffset1;
    //            const f32v2 offset1 = f32v2(CARTESIAN_EDGE_DIRS_ABS[cartesian]) * (f32)(edge1.lengthMinusOne + 1.0f);
    //            if (cartesian == (ui32)Cartesian::EAST) cornerPos1.x += 1.0f;
    //            else if (cartesian == (ui32)Cartesian::NORTH) cornerPos1.y += 1.0f;
    //            const f32v2 pos1 = cornerPos1 + offset1 * 0.5f;
    //            const f32v3 pointA = helperGet3DPoint(worldGrid, patchId, heightData, pos1);
    //            // Connect to our side
    //            for (ui32 j = i + 1; j < edgeCount; ++j) {
    //                const LiteCoarseNavNodeEdge& edge2 = node.edges[cartesian][j];
    //                const f32v2 edgeOffset2 = f32v2(CARTESIAN_EDGE_DIRS_ABS[cartesian]) * (f32)edge2.start + f32v2(NAV_NODE_EDGE_OFFSETS[cartesian]);
    //                f32v2 cornerPos2 = cornerWorldPos + edgeOffset2;
    //                const f32v2 offset2 = f32v2(CARTESIAN_EDGE_DIRS_ABS[cartesian]) * (f32)(edge2.lengthMinusOne + 1.0f);
    //                if (cartesian == (ui32)Cartesian::EAST) cornerPos2.x += 1.0f;
    //                else if (cartesian == (ui32)Cartesian::NORTH) cornerPos2.y += 1.0f;
    //                const f32v2 pos2 = cornerPos2 + offset2 * 0.5f;
    //                DebugRenderer::drawLineBetweenPoints(pointA, helperGet3DPoint(worldGrid, patchId, heightData, pos2), color2, lifetime, debugId);
    //            }

    //            // Connect to all other sides
    //            for (ui32 cartesian2 = cartesian + 1; cartesian2 < 4; ++cartesian2) {
    //                const ui32 edgeCount2 = node.counts[cartesian2];
    //                for (ui32 j = 0; j < edgeCount2; ++j) {
    //                    const LiteCoarseNavNodeEdge& edge2 = node.edges[cartesian2][j];
    //                    const f32v2 edgeOffset2 = f32v2(CARTESIAN_EDGE_DIRS_ABS[cartesian2]) * (f32)edge2.start + f32v2(NAV_NODE_EDGE_OFFSETS[cartesian2]);
    //                    f32v2 cornerPos2 = cornerWorldPos + edgeOffset2;
    //                    const f32v2 offset2 = f32v2(CARTESIAN_EDGE_DIRS_ABS[cartesian2]) * (f32)(edge2.lengthMinusOne + 1.0f);
    //                    if (cartesian2 == (ui32)Cartesian::EAST) cornerPos2.x += 1.0f;
    //                    else if (cartesian2 == (ui32)Cartesian::NORTH) cornerPos2.y += 1.0f;
    //                    const f32v2 pos2 = cornerPos2 + offset2 * 0.5f;
    //                    DebugRenderer::drawLineBetweenPoints(pointA, helperGet3DPoint(worldGrid, patchId, heightData, pos2), color2, lifetime, debugId);
    //                }
    //            }
    //        }
    //    }
    //}
    assert(false);
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