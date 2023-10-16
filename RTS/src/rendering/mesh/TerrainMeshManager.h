#pragma once

#include "world/IHeightmapGrid.h"
#include <boost/container/flat_set.hpp>

class HeightmapTerrainQuadtree;
class TerrainMesh;

// Shared by render and game thread
class TerrainMeshManager
{
public:
    TerrainMeshManager(World& world);
    ~TerrainMeshManager();

    void tickGameThread(const f32v2& loadCenter);

    void dirtyAllTerrain();

    void addTerrainMesh(const TerrainMesh* mesh) { ASSERT_RENDER_THREAD(); mTerrainMeshes.insert(mesh); }
    void removeTerrainMesh(const TerrainMesh* mesh) { ASSERT_RENDER_THREAD(); mTerrainMeshes.erase(mesh); }
    void addTerrainWaterMesh(const TerrainMesh* mesh) { ASSERT_RENDER_THREAD(); mTerrainWaterMeshes.insert(mesh); }
    void removeTerrainWaterMesh(const TerrainMesh* mesh) { ASSERT_RENDER_THREAD(); mTerrainWaterMeshes.erase(mesh); }

    const std::vector<HeightmapTerrainQuadtree>& getTerrainQuadtrees() const { ASSERT_GAME_THREAD(); return mTerrainTrees; }

    const boost::container::flat_set<const TerrainMesh*>& getTerrainMeshes() const { ASSERT_RENDER_THREAD(); return mTerrainMeshes; }
    const boost::container::flat_set<const TerrainMesh*>& getTerrainWaterMeshes() const { ASSERT_RENDER_THREAD(); return mTerrainWaterMeshes; }

private:
    // Mesh data
    boost::container::flat_set<const TerrainMesh*> mTerrainMeshes;
    boost::container::flat_set<const TerrainMesh*> mTerrainWaterMeshes;

    // Events
    void onTerrainModified(const boost::container::flat_set<i32v2>& modifiedPositions);
    IHeightmapGridListeners mHeightmapGridListeners;
    World& mWorld;

    ui32 mWidthTerrainTrees = 0;
    std::vector<HeightmapTerrainQuadtree> mTerrainTrees;
};

