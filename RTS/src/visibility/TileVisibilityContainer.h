#pragma once

#include "tile/TileSpatialGrid.h"

class Tile;
class TileWallContainer;
struct TileWalls;

class TileVisibilityContainer {
public:
    void init(const TileSpatialGrid* tileSpatialGrid, const std::vector<Tile>& tiles, const TileWallContainer& tileWalls);
    void resize(const TileSpatialGrid* tileSpatialGrid);
    void copyFrom(const TileVisibilityContainer& other);
    // TODO: Serialize
    void destroy();

    bool isTileVisibleFromDirection(TileIndex tileIndex, Cartesian dir);
    // Return true if it changed
    bool refreshTileVisibility(TileIndex tileIndex, const Tile& tile, const TileWalls& tileWalls);

private:
    // Each tile has 4 visibility edges, tiles share edges. We store an extra tile border, and each tile owns its -x and -y edges.
    // First -y then -x
    BitArray mVisibilityEdges;
    // Each tile has its own visibility info used to construct the visibility edges
    BitArray mTileVisibility; //< Not used by worker thread
    const TileSpatialGrid* mTileSpatialGrid = nullptr;
    i32v3 mEdgesDims = i32v3(0);
    i32 mFloorStride = 0;
};

