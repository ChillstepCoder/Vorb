#pragma once

#include "data_structure/FlatQuadtree.h"

class GrassBillboardMesh;
class Chunk;
class Camera3D;
DECL_VG(class GLProgram);

// For node I, its children are 4 * i + 1 through 4 * i + 4
// A complete quadtree of N levels has 4^N - 1)
class ChunkGrassQuadtree : public FlatQuadtree<GRASS_QUADTREE_MAX_LOD, CHUNK_WIDTH>
{
public:
    ChunkGrassQuadtree(const Chunk& mChunk);
    ~ChunkGrassQuadtree();

    void render(const Camera3D& camera, const vg::GLProgram& program);

    ui32 getRefCount() const { return mRefCount; }

private:
    void updateMeshForPatch(QuadtreePatch& patch, ui32 lod, ui32 patchIndex) override;
    void freeMeshForPatch(ui32 patchIndex) override;

    // Flat for cache coherency, no allocations, and multithreading
    const Chunk& mChunk;
    std::unique_ptr<GrassBillboardMesh> mMeshes[FlatQuadtree<GRASS_QUADTREE_MAX_LOD, CHUNK_WIDTH>::NODE_COUNT];
    ui32 mRefCount = 0;
};