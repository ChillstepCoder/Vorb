#pragma once

#include "world/IHeightmapGrid.h"
#include <boost/container/flat_set.hpp>

class HeightmapTerrainQuadtree;
class TerrainMeshManager
{
public:
    TerrainMeshManager();
    ~TerrainMeshManager();

    void tick();

    // TODO: Call this
    void dirtyTerrainFromBrush(const f32v2& pos, f32 brushRadius);
    void dirtyAllTerrain();

    const std::vector<HeightmapTerrainQuadtree>& getTerrainQuadtrees() const { return mTerrainTrees; }

private:
    // Events
    void onTerrainModified(const boost::container::flat_set<i32v2>& modifiedPositions);
    IHeightmapGridListeners mHeightmapGridListeners;

    std::vector<HeightmapTerrainQuadtree> mTerrainTrees;
};

