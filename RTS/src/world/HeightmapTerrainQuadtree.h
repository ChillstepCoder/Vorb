#pragma once

#include "world/WorldData.h"
#include "data_structure/FlatQuadtree.h"

#include "rendering/mesh/Mesh.h"

struct TerrainMeshTaskData;
class World;
class TerrainMeshBuilder;
DECL_VG(class GLProgram);

class TerrainMesh {
public:
    TerrainMesh(ui32 patchIndex) : mIndex(patchIndex) {};

    VORB_NON_COPYABLE_BUT_MOVABLE(TerrainMesh);

    Mesh mMesh;
    ui32 mIndex;
    std::atomic<f32> mCrossfadeAlpha = 0.0f;
    std::atomic_int mCrossfadeDir = 0; // -1 = down, 0 = none, 1 = up
};

class HeightmapTerrainQuadtree : public FlatQuadtree<TERRAIN_QUADTREE_MAX_LOD, TERRAIN_QUADTREE_WIDTH>
{
public:
    HeightmapTerrainQuadtree(World& world, const f32v2& worldPosition);
    ~HeightmapTerrainQuadtree();

    VORB_NON_COPYABLE_BUT_MOVABLE(HeightmapTerrainQuadtree);

    void markDirty();

private:
    virtual void resetCrossfadeRenderForPatch(ui32 patchIndex, int crossfadeDir, f32 crossfadeAlpha) override;
    virtual void updateCrossfadeRenderForPatch(ui32 patchIndex, f32 crossfadeAlpha) override;

    void buildMeshForPatch(QuadtreePatch& patch, ui32 lod, ui32 patchIndex) override;

    void createMeshesHighestLOD(TerrainMeshTaskData* taskData);
    void finishMeshes(TerrainMeshBuilder& terrainBuilder, ui32 patchIndex);
    void freeMeshForPatch(ui32 patchIndex) override;

    World& mWorld;
    std::unique_ptr<TerrainMesh> mTerrainMeshes[FlatQuadtree<TERRAIN_QUADTREE_MAX_LOD, CHUNK_WIDTH>::NODE_COUNT];
    std::unique_ptr<TerrainMesh> mWaterMeshes[FlatQuadtree<TERRAIN_QUADTREE_MAX_LOD, CHUNK_WIDTH>::NODE_COUNT];
};
