#pragma once

#include "world/WorldData.h"
#include "data_structure/FlatQuadtree.h"

// Why does excluding this cause an error
#include "rendering/mesh/TerrainMesh.h"
#include "rendering/mesh/WaterMesh.h"

class Camera3D;
class WorldGrid;
DECL_VG(class GLProgram);

class HeightmapTerrainQuadtree : public FlatQuadtree<TERRAIN_QUADTREE_MAX_LOD, TERRAIN_QUADTREE_WIDTH>
{
public:
    HeightmapTerrainQuadtree();
    ~HeightmapTerrainQuadtree();

    VORB_NON_COPYABLE_BUT_MOVABLE(HeightmapTerrainQuadtree);

    void init(const f32v2& worldPosition, WorldGrid& worldGrid);

    void renderTerrain(const Camera3D& camera, const vg::GLProgram& program) const;
    void renderWater(const Camera3D& camera, const vg::GLProgram& program) const;
    
    void markDirty();

private:
    void buildMeshForPatch(QuadtreePatch& patch, ui32 lod, ui32 patchIndex) override;

    void createMeshes(const HeightmapPatchID id, ui32 patchIndex, ui32 lod);
    void finishMeshes(ui32 patchIndex);

    void freeMeshForPatch(ui32 patchIndex) override;

    std::unique_ptr<TerrainMesh> mTerrainMeshes[FlatQuadtree<GRASS_QUADTREE_MAX_LOD, CHUNK_WIDTH>::NODE_COUNT];
    std::unique_ptr<WaterMesh> mWaterMeshes[FlatQuadtree<GRASS_QUADTREE_MAX_LOD, CHUNK_WIDTH>::NODE_COUNT];
    ui32 mRefCount = 0; // TODO: This is probably unneeded
    WorldGrid* mWorldGrid = nullptr;
};
