#pragma once

#include "tile/TileSpatialGrid.h"

class Tile;
class TileWallContainer;
struct TileWalls;

typedef i32 VisEdgeIndex;

class TileVisibilityContainer {
public:
    void init(const TileSpatialGrid* tileSpatialGrid, const std::vector<Tile>& tiles, const TileWallContainer& tileWalls);
    void resize(const TileSpatialGrid* tileSpatialGrid);
    void copyFrom(const TileVisibilityContainer& other);
    // TODO: Serialize
    void destroy();

    // Return true if there was a change
    void refreshTileVisibility(TileIndex tileIndex, const Tile& prevTile, const Tile& newTile);
    void refreshTileVisibility(TileIndex tileIndex, const TileWalls& prevTileWalls, const TileWalls& newTileWalls);
    void refreshTileVisibility(TileIndex tileIndex, TileWall prevWall, TileWall newWall, Cartesian dir);

    bool getOccludedEdge(TileIndex tileIndex, Cartesian dir) const;

    void debugRender() const;

private:
    VisEdgeIndex getEdgeIndexBase(TileIndex tileIndex) const {
        return tileIndex * 4;
    }
    VisEdgeIndex getEdgeIndexFromBase(VisEdgeIndex base, Cartesian dir) const {
        return base + VisEdgeIndex(dir);
    }
    VisEdgeIndex getEdgeIndex(TileIndex tileIndex, Cartesian dir) const {
        return tileIndex * 4 + VisEdgeIndex(dir);
    }

    void occludeAllEdgesForTile(TileIndex tileIndex);
    // TODO: Microoptimization - we could allocate mVisibilityEdges and mTileVisibility in one big BitArray to reduce fragmentation and save a cache miss
    // Each tile has 4 visibility edges representing whether you can see into or out of the tile in that direction
    BitArray mOccludedEdges;
    // Each tile has its own visibility info used to construct the visibility edges
    BitArray mTileOcclusion; // Not used by visibility thread. 1 means we do not have visibility
    std::vector<ui8> mTileVisibilityBlockerCounts; // > 1 means we do not have visibility. Not used by visibility thread
    const TileSpatialGrid* mTileSpatialGrid = nullptr;
    i32 mFloorStride = 0;
};

