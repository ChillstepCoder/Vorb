#include "stdafx.h"

#include "World.h"
#include "TileScanner.h"

struct BfsNode {
    BfsNode() {};
    BfsNode(ui32v2 pos, ui32 depth) : pos(pos), depth(depth) {};

    ui32v2 pos;
    ui32 depth;
};

struct closedListNode {
    closedListNode() {}
    closedListNode(ui32v2& xy) : xy(xy) {};

    bool operator<(const closedListNode& l) const {
        return l.cmpValue < cmpValue;
    }

    union {
        ui32v2 xy;
        ui64 cmpValue; // For ez set lookup
    };
};

std::vector<LiteTileHandle> TileScanner::scanForResource(World& world, TileResource resource, const ui32v2& startWorldPos, ui32 maxDistance, ui32 maxTilesToReturn /* = UINT32_MAX */) {
    std::vector<LiteTileHandle> tilesToReturn;
    if (maxTilesToReturn != UINT32_MAX) {
        tilesToReturn.reserve(maxTilesToReturn);
    }
    
    // BFS
    // TODO: Custom memory management
    std::set<closedListNode> closedList;
    std::queue<BfsNode> openList;
    const f32 maxDistSq = SQ(maxDistance);
    openList.emplace(startWorldPos, 0);

    while (openList.size()) {
        BfsNode node = openList.front();
        openList.pop();
        TileHandle tileHandle = world.getTileHandleAtWorldPos(node.pos);
        if (!tileHandle.isValid()) continue;

        // Store this if it contains a tile we want
        // Skip the ground layer, it is never a resource
        for (int i = TILE_LAYER_MID; i < TILE_LAYER_COUNT; ++i) {
            const TileID id = tileHandle.tile.layers[i];
            if (id != TILE_ID_NONE) {
                if (TileRepository::getTileData(id).resource == resource) {
                    tilesToReturn.emplace_back(tileHandle.chunk->getChunkID(), tileHandle.index);
                    if (tilesToReturn.size() >= maxTilesToReturn) {
                        return tilesToReturn;
                    }
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
                openList.emplace(nextPos, nextDepth);
            }
        }

        // Left
        {
            ui32v2 nextPos(node.pos.x - 1, node.pos.y);
            if (closedList.find(nextPos) == closedList.end()) {
                closedList.insert(nextPos);
                openList.emplace(nextPos, nextDepth);
            }
        }

        // Right
        {
            ui32v2 nextPos(node.pos.x + 1, node.pos.y);
            if (closedList.find(nextPos) == closedList.end()) {
                closedList.insert(nextPos);
                openList.emplace(nextPos, nextDepth);
            }
        }

        // Top
        {
            ui32v2 nextPos(node.pos.x, node.pos.y + 1);
            if (closedList.find(nextPos) == closedList.end()) {
                closedList.insert(nextPos);
                openList.emplace(nextPos, nextDepth);
            }
        }
    }

    return tilesToReturn;
}
