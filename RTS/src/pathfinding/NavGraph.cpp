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

struct DisjointSetNode {
    ui32 id;
};

NavGraph::NavGraph(World& world) : mWorld(world)
{

}

void NavGraph::buildNavNodesForChunkSynchronous(Chunk& chunk) {
    PreciseTimer timer;

    std::vector<NavNode>& navNodes = mNodes[chunk.getChunkID().id];
    navNodes.clear();
    navNodes.reserve(MIN_SUBCHUNKS_PER_CHUNK);

    // TODO: Separate internal with border chunks for faster lookups??
    // Iterate through sub chunks
    std::vector<TileCollision>& tileCollision = chunk.getAllTileCollision();
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
                    TileCollision& collision = tileCollision[index];
                    const int djArryIndex = y * SUBCHUNK_WIDTH + x;
                    bool assigned = false;
                    
                    if (x != 0) {
                        TileCollision& left = tileCollision[index - 1];
                        // Check if we can cross between
                        if (abs((int)left.baseZPosition - (int)collision.baseZPosition) < 2) {
                            djNodeIDs[djArryIndex] = djNodeIDs[djArryIndex - 1];
                            assigned = true;
                        }
                    }
                    if (y != 0) {
                        TileCollision& bottom = tileCollision[index - CHUNK_WIDTH];
                        // Check if we can cross between
                        if (abs((int)bottom.baseZPosition - (int)collision.baseZPosition) < 2) {
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

            buildEdges(chunk, cornerX, cornerY, djNodes, djNodeIDs, navNodeIdTable, navNodes, Cartesian::DOWN);
            buildEdges(chunk, cornerX, cornerY, djNodes, djNodeIDs, navNodeIdTable, navNodes, Cartesian::LEFT);
            buildEdges(chunk, cornerX + SUBCHUNK_WIDTH - 1, cornerY, djNodes, djNodeIDs, navNodeIdTable, navNodes, Cartesian::RIGHT);
            buildEdges(chunk, cornerX, cornerY + SUBCHUNK_WIDTH - 1, djNodes, djNodeIDs, navNodeIdTable, navNodes, Cartesian::UP);

            // Update all nav indices
            for (int y = 0; y < SUBCHUNK_WIDTH; ++y) {
                const int cornerY = sy * SUBCHUNK_WIDTH;
                for (int x = 0; x < SUBCHUNK_WIDTH; ++x) {
                    const int cornerX = sx * SUBCHUNK_WIDTH;
                    TileIndex index(cornerX + x, cornerY + y);
                    TileCollision& collision = tileCollision[index];
                    const ui32 djIndex = y * SUBCHUNK_WIDTH + x;
                    const ui32 navTableId = djNodes[djNodeIDs[djIndex]].id;
                    const NavNodeIndex navNodeIndex = navNodeIdTable[navTableId];
                    collision.navNodeIndex = navNodeIndex;
                }
            }
        }
    }
    // Iterate through outer sub chunks (has chunk neighbors)
}

void NavGraph::buildNavNodesForChunkAsync(Chunk& chunk) {

    // TODO: Race conditions
    chunk.incRef();
    chunk.mIsNavmeshing.store(true);
    if (sDebugOptions.mNavGraphUpdates) {
        Services::Threadpool::ref().addTask([&](ThreadPoolWorkerData* workerData) {
            // Update nav graph on worker thread
            buildNavNodesForChunkSynchronous(chunk);
            chunk.mIsNavmeshing.store(false);
            chunk.decRef();
        }, [&]() {

            if (sDebugOptions.mNavGraphUpdates) {
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
        }, nullptr);
    }
}

void NavGraph::debugDrawNavGraphForChunk(Chunk& chunk, ui32 lifetime)
{
    const WorldGrid& worldGrid = mWorld.getWorldGrid();
    std::vector<NavNode>& navNodes = mNodes[chunk.getChunkID().id];
    for (auto&& node : navNodes) {
        for (auto&& edge : node.edges) {
            f32v2 cornerPos = chunk.getWorldPos() + f32v2(edge.start.getX(), edge.start.getY());
            if (edge.dir == Cartesian::RIGHT) cornerPos.x += 1.0f;
            else if (edge.dir == Cartesian::UP) cornerPos.y += 1.0f;
            f32v2 offset = f32v2(CARTESIAN_EDGE_DIRS_ABS[enum_cast(edge.dir)]) * (f32)(edge.length);
            DebugRenderer::drawLineBetweenPoints(helperGet3DPoint(worldGrid, cornerPos), helperGet3DPoint(worldGrid, cornerPos + offset), color4(0.0f, 1.0f, 1.0f), lifetime);
            f32v2 second = cornerPos + offset * 0.5f;
            DebugRenderer::drawLineBetweenPoints(helperGet3DPoint(worldGrid, second), helperGet3DPoint(worldGrid, second + f32v2(CARTESIAN_NORMALS[enum_cast(edge.dir)])), color4(0.0f, 1.0f, 1.0f), lifetime);
        }
    }
    for (int k = 0; k < navNodes.size(); ++k) {
        auto&& node = navNodes[k];
        for (int i = 0; i < node.edges.size() - 1; ++i) {
            for (int j = i + 1; j < node.edges.size(); ++j) {
                ui32 id = k;
                color4 color = color4(255, 0, 0);
                NavNodeEdge& edge1 = node.edges[i];
                NavNodeEdge& edge2 = node.edges[j];
                f32v2 offset1 = f32v2(CARTESIAN_EDGE_DIRS_ABS[enum_cast(edge1.dir)]) * (f32)(edge1.length);
                f32v2 cornerPos1 = chunk.getWorldPos() + f32v2(edge1.start.getX(), edge1.start.getY());
                if (edge1.dir == Cartesian::RIGHT) cornerPos1.x += 1.0f;
                else if (edge1.dir == Cartesian::UP) cornerPos1.y += 1.0f;
                f32v2 pos1 = cornerPos1 + offset1 * 0.5f;
                f32v2 offset2 = f32v2(CARTESIAN_EDGE_DIRS_ABS[enum_cast(edge2.dir)]) * (f32)(edge2.length);
                f32v2 cornerPos2 = chunk.getWorldPos() + f32v2(edge2.start.getX(), edge2.start.getY());
                if (edge2.dir == Cartesian::RIGHT) cornerPos2.x += 1.0f;
                else if (edge2.dir == Cartesian::UP) cornerPos2.y += 1.0f;
                f32v2 pos2 = cornerPos2 + offset2 * 0.5f;
                DebugRenderer::drawLineBetweenPoints(helperGet3DPoint(worldGrid, pos1), helperGet3DPoint(worldGrid, pos2), color, lifetime);
            }
        }
    }
}

void NavGraph::buildEdges(Chunk& chunk, const int cornerX, const int cornerY, DisjointSetNode* djNodes, ui32* djNodeIDs, NavNodeIndex* navNodeIdTable, std::vector<NavNode>& navNodes, Cartesian dir)
{
    std::vector<TileCollision>& tileCollision = chunk.getAllTileCollision();
    ui32 currNodeId;
    i32v2 start(0);
    int length = 0;
    i32v2 subChunkRelativePos = CARTESIAN_EDGE_INDEX_OFFSET_MULTS[enum_cast(dir)] * (SUBCHUNK_WIDTH - 1);
    ui32 prevNodeId = djNodes[subChunkRelativePos.y * SUBCHUNK_WIDTH + subChunkRelativePos.x].id;
    i32v2 chunkRelativePos(cornerX, cornerY);
    i32v2 adjWorldPos = i32v2(chunk.getWorldPos()) + i32v2(cornerX + CARTESIAN_NORMALS[enum_cast(dir)].x, cornerY + CARTESIAN_NORMALS[enum_cast(dir)].y);
    for (int i = 0; i < SUBCHUNK_WIDTH; ++i) {
        TileIndex index(chunkRelativePos.x, chunkRelativePos.y);
        TileCollision& collision = tileCollision[index];
        const TileCollision& bottom = mWorld.getTileCollisionAtWorldPos(ui32v2(adjWorldPos));
        const ui32 djIndex = subChunkRelativePos.y * SUBCHUNK_WIDTH + subChunkRelativePos.x;
        currNodeId = djNodes[djNodeIDs[djIndex]].id;
        // Check if we have an edge break
        if (currNodeId != prevNodeId) {
            // Finish edge
            if (length != 0) {
                addNodeEdge(chunk, navNodeIdTable, prevNodeId, navNodes, TileIndex(start.x, start.y), length, dir);
                length = 0;
            }
            prevNodeId = currNodeId;
        }

        if (abs((int)bottom.baseZPosition - (int)collision.baseZPosition) < 2) {
            // Start new edge
            if (length == 0) {
                start = chunkRelativePos;
            }
            // Extend edge length
            ++length;
        }
        else if (length != 0) {
            addNodeEdge(chunk, navNodeIdTable, currNodeId, navNodes, TileIndex(start.x, start.y), length, dir);
            length = 0;
        }
        const i32v2& edgeDir = CARTESIAN_EDGE_DIRS_ABS[enum_cast(dir)];
        adjWorldPos += edgeDir;
        chunkRelativePos += edgeDir;
        subChunkRelativePos += edgeDir;
    }
    // Add final edge if we reached end
    if (length != 0) {
        addNodeEdge(chunk, navNodeIdTable, currNodeId, navNodes, TileIndex(start.x, start.y), length, dir);
    }
}

void NavGraph::addNodeEdge(Chunk& chunk, NavNodeIndex* navNodeIdTable, const ui32 djIndex, std::vector<NavNode>& navNodes, TileIndex start, int length, Cartesian dir) {
    NavNode* currNavNode;
    // Add nav node if it doesnt exist yet
    ui16& navNodeId = navNodeIdTable[djIndex];
    if (navNodeId == UINT16_MAX) {
        navNodeId = (ui16)navNodes.size();
        currNavNode = &navNodes.emplace_back(chunk);
    }
    else {
        currNavNode = &navNodes[navNodeId];
    }
    // Add edge
    currNavNode->edges.emplace_back(start, length, dir);
}