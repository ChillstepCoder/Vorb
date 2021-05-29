#include "stdafx.h"

#include "World.h"
#include "TileScanner.h"

struct BfsNode {
    ui32v2 pos;
    ui32 depth;
};

std::vector<LiteTileHandle> TileScanner::scanForTiles(World& world, const TileID tileIDs[], ui32 numTileIDs, const ui32v2& startWorldPos, ui32 maxDistance) {
    std::vector<LiteTileHandle> tilesToReturn;
    
    // BFS
    // TODO: Custom memory management
    std::set<ui32v2> closedList;
    std::queue<BfsNode> mOpenList;
    const f32 maxDistSq = SQ(maxDistance);
    mOpenList.emplace(startWorldPos, 0);

    while (mOpenList.size()) {
        BfsNode node = mOpenList.front();
        mOpenList.pop();
        TileHandle tileHandle = world.getTileHandleAtWorldPos(node.pos);
        if (!tileHandle.isValid()) continue;

        // Store this if it contains a tile we want
        for (int i = 0; i < TILE_LAYER_COUNT; ++i) {
            const TileID id = tileHandle.tile.layers[i];
            for (ui32 j = 0; j < numTileIDs; ++j) {
                if (id == tileIDs[j]) {
                    tilesToReturn.emplace_back(tileHandle.chunk->getChunkID(), tileHandle.index);
                    i = TILE_LAYER_COUNT;
                    break;
                }
            }
        }

        // Stop at max distance
        if (node.depth >= maxDistance) {
            continue;
        }

        // Do BFS
        const ui32 nextDepth = node.depth + 1;

        // Bottom
        {
            ui32v2 nextPos(node.pos.x, node.pos.y - 1);
            if (closedList.find(nextPos) == closedList.end()) {
                closedList.insert(nextPos);
                mOpenList.emplace(nextPos, nextDepth);
            }
        }

        // Left
        {
            ui32v2 nextPos(node.pos.x - 1, node.pos.y);
            if (closedList.find(nextPos) == closedList.end()) {
                closedList.insert(nextPos);
                mOpenList.emplace(nextPos, nextDepth);
            }
        }

        // Right
        {
            ui32v2 nextPos(node.pos.x + 1, node.pos.y);
            if (closedList.find(nextPos) == closedList.end()) {
                closedList.insert(nextPos);
                mOpenList.emplace(nextPos, nextDepth);
            }
        }

        // Top
        {
            ui32v2 nextPos(node.pos.x, node.pos.y + 1);
            if (closedList.find(nextPos) == closedList.end()) {
                closedList.insert(nextPos);
                mOpenList.emplace(nextPos, nextDepth);
            }
        }
    }
}
