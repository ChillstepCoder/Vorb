#pragma once

#include "data_structure/FlatQuadtree.h"
// TODO: I dont like this include for GrassBillboardMesh
#include "rendering/GrassBillboardMesh.h"

class GrassBillboardMesh;
class Chunk;
class World;
DECL_VG(class GLProgram);

using ChunkGrassFlatQuadtree = FlatQuadtree<GRASS_QUADTREE_MAX_LOD, CHUNK_WIDTH>;

// For node I, its children are 4 * i + 1 through 4 * i + 4
// A complete quadtree of N levels has 4^N - 1)
class ChunkGrassQuadtree : public ChunkGrassFlatQuadtree
{
public:
    ChunkGrassQuadtree(const Chunk& mChunk);
    ~ChunkGrassQuadtree();

    ui32 getRefCount() const { return mRefCount; }
    const f32v3 getWorldPos3D() const { return f32v3(mWorldPos.x, mWorldPos.y, 0.0f); }

private:
    virtual void resetCrossfadeRenderForPatch(ui32 patchIndex, int crossfadeDir, f32 crossfadeAlpha) override;
    virtual void updateCrossfadeRenderForPatch(ui32 patchIndex, f32 crossfadeAlpha) override;

    void buildMeshForPatch(QuadtreePatch& patch, ui32 lod, ui32 patchIndex) override;
    void freeMeshForPatch(ui32 patchIndex) override;
    void finishMesh(GrassBillboardMeshBuilder& meshBuilder, ui32 patchIndex);

    // Flat for cache coherency, no allocations, and multithreading
    const Chunk& mChunk;
    std::unique_ptr<GrassMesh> mMeshes[ChunkGrassFlatQuadtree::NODE_COUNT];
    ui32 mRefCount = 0;
};
