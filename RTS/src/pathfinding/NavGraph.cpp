#include "stdafx.h"
#include "NavGraph.h"

#include "World.h"
#include "world/Chunk.h"
#include "DebugRenderer.h"
#include "options/DebugOptions.h"

#include "services/Services.h"

// 0 or 1 for rendering debug
#define DEBUG_RENDER_NAV_NODES 1

inline f32v3 helperGet3DPoint(const WorldGrid& worldGrid, const f32v2& pos2d) {
    return f32v3(pos2d.x, pos2d.y, worldGrid.tryComputeHeightAtPoint(pos2d));
}

inline f32v3 helperGet3DPoint(const WorldGrid& worldGrid, const ChunkID& chunkId, const f32* heightData, const f32v2& pos2d) {
    return f32v3(pos2d.x, pos2d.y, worldGrid.computeHeightAtPoint(chunkId, heightData, pos2d));
}

struct DisjointSetNode {
    ui32 id;
};

NavGraph::NavGraph(World& world) : mWorld(world)
{

}

void NavGraph::buildNavNodesForChunkSynchronous(Chunk& chunk) {
    PreciseTimer timer;

    std::vector<NavNode> navNodes;
    navNodes.reserve(MIN_SUBCHUNKS_PER_CHUNK);

    // TODO: Separate internal with border chunks for faster lookups??
    // Iterate through sub chunks
    std::vector<Tile>& tiles = chunk.mTiles;
    for (int sy = 0; sy < MIN_SUBCHUNKS_PER_CHUNK_ROW; ++sy) {
        const int cornerY = sy * SUBCHUNK_WIDTH;
        for (int sx = 0; sx < MIN_SUBCHUNKS_PER_CHUNK_ROW; ++sx) {
            const int cornerX = sx * SUBCHUNK_WIDTH;

            // Find the nav nodes for each sub chunk
            DisjointSetNode djNodes[SUBCHUNK_WIDTH_SQ];
            ui32 djNodeIDs[SUBCHUNK_WIDTH_SQ];
            ui32 totalSets = 0;
            // Iterate internally to find disjoint sets
            for (int y = 0; y < SUBCHUNK_WIDTH; ++y) {
                for (int x = 0; x < SUBCHUNK_WIDTH; ++x) {
                    TileIndex index(cornerX + x, cornerY + y);
                    Tile& tile = tiles[index];
                    const f32 baseZPosition = tile.getBaseZPositionUncompressed();
                    const int djArryIndex = y * SUBCHUNK_WIDTH + x;
                    bool assigned = false;
                    
                    if (x != 0) {
                        Tile& left = tiles[index - 1];
                        // Check if we can cross between
                        if (abs(left.getBaseZPositionUncompressed() - baseZPosition) < 2.0f) {
                            djNodeIDs[djArryIndex] = djNodeIDs[djArryIndex - 1];
                            assigned = true;
                        }
                    }
                    if (y != 0) {
                        Tile& bottom = tiles[index - CHUNK_WIDTH];
                        // Check if we can cross between
                        if (abs(bottom.getBaseZPositionUncompressed() - baseZPosition) < 2.0f) {
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
                    // If we haven't been joined, make a new node
                    if (!assigned) {
                        djNodeIDs[djArryIndex] = totalSets;
                        djNodes[totalSets] = { totalSets };
                        ++totalSets;
                    }
                }
            }
            // Now, iterate through the edges to produce nav nodes with edge information
            NavNodeIndex navNodeIdTable[SUBCHUNK_WIDTH_SQ];
            memset(navNodeIdTable, 0xffui8, sizeof(ui16) * SUBCHUNK_WIDTH_SQ);

            const TileIndex cornerIndex(cornerX, cornerY);
            buildEdges(chunk, cornerX, cornerY, cornerIndex, djNodes, djNodeIDs, navNodeIdTable, navNodes, Cartesian::DOWN);
            buildEdges(chunk, cornerX, cornerY, cornerIndex, djNodes, djNodeIDs, navNodeIdTable, navNodes, Cartesian::LEFT);
            buildEdges(chunk, cornerX + SUBCHUNK_WIDTH - 1, cornerY, cornerIndex, djNodes, djNodeIDs, navNodeIdTable, navNodes, Cartesian::RIGHT);
            buildEdges(chunk, cornerX, cornerY + SUBCHUNK_WIDTH - 1, cornerIndex, djNodes, djNodeIDs, navNodeIdTable, navNodes, Cartesian::UP);

            // Update all nav indices
            for (int y = 0; y < SUBCHUNK_WIDTH; ++y) {
                const int cornerY = sy * SUBCHUNK_WIDTH;
                for (int x = 0; x < SUBCHUNK_WIDTH; ++x) {
                    const int cornerX = sx * SUBCHUNK_WIDTH;
                    TileIndex index(cornerX + x, cornerY + y);
                    Tile& tile = tiles[index];
                    const ui32 djIndex = y * SUBCHUNK_WIDTH + x;
                    const ui32 navTableId = djNodes[djNodeIDs[djIndex]].id;
                    const NavNodeIndex navNodeIndex = navNodeIdTable[navTableId];
                    tile.navNodeIndex = navNodeIndex;
                }
            }
        }
    }
    // Iterate through outer sub chunks (has chunk neighbors)
    // TODO: what? did I forget to do this

    // Build nav list as static array
    NavPatch& patch = mPatches[chunk.getChunkID().id];

    // If we have old nav data, delete it
    if (patch.nodes) {
        delete[] patch.nodes;
    }

    if (navNodes.size()) {
        patch.size = (ui32)navNodes.size();
        patch.nodes = new NavNode[patch.size];
        memcpy(patch.nodes, navNodes.data(), sizeof(NavNode) * patch.size);
    }
    else {
        patch.nodes = nullptr;
        patch.size = 0;
    }
}

void NavGraph::buildNavNodesForChunkAsync(Chunk& chunk) {
    // TODO: Race conditions, we are writing to the nav graph on separate thread.
    // Fix1... move the memcpy to the main thread?
    // Fix2 is more involved as we can be reading chunk data that is in flux, we need to read lock the chunk

    chunk.incRef();
    chunk.incRefNeighbors4();
    chunk.mIsNavmeshing.store(true);
    if (sDebugOptions.mShowNavGraphUpdates) {
        Services::Threadpool::ref().addTask([&](ThreadPoolWorkerData* workerData) {
            // Update nav graph on worker thread
            buildNavNodesForChunkSynchronous(chunk);
            chunk.mIsNavmeshing.store(false);
            chunk.decRef();
            chunk.decRefNeighbors4();
        }, [&]() {

            if (sDebugOptions.mShowNavGraphUpdates) {
                debugDrawNavGraphForChunk(chunk, 250);
            }
        });
    }
    else {
        Services::Threadpool::ref().addTask([&](ThreadPoolWorkerData* workerData) {
            // Update nav graph on worker thread
            buildNavNodesForChunkSynchronous(chunk);
            chunk.mIsNavmeshing.store(false);
            chunk.decRef();
            chunk.decRefNeighbors4();
        }, nullptr);
    }
}

void NavGraph::debugDrawNavGraphForChunk(const Chunk& chunk, ui32 lifetime, int debugId /*= 0*/) const
{
    const color4 color1(0.0f, 1.0f, 1.0f, 0.75f);
    const color4 color2(1.0f, 0.0f, 0.0f, 0.75f);
    const WorldGrid& worldGrid = mWorld.getWorldGrid();
    const ChunkID& chunkId = chunk.getChunkID();
    const f32* heightData = worldGrid.getHeightDataAt(chunkId)->data;
    const NavPatch& patch = mPatches[chunk.getChunkID().id];
    // Draw edges
    for (ui32 nodeIndex = 0; nodeIndex < patch.size; ++nodeIndex) {
        const NavNode& node = patch.nodes[nodeIndex];
        const f32v2 cornerWorldPos = chunk.getWorldPos() + f32v2(node.cornerPos.getX(), node.cornerPos.getY());
        for (ui32 cartesian = 0; cartesian < 4; ++cartesian) {
            const ui32 edgeCount = node.counts[cartesian];
            for (ui32 i = 0; i < edgeCount; ++i) {
                const LiteNavNodeEdge& edge = node.edges[cartesian][i];
                const f32v2 edgeOffset = f32v2(CARTESIAN_EDGE_DIRS_ABS[cartesian]) * (f32)edge.start + f32v2(NAV_NODE_EDGE_OFFSETS[cartesian].xy);
                f32v2 cornerPos = cornerWorldPos + edgeOffset;
                if (cartesian == (ui32)Cartesian::RIGHT) cornerPos.x += 1.0f;
                else if (cartesian == (ui32)Cartesian::UP) cornerPos.y += 1.0f;
                const f32v2 offset = f32v2(CARTESIAN_EDGE_DIRS_ABS[cartesian]) * (f32)(edge.lengthMinusOne + 1.0f);
                const f32v3 pointA = helperGet3DPoint(worldGrid, chunkId, heightData, cornerPos);
                const f32v3 pointB = helperGet3DPoint(worldGrid, chunkId, heightData, cornerPos + offset);
                DebugRenderer::drawLineBetweenPoints(pointA, pointB, color1, lifetime, debugId);
                const f32v3 second(cornerPos.x + offset.x * 0.5f, cornerPos.y + offset.y * 0.5f, (pointA.z + pointB.z) * 0.5f);
                const f32v3 third(second.x + CARTESIAN_NORMALS[cartesian].x, second.y + CARTESIAN_NORMALS[cartesian].y, second.z);
                DebugRenderer::drawLineBetweenPoints(second, third, color1, lifetime, debugId);
            }
        }
    }
    // Draw connections between edges
    for (int nodeIndex = 0; nodeIndex < patch.size; ++nodeIndex) {
        const NavNode& node = patch.nodes[nodeIndex];
        const f32v2 cornerWorldPos = chunk.getWorldPos() + f32v2(node.cornerPos.getX(), node.cornerPos.getY());
        for (ui32 cartesian = 0; cartesian < 4; ++cartesian) {
            const ui32 edgeCount = node.counts[cartesian];
            for (ui32 i = 0; i < edgeCount; ++i) {
                const LiteNavNodeEdge& edge1 = node.edges[cartesian][i];
                const f32v2 edgeOffset1 = f32v2(CARTESIAN_EDGE_DIRS_ABS[cartesian]) * (f32)edge1.start + f32v2(NAV_NODE_EDGE_OFFSETS[cartesian].xy);
                f32v2 cornerPos1 = cornerWorldPos + edgeOffset1;
                const f32v2 offset1 = f32v2(CARTESIAN_EDGE_DIRS_ABS[cartesian]) * (f32)(edge1.lengthMinusOne + 1.0f);
                if (cartesian == (ui32)Cartesian::RIGHT) cornerPos1.x += 1.0f;
                else if (cartesian == (ui32)Cartesian::UP) cornerPos1.y += 1.0f;
                const f32v2 pos1 = cornerPos1 + offset1 * 0.5f;
                const f32v3 pointA = helperGet3DPoint(worldGrid, chunkId, heightData, pos1);
                // Connect to our side
                for (ui32 j = i + 1; j < edgeCount; ++j) {
                    const LiteNavNodeEdge& edge2 = node.edges[cartesian][j];
                    const f32v2 edgeOffset2 = f32v2(CARTESIAN_EDGE_DIRS_ABS[cartesian]) * (f32)edge2.start + f32v2(NAV_NODE_EDGE_OFFSETS[cartesian].xy);
                    f32v2 cornerPos2 = cornerWorldPos + edgeOffset2;
                    const f32v2 offset2 = f32v2(CARTESIAN_EDGE_DIRS_ABS[cartesian]) * (f32)(edge2.lengthMinusOne + 1.0f);
                    if (cartesian == (ui32)Cartesian::RIGHT) cornerPos2.x += 1.0f;
                    else if (cartesian == (ui32)Cartesian::UP) cornerPos2.y += 1.0f;
                    const f32v2 pos2 = cornerPos2 + offset2 * 0.5f;
                    DebugRenderer::drawLineBetweenPoints(pointA, helperGet3DPoint(worldGrid, chunkId, heightData, pos2), color2, lifetime, debugId);
                }

                // Connect to all other sides
                for (ui32 cartesian2 = cartesian + 1; cartesian2 < 4; ++cartesian2) {
                    const ui32 edgeCount2 = node.counts[cartesian2];
                    for (ui32 j = 0; j < edgeCount2; ++j) {
                        const LiteNavNodeEdge& edge2 = node.edges[cartesian2][j];
                        const f32v2 edgeOffset2 = f32v2(CARTESIAN_EDGE_DIRS_ABS[cartesian2]) * (f32)edge2.start + f32v2(NAV_NODE_EDGE_OFFSETS[cartesian2].xy);
                        f32v2 cornerPos2 = cornerWorldPos + edgeOffset2;
                        const f32v2 offset2 = f32v2(CARTESIAN_EDGE_DIRS_ABS[cartesian2]) * (f32)(edge2.lengthMinusOne + 1.0f);
                        if (cartesian2 == (ui32)Cartesian::RIGHT) cornerPos2.x += 1.0f;
                        else if (cartesian2 == (ui32)Cartesian::UP) cornerPos2.y += 1.0f;
                        const f32v2 pos2 = cornerPos2 + offset2 * 0.5f;
                        DebugRenderer::drawLineBetweenPoints(pointA, helperGet3DPoint(worldGrid, chunkId, heightData, pos2), color2, lifetime, debugId);
                    }
                }
            }
        }
    }
}

void NavGraph::buildEdges(Chunk& chunk, const int cornerX, const int cornerY, TileIndex cornerIndex, DisjointSetNode* djNodes, ui32* djNodeIDs, NavNodeIndex* navNodeIdTable, std::vector<NavNode>& navNodes, Cartesian dir)
{
    std::vector<Tile>& tiles = chunk.mTiles;
    ui32 currNodeId;
    i32v2 start(0);
    int length = 0;
    i32v2 subChunkRelativePos = CARTESIAN_EDGE_INDEX_OFFSET_MULTS[enum_cast(dir)] * (SUBCHUNK_WIDTH - 1);
    ui32 prevNodeId = djNodes[subChunkRelativePos.y * SUBCHUNK_WIDTH + subChunkRelativePos.x].id;
    i32v2 chunkRelativePos(cornerX, cornerY);
    i32v2 adjWorldPos = i32v2(chunk.getWorldPos()) + i32v2(cornerX + CARTESIAN_NORMALS[enum_cast(dir)].x, cornerY + CARTESIAN_NORMALS[enum_cast(dir)].y);
    for (int i = 0; i < SUBCHUNK_WIDTH; ++i) {
        TileIndex index(chunkRelativePos.x, chunkRelativePos.y);
        Tile& tile = tiles[index];
        const Tile& bottom = mWorld.getTileAtWorldPos(f32v2(adjWorldPos));
        const ui32 djIndex = subChunkRelativePos.y * SUBCHUNK_WIDTH + subChunkRelativePos.x;
        currNodeId = djNodes[djNodeIDs[djIndex]].id;
        // Check if we have an edge break
        if (currNodeId != prevNodeId) {
            // Finish edge
            if (length != 0) {
                addNodeEdge(chunk, navNodeIdTable, prevNodeId, navNodes, cornerIndex, TileIndex(start.x, start.y), length, dir);
                length = 0;
            }
            prevNodeId = currNodeId;
        }

        if (abs(bottom.getBaseZPositionUncompressed() - tile.getBaseZPositionUncompressed()) < 2.0f) {
            // Start new edge
            if (length == 0) {
                start = chunkRelativePos;
            }
            // Extend edge length
            ++length;
        }
        else if (length != 0) {
            addNodeEdge(chunk, navNodeIdTable, currNodeId, navNodes, cornerIndex, TileIndex(start.x, start.y), length, dir);
            length = 0;
        }
        const i32v2& edgeDir = CARTESIAN_EDGE_DIRS_ABS[enum_cast(dir)];
        adjWorldPos += edgeDir;
        chunkRelativePos += edgeDir;
        subChunkRelativePos += edgeDir;
    }
    // Add final edge if we reached end
    if (length != 0) {
        addNodeEdge(chunk, navNodeIdTable, currNodeId, navNodes, cornerIndex, TileIndex(start.x, start.y), length, dir);
    }
}

void NavGraph::addNodeEdge(Chunk& chunk, NavNodeIndex* navNodeIdTable, const ui32 djIndex, std::vector<NavNode>& navNodes, TileIndex corner, TileIndex start, int length, Cartesian dir) {
    NavNode* currNavNode;
    // Add nav node if it doesnt exist yet
    ui16& navNodeId = navNodeIdTable[djIndex];
    if (navNodeId == INVALID_NAV_NODE_INDEX) {
        navNodeId = (ui16)navNodes.size();
        currNavNode = &navNodes.emplace_back();
        currNavNode->chunkId = chunk.getChunkID().id;
        currNavNode->cornerPos = corner;
        currNavNode->numBottom = 0;
        currNavNode->numLeft = 0;
        currNavNode->numRight = 0;
        currNavNode->numTop = 0;
        currNavNode->isClosed = false;
    }
    else {
        currNavNode = &navNodes[navNodeId];
        assert(corner == currNavNode->cornerPos);
    }
    ui8& currCount = currNavNode->counts[enum_cast(dir)];
    assert(currCount < 8);
    assert(length > 0 && length <= 16);

    // Add edge
    LiteNavNodeEdge& edge = currNavNode->edges[enum_cast(dir)][currCount++];
    edge.lengthMinusOne = length - 1;

    // Because dir is separated into separate arrays, and is always along the subchunk boundary, we can encode where the start is along a 0-15 integer (4 byte)
    switch (dir) {
        case Cartesian::LEFT:
        case Cartesian::RIGHT: {
            int offsety = start.getY() - corner.getY();
            assert(offsety < 16 && offsety >= 0);
            edge.start = offsety;
        }
        break;
        case Cartesian::DOWN:
        case Cartesian::UP: {
            int offsetX = start.getX() - corner.getX();
            assert(offsetX < 16 && offsetX >= 0);
            edge.start = offsetX;
        }
        break;
    }
}