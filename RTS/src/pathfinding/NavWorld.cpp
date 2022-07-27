#include "stdafx.h"
#include "NavWorld.h"

#include "World.h"
#include "world/Chunk.h"
#include "DebugRenderer.h"
#include "options/DebugOptions.h"

#include "debugging/VisualLogger.h"

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

struct DisjointSetNode {
    ui32 id;
};

NavWorld::NavWorld(World& world) : mWorld(world)
{

}

void NavWorld::buildNavGraphForContainer(TileContainer& tileContainer) {

    VisualLog* visLog = VisualLogger::tryGetNewVisualLog("Nav Graph");

    // Nav thread only
    assert(IS_NAV_THREAD());

    //ScopedTimer timer("Built nav graph");
    const i32v3& dims = tileContainer.getDims();
    const i32v2 subchunkCount(ceil((f32)dims.x / SUBCHUNK_WIDTH), ceil((f32)dims.y / SUBCHUNK_WIDTH));

    std::vector<CoarseNavNode> navNodes;
    navNodes.reserve(subchunkCount.x * subchunkCount.y * tileContainer.getDims().z);

    Chunk* chunk = nullptr;
    if (tileContainer.isTerrain()) {
        chunk = &mWorld.getWorldGrid().getChunk(ChunkID(f32v2(tileContainer.getWorldPos2D())));
    }

    // TODO: Separate internal with border chunks for faster lookups??
    // Iterate through sub chunks
    const std::vector<Tile>& tiles = tileContainer.getTiles();
    for (int sz = 0; sz < tileContainer.getDims().z; ++sz) {
        //const int zIndexStart = sz * dims.x * dims.y;
        for (int sy = 0; sy < subchunkCount.y; ++sy) {
            const int cornerY = sy * SUBCHUNK_WIDTH;
            for (int sx = 0; sx < subchunkCount.x; ++sx) {
                const int cornerX = sx * SUBCHUNK_WIDTH;
                int subchunkWidth = SUBCHUNK_WIDTH;
                int subchunkDepth = SUBCHUNK_WIDTH;
                // Last subchunk might be smaller
                if (sx == subchunkCount.x - 1) {
                    subchunkWidth = SUBCHUNK_WIDTH - (cornerX + SUBCHUNK_WIDTH - dims.x);
                }
                if (sy == subchunkCount.y - 1) {
                    subchunkDepth = SUBCHUNK_WIDTH - (cornerY + SUBCHUNK_WIDTH - dims.y);
                }
                assert(subchunkDepth);
                assert(subchunkWidth);
                // Find the nav nodes for each sub chunk
                DisjointSetNode djNodes[SUBCHUNK_WIDTH_SQ];
                ui32 djNodeIDs[SUBCHUNK_WIDTH_SQ];
                ui32 totalSets = 0;
                // Iterate internally to find disjoint sets
                for (int y = 0; y < subchunkDepth; ++y) {
                    for (int x = 0; x < subchunkWidth; ++x) {
                        bool assigned = false;
                        const int djArryIndex = y * SUBCHUNK_WIDTH + x;
                        const TileIndex index = tileContainer.getTileIndexFromXYZOffset(cornerX + x, cornerY + y, sz);
                        // Structures block navgraph
                        StructureArrayPtr structures = chunk ? chunk->getStructuresAtThreadSafe(index) : StructureArrayPtr{ nullptr, 0 };
                        if (!structures.first) {
                            const Tile& tile = tiles[index];
                            const f32 groundZPosition = tile.getGroundZPositionUncompressedThreadSafe();

                            if (x != 0) {
                                const Tile& left = tiles[index - 1];
                                // Check if we can cross between
                                if (abs(left.getGroundZPositionUncompressedThreadSafe() - groundZPosition) < 2.0f) {
                                    djNodeIDs[djArryIndex] = djNodeIDs[djArryIndex - 1];
                                    assigned = true;
                                }
                            }
                            if (y != 0) {
                                const Tile& bottom = tiles[index - dims.x];
                                // Check if we can cross between
                                if (abs(bottom.getGroundZPositionUncompressedThreadSafe() - groundZPosition) < 2.0f) {
                                    if (assigned) {
                                        // If we already assigned to left, merge the sets
                                        ui32 prevID = djNodeIDs[djArryIndex];
                                        ui32 botID = djNodeIDs[djArryIndex - SUBCHUNK_WIDTH];
                                        djNodes[prevID].id = djNodes[botID].id;
                                    }
                                    else {
                                        djNodeIDs[djArryIndex] = djNodeIDs[djArryIndex - SUBCHUNK_WIDTH];
                                        assigned = true;
                                    }
                                }
                            }
                        }
                        // If we haven't been joined, make a new node
                        if (!assigned) {
                            djNodeIDs[djArryIndex] = totalSets;
                            djNodes[totalSets] = { totalSets };
                            ++totalSets;
                        }
                    }
                }
                // Now, iterate through the edges to produce nav nodes with edge information
                CoarseNavNodeIndex navNodeIdTable[SUBCHUNK_WIDTH_SQ];
                memset(navNodeIdTable, 0xffui8, sizeof(ui16) * SUBCHUNK_WIDTH_SQ);

                const TileIndex cornerIndex = tileContainer.getTileIndexFromXYZOffset(cornerX, cornerY, sz);
                const i32v2 subchunkDims(subchunkWidth, subchunkDepth);
                buildEdges(tileContainer, cornerX, cornerY, sz, subchunkDims, cornerIndex, djNodes, djNodeIDs, navNodeIdTable, navNodes, Cartesian::SOUTH);
                buildEdges(tileContainer, cornerX, cornerY, sz, subchunkDims, cornerIndex, djNodes, djNodeIDs, navNodeIdTable, navNodes, Cartesian::WEST);
                buildEdges(tileContainer, cornerX + subchunkWidth - 1, cornerY, sz, subchunkDims, cornerIndex, djNodes, djNodeIDs, navNodeIdTable, navNodes, Cartesian::EAST);
                buildEdges(tileContainer, cornerX, cornerY + subchunkDepth - 1, sz, subchunkDims, cornerIndex, djNodes, djNodeIDs, navNodeIdTable, navNodes, Cartesian::NORTH);

                // Update all nav indices
                for (int y = 0; y < subchunkDepth; ++y) {
                    const int cornerY = sy * SUBCHUNK_WIDTH;
                    for (int x = 0; x < subchunkWidth; ++x) {
                        const int cornerX = sx * SUBCHUNK_WIDTH;
                        TileIndex index = tileContainer.getTileIndexFromXYZOffset(cornerX + x, cornerY + y, sz);
                        const Tile& tile = tiles[index];
                        const ui32 djIndex = y * SUBCHUNK_WIDTH + x;
                        const ui32 navTableId = djNodes[djNodeIDs[djIndex]].id;
                        const CoarseNavNodeIndex navNodeIndex = navNodeIdTable[navTableId];
                        tile.setNavNodeIndex(navNodeIndex);
                    }
                }
            }
        }
    }
    // Iterate through outer sub chunks (has chunk neighbors)
    // TODO: what? did I forget to do this

    // Build nav list as static array
    CoarseNavGraph& patch = mNavGraphs[tileContainer.getId()];

    // If we have old nav data, delete it
    if (patch.nodes) {
        delete[] patch.nodes;
    }

    if (navNodes.size()) {
        patch.size = (ui32)navNodes.size();
        patch.nodes = new CoarseNavNode[patch.size];
        for (int i = 0; i < patch.size; ++i) {
            patch.nodes[i] = std::move(navNodes[i]);
        }
    }
    else {
        patch.nodes = nullptr;
        patch.size = 0;
    }
}

void NavWorld::debugDrawNavGraphForContainer(const TileContainer& tileContainer, ui32 lifetime, int debugId /*= 0*/) const
{
    const color4 color1(0.0f, 1.0f, 1.0f, 0.75f);
    const color4 color2(1.0f, 0.0f, 0.0f, 0.75f);
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
    const CoarseNavGraph& patch = it->second;
    // Draw edges
    for (ui32 nodeIndex = 0; nodeIndex < patch.size; ++nodeIndex) {
        const CoarseNavNode& node = patch.nodes[nodeIndex];
        const f32v2 cornerWorldPos = f32v2(tileContainer.getWorldPos2D()) + f32v2(tileContainer.getTileXYOffset(node.cornerPos));
        for (ui32 cartesian = 0; cartesian < 4; ++cartesian) {
            const ui32 edgeCount = node.counts[cartesian];
            for (ui32 i = 0; i < edgeCount; ++i) {
                const LiteCoarseNavNodeEdge& edge = node.edges[cartesian][i];
                const f32v2 edgeOffset = f32v2(CARTESIAN_EDGE_DIRS_ABS[cartesian]) * (f32)edge.start + f32v2(NAV_NODE_EDGE_OFFSETS[cartesian]);
                f32v2 cornerPos = cornerWorldPos + edgeOffset;
                if (cartesian == (ui32)Cartesian::EAST) cornerPos.x += 1.0f;
                else if (cartesian == (ui32)Cartesian::NORTH) cornerPos.y += 1.0f;
                const f32v2 offset = f32v2(CARTESIAN_EDGE_DIRS_ABS[cartesian]) * (f32)(edge.lengthMinusOne + 1.0f);
                const f32v3 pointA = helperGet3DPoint(worldGrid, patchId, heightData, cornerPos);
                const f32v3 pointB = helperGet3DPoint(worldGrid, patchId, heightData, cornerPos + offset);
                DebugRenderer::drawLineBetweenPoints(pointA, pointB, color1, lifetime, debugId);
                const f32v3 second(cornerPos.x + offset.x * 0.5f, cornerPos.y + offset.y * 0.5f, (pointA.z + pointB.z) * 0.5f);
                const f32v3 third(second.x + CARTESIAN_NORMALS[cartesian].x, second.y + CARTESIAN_NORMALS[cartesian].y, second.z);
                DebugRenderer::drawLineBetweenPoints(second, third, color1, lifetime, debugId);
            }
        }
    }
    // Draw connections between edges
    for (int nodeIndex = 0; nodeIndex < patch.size; ++nodeIndex) {
        const CoarseNavNode& node = patch.nodes[nodeIndex];
        const f32v2 cornerWorldPos = f32v2(tileContainer.getWorldPos2D()) + f32v2(tileContainer.getTileXYOffset(node.cornerPos));
        for (ui32 cartesian = 0; cartesian < 4; ++cartesian) {
            const ui32 edgeCount = node.counts[cartesian];
            for (ui32 i = 0; i < edgeCount; ++i) {
                const LiteCoarseNavNodeEdge& edge1 = node.edges[cartesian][i];
                const f32v2 edgeOffset1 = f32v2(CARTESIAN_EDGE_DIRS_ABS[cartesian]) * (f32)edge1.start + f32v2(NAV_NODE_EDGE_OFFSETS[cartesian]);
                f32v2 cornerPos1 = cornerWorldPos + edgeOffset1;
                const f32v2 offset1 = f32v2(CARTESIAN_EDGE_DIRS_ABS[cartesian]) * (f32)(edge1.lengthMinusOne + 1.0f);
                if (cartesian == (ui32)Cartesian::EAST) cornerPos1.x += 1.0f;
                else if (cartesian == (ui32)Cartesian::NORTH) cornerPos1.y += 1.0f;
                const f32v2 pos1 = cornerPos1 + offset1 * 0.5f;
                const f32v3 pointA = helperGet3DPoint(worldGrid, patchId, heightData, pos1);
                // Connect to our side
                for (ui32 j = i + 1; j < edgeCount; ++j) {
                    const LiteCoarseNavNodeEdge& edge2 = node.edges[cartesian][j];
                    const f32v2 edgeOffset2 = f32v2(CARTESIAN_EDGE_DIRS_ABS[cartesian]) * (f32)edge2.start + f32v2(NAV_NODE_EDGE_OFFSETS[cartesian]);
                    f32v2 cornerPos2 = cornerWorldPos + edgeOffset2;
                    const f32v2 offset2 = f32v2(CARTESIAN_EDGE_DIRS_ABS[cartesian]) * (f32)(edge2.lengthMinusOne + 1.0f);
                    if (cartesian == (ui32)Cartesian::EAST) cornerPos2.x += 1.0f;
                    else if (cartesian == (ui32)Cartesian::NORTH) cornerPos2.y += 1.0f;
                    const f32v2 pos2 = cornerPos2 + offset2 * 0.5f;
                    DebugRenderer::drawLineBetweenPoints(pointA, helperGet3DPoint(worldGrid, patchId, heightData, pos2), color2, lifetime, debugId);
                }

                // Connect to all other sides
                for (ui32 cartesian2 = cartesian + 1; cartesian2 < 4; ++cartesian2) {
                    const ui32 edgeCount2 = node.counts[cartesian2];
                    for (ui32 j = 0; j < edgeCount2; ++j) {
                        const LiteCoarseNavNodeEdge& edge2 = node.edges[cartesian2][j];
                        const f32v2 edgeOffset2 = f32v2(CARTESIAN_EDGE_DIRS_ABS[cartesian2]) * (f32)edge2.start + f32v2(NAV_NODE_EDGE_OFFSETS[cartesian2]);
                        f32v2 cornerPos2 = cornerWorldPos + edgeOffset2;
                        const f32v2 offset2 = f32v2(CARTESIAN_EDGE_DIRS_ABS[cartesian2]) * (f32)(edge2.lengthMinusOne + 1.0f);
                        if (cartesian2 == (ui32)Cartesian::EAST) cornerPos2.x += 1.0f;
                        else if (cartesian2 == (ui32)Cartesian::NORTH) cornerPos2.y += 1.0f;
                        const f32v2 pos2 = cornerPos2 + offset2 * 0.5f;
                        DebugRenderer::drawLineBetweenPoints(pointA, helperGet3DPoint(worldGrid, patchId, heightData, pos2), color2, lifetime, debugId);
                    }
                }
            }
        }
    }
}

void NavWorld::buildEdges(TileContainer& tileContainer, const int cornerX, const int cornerY, const int zPos, const i32v2& subchunkDims, TileIndex cornerIndex, DisjointSetNode* djNodes, ui32* djNodeIDs, CoarseNavNodeIndex* navNodeIdTable, std::vector<CoarseNavNode>& navNodes, Cartesian dir)
{
    const std::vector<Tile>& tiles = tileContainer.getTiles();
    ui32 currNodeId;
    i32v2 start(0);
    int length = 0;
    i32v2 subChunkRelativePos = CARTESIAN_EDGE_INDEX_OFFSET_MULTS[e_cast(dir)] * (subchunkDims - 1);
    ui32 prevNodeId = djNodes[subChunkRelativePos.y * SUBCHUNK_WIDTH + subChunkRelativePos.x].id;
    i32v2 containerRelativePos(cornerX, cornerY);
    i32v2 adjWorldPos = tileContainer.getWorldPos2D() + i32v2(cornerX + CARTESIAN_NORMALS[e_cast(dir)].x, cornerY + CARTESIAN_NORMALS[e_cast(dir)].y);

    ui16 edgeBits = 0;
    int axis = CARTESIAN_EDGEWALK_AXIS[e_cast(dir)];
    for (int i = 0; i < subchunkDims[axis]; ++i) {
        TileIndex index = tileContainer.getTileIndexFromXYZOffset(containerRelativePos.x, containerRelativePos.y, zPos);
        const Tile& tile = tiles[index];
        const ui32 djIndex = subChunkRelativePos.y * SUBCHUNK_WIDTH + subChunkRelativePos.x;
        currNodeId = djNodes[djNodeIDs[djIndex]].id;
        // Check if we have an edge break
        if (currNodeId != prevNodeId) {
            // Finish edge
            if (length != 0) {
                addNodeEdge(tileContainer, navNodeIdTable, prevNodeId, navNodes, cornerIndex, tileContainer.getTileIndexFromXYZOffset(start.x, start.y, zPos), length, dir);
                length = 0;
            }
            prevNodeId = currNodeId;
        }
        const bool isImpassable = tile.hasFlagThreadSafe(TileFlags::TILE_FLAG_IS_IMPASSABLE);
        if (!isImpassable) {
            edgeBits |= 1 << i;
        }

        if (true/*if we dont have an edge break*/) {
            // Start new edge
            if (length == 0) {
                start = containerRelativePos;
            }
            // Extend edge length
            ++length;
        }
        else if (length != 0) {
            addNodeEdge(tileContainer, navNodeIdTable, currNodeId, navNodes, cornerIndex, tileContainer.getTileIndexFromXYZOffset(start.x, start.y, zPos), length, dir);
            length = 0;
        }
        const i32v2& edgeDir = CARTESIAN_EDGE_DIRS_ABS[e_cast(dir)];
        adjWorldPos += edgeDir;
        containerRelativePos += edgeDir;
        subChunkRelativePos += edgeDir;
    }
    // Add final edge if we reached end
    if (length != 0) {
        addNodeEdge(tileContainer, navNodeIdTable, currNodeId, navNodes, cornerIndex, tileContainer.getTileIndexFromXYZOffset(start.x, start.y, zPos), length, dir);
    }
}

void NavWorld::addNodeEdge(TileContainer& tileContainer, CoarseNavNodeIndex* navNodeIdTable, const ui32 djIndex, std::vector<CoarseNavNode>& navNodes, TileIndex corner, TileIndex start, int length, Cartesian dir) {
    CoarseNavNode* currNavNode;
    // Add nav node if it doesnt exist yet
    ui16& navNodeId = navNodeIdTable[djIndex];
    if (navNodeId == INVALID_NAV_NODE_INDEX) {
        navNodeId = (ui16)navNodes.size();
        currNavNode = &navNodes.emplace_back();
        currNavNode->tileContainerID = tileContainer.getId();
        currNavNode->cornerPos = corner;
        currNavNode->numSouth = 0;
        currNavNode->numWest = 0;
        currNavNode->numEast = 0;
        currNavNode->numNorth = 0;
        currNavNode->isClosed = false;
        currNavNode->width = SUBCHUNK_WIDTH;
        currNavNode->depth = SUBCHUNK_WIDTH;
    }
    else {
        currNavNode = &navNodes[navNodeId];
        assert(corner == currNavNode->cornerPos);
    }
    ui8& currCount = currNavNode->counts[e_cast(dir)];
    assert(currCount <= 8);
    assert(length > 0 && length <= 16);

    // Add edge
    LiteCoarseNavNodeEdge& edge = currNavNode->edges[e_cast(dir)][currCount++];
    edge.lengthMinusOne = length - 1;

    // Because dir is separated into separate arrays, and is always along the subchunk boundary, we can encode where the start is along a 0-15 integer (4 byte)
    const ui32v2 cornerXYOffset = tileContainer.getTileXYOffset(corner);
    const ui32v2 startXYOffset = tileContainer.getTileXYOffset(start);
    switch (dir) {
        case Cartesian::WEST:
        case Cartesian::EAST: {
            assert(startXYOffset.y >= cornerXYOffset.y);
            int offsety = startXYOffset.y - cornerXYOffset.y;
            assert(offsety < 16);
            edge.start = offsety;
        }
        break;
        case Cartesian::SOUTH:
        case Cartesian::NORTH: {
            assert(startXYOffset.x >= cornerXYOffset.x);
            int offsetX = startXYOffset.x - cornerXYOffset.x;
            assert(offsetX < 16);
            edge.start = offsetX;
        }
        break;
    }
}