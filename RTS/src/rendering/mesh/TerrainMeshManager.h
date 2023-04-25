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

    void dirtyAllTerrain();

    const std::vector<HeightmapTerrainQuadtree>& getTerrainQuadtrees() const { return mTerrainTrees; }

private:
    // Events
    void onTerrainModified(const boost::container::flat_set<i32v2>& modifiedPositions);
    IHeightmapGridListeners mHeightmapGridListeners;

    std::vector<HeightmapTerrainQuadtree> mTerrainTrees;
};

