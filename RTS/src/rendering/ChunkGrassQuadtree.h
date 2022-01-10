#pragma once

#include "data_structure/FlatQuadtree.h"

class GrassBillboardMesh;
class Chunk;
class Camera3D;
class WorldGrid;
DECL_VG(class GLProgram);

using ChunkGrassFlatQuadtree = FlatQuadtree<GRASS_QUADTREE_MAX_LOD, CHUNK_WIDTH>;

// For node I, its children are 4 * i + 1 through 4 * i + 4
// A complete quadtree of N levels has 4^N - 1)
class ChunkGrassQuadtree : public ChunkGrassFlatQuadtree
{
public:
    ChunkGrassQuadtree(const Chunk& mChunk, WorldGrid& worldGrid);
    ~ChunkGrassQuadtree();

    void render(const Camera3D& camera, const vg::GLProgram& program) const;

    ui32 getRefCount() const { return mRefCount; }

private:
    void buildMeshForPatch(QuadtreePatch& patch, ui32 lod, ui32 patchIndex) override;
    void freeMeshForPatch(ui32 patchIndex) override;

    // Flat for cache coherency, no allocations, and multithreading
    const Chunk& mChunk;
    WorldGrid& mWorldGrid;
    std::unique_ptr<GrassBillboardMesh> mMeshes[ChunkGrassFlatQuadtree::NODE_COUNT];
    ui32 mRefCount = 0;
};