#include "stdafx.h"
#include "HeightmapTerrainQuadtree.h"

#include "camera/Camera3D.h"
#include "options/DebugOptions.h"
#include "DebugRenderer.h"
#include "world/WorldGrid.h"

#include "generation/WorldGeneration.h"
#include <Vorb/graphics/GLProgram.h>

constexpr f32 TERRAIN_SUBDIVIDE_DISTANCES_SQ[GRASS_QUADTREE_MAX_LOD] = { // sqrt(pow(WIDTH, 2) * 2) for diagonal distance widths
    SQ(6000.0f),
    SQ(3000.0f),
    SQ(600.0f),
    SQ(64.0f),
    -FLT_MAX // Never subdivide last
};


HeightmapTerrainQuadtree::HeightmapTerrainQuadtree() : FlatQuadtree(f32v2(0.0f), TERRAIN_SUBDIVIDE_DISTANCES_SQ, sDebugOptions.mTerrainLodDistanceOffset) {

}

HeightmapTerrainQuadtree::~HeightmapTerrainQuadtree()
{

}

void HeightmapTerrainQuadtree::init(const f32v2& worldPosition, WorldGrid& worldGrid)
{
    mWorldPos = worldPosition;
    mWorldGrid = &worldGrid;
}

void HeightmapTerrainQuadtree::renderTerrain(const Camera3D& camera, const vg::GLProgram& program) const {
    VGUniform crossfadeAlphaUniform = program.getUniform("unCrossfadeAlpha"); // TODO: Cache?
    VGUniform crossfadeDirectionUniform = program.getUniform("unCrossfadeDirection");
    f32v3 pos3D(mWorldPos.x, mWorldPos.y, 0.0f);
    VGUniform offsetUniform = program.getUniform("unOffset");
    f32v3 offset = pos3D - camera.getPosition();
    glUniform3fv(offsetUniform, 1, &offset.x);

    for (ui32 i = 0; i < mNumActiveNodes; ++i) {
        ui32 index = mActiveNodes[i];
        const QuadtreePatch& patch = mNodes[index];

        if (patch.canRender()) {
            auto& mesh = mTerrainMeshes[index];
            ui32 lod = QUADTREE_LOD_FROM_INDEX[index];
            f32v2 centerPos = f32v2(PATCH_POSITIONS.data[index].xy) + f32v2(LOD_HALF_DIMS[lod].xy);
            f32v3 centerPos3d(centerPos.x, centerPos.y, 0.0f);
            if (patch.isCrossfading()) {
                glUniform1f(crossfadeAlphaUniform, mCrossfadeTable[patch.mCrossFadeTableIndex] * 0.5f /* Constant that was selected via trial and error*/);
                glUniform1f(crossfadeDirectionUniform, patch.mFlags & QUADTREE_PATCH_FLAG_CROSSFADING_IN ? 1.0f : 0.0f);
            }
            else {
                glUniform1f(crossfadeAlphaUniform, 0.0f);
                glUniform1f(crossfadeDirectionUniform, 0.0f);
            }
            const BoundingSphere& bounds = mesh->getBoundingSphere();
            if (camera.sphereIsVisible(bounds.center, bounds.radius)) {
                mesh->draw(program);
            }
        }
    }
}

void HeightmapTerrainQuadtree::renderWater(const Camera3D& camera, const vg::GLProgram& program) const {
    f32v3 pos3D(mWorldPos.x, mWorldPos.y, 0.0f);
    VGUniform offsetUniform = program.getUniform("unOffset");
    f32v3 offset = pos3D - camera.getPosition();
    glUniform3fv(offsetUniform, 1, &offset.x);

    for (ui32 i = 0; i < mNumActiveNodes; ++i) {
        ui32 index = mActiveNodes[i];
        const QuadtreePatch& patch = mNodes[index];

        if (patch.canRender()) {
            auto& mesh = mWaterMeshes[index];
            ui32 lod = QUADTREE_LOD_FROM_INDEX[index];
            f32v2 centerPos = f32v2(PATCH_POSITIONS.data[index].xy) + f32v2(LOD_HALF_DIMS[lod].xy);
            f32v3 centerPos3d(centerPos.x, centerPos.y, 0.0f);
            if (patch.isCrossfading() && (patch.mFlags & QUADTREE_PATCH_FLAG_CROSSFADING_OUT)) {
                // We simply dont crossfade water, always render the in crossfade only
                continue;
            }
            const BoundingSphere& bounds = mesh->getBoundingSphere();
            if (camera.sphereIsVisible(bounds.center, bounds.radius)) {
                mesh->draw(program);
            }
        }
    }
}

void HeightmapTerrainQuadtree::markDirty() {
    for (ui32 i = 0; i < mNumActiveNodes; ++i) {
        ui32 index = mActiveNodes[i];
        QuadtreePatch& patch = mNodes[index];
        patch.mFlags |= QUADTREE_PATCH_FLAG_DIRTY_MESH;
    }
}

void createTerrainAndWaterMesh(
    TerrainMesh& mesh,
    WaterMesh& waterMesh,
    const ui32v2& posStart,
    ui32 lod,
    const f32v2& worldPos
) {
    const ui32v2& dims = (ui32v2&)FlatQuadtree<TERRAIN_QUADTREE_MAX_LOD, TERRAIN_QUADTREE_WIDTH>::LOD_DIMS[lod];
    f32v2 quadDims = f32v2(dims) / f32v2(TERRAIN_MESH_WIDTH_QUADS);
    mesh.beginMesh(posStart, dims.x);
    waterMesh.beginMesh(posStart, dims.x);

    // AABB calculation
    f32AABB3 aabb;
    aabb.dims.x = dims.x;
    aabb.dims.y = dims.y;
    aabb.pos.x = posStart.x + worldPos.x;
    aabb.pos.y = posStart.y + worldPos.y;
    f32 minZ = FLT_MAX;
    f32 maxZ = FLT_MIN;

    // Generate heightfield
    f32 paddedHeightfield[TERRAIN_MESH_PADDED_WIDTH_VERTS][TERRAIN_MESH_PADDED_WIDTH_VERTS];
    for (ui32 y = 0; y < TERRAIN_MESH_PADDED_WIDTH_VERTS; ++y) {
        for (ui32 x = 0; x < TERRAIN_MESH_PADDED_WIDTH_VERTS; ++x) {
            const f32v2 vertPos = f32v2(posStart.x + ((f32)x - 1.0f) * quadDims.x, posStart.y + ((f32)y - 1.0f) * quadDims.y);
            f32 zPos = sWorldGen.getHeightAtPos(f32v2(vertPos.x + worldPos.x, vertPos.y + worldPos.y));
            if (zPos < minZ) minZ = zPos;
            if (zPos > maxZ) maxZ = zPos;
            paddedHeightfield[y][x] = zPos;
        }
    }
    mesh.setVertsFromPaddedHeightfield(paddedHeightfield);
    waterMesh.setVertsFromPaddedHeightfield(paddedHeightfield);
    // Bounding sphere
    aabb.pos.z = minZ;
    aabb.dims.z = maxZ - minZ;
    const BoundingSphere sphere = boundingSphereFromAABB(aabb);
    mesh.setBoundingSphere(sphere);
    waterMesh.setBoundingSphere(sphere);
};

void createTerrainAndWaterMesh(
    TerrainMesh& mesh,
    WaterMesh& waterMesh,
    const ui32v2& posStart,
    ui32 lod,
    const f32v2& worldPos,
    const HeightmapPatchData* paddedHeightData[9]
) {
    const ui32v2& dims = (ui32v2&)FlatQuadtree<TERRAIN_QUADTREE_MAX_LOD, TERRAIN_QUADTREE_WIDTH>::LOD_DIMS[lod];
    f32v2 quadDims = f32v2(dims) / f32v2(TERRAIN_MESH_WIDTH_QUADS);
    mesh.beginMesh(posStart, dims.x);
    waterMesh.beginMesh(posStart, dims.x);

    f32 paddedHeightfield[TERRAIN_MESH_PADDED_WIDTH_VERTS][TERRAIN_MESH_PADDED_WIDTH_VERTS];

    // Copy bounding sphere
    const HeightmapPatchData* bl = paddedHeightData[0];
    const HeightmapPatchData* b = paddedHeightData[1];
    const HeightmapPatchData* br = paddedHeightData[2];
    const HeightmapPatchData* l = paddedHeightData[3];
    const HeightmapPatchData* c = paddedHeightData[4];
    const HeightmapPatchData* r = paddedHeightData[5];
    const HeightmapPatchData* tl = paddedHeightData[6];
    const HeightmapPatchData* t = paddedHeightData[7];
    const HeightmapPatchData* tr = paddedHeightData[8];
    mesh.setBoundingSphere(c->boundingSphere);
    waterMesh.setBoundingSphere(c->boundingSphere);

    // Center memcopy row by row
    for (ui32 y = 0; y < TERRAIN_MESH_WIDTH_VERTS; ++y) {
        memcpy(&paddedHeightfield[y + 1][1], &c->data[y * HEIGHTMAP_VERT_WIDTH_PER_CHUNK], sizeof(f32) * HEIGHTMAP_VERT_WIDTH_PER_CHUNK);
    }
    static_assert(TERRAIN_MESH_WIDTH_VERTS == HEIGHTMAP_VERT_WIDTH_PER_CHUNK);

    // === Generate edges ===
    // Left and right edge
    for (int y = 1; y < TERRAIN_MESH_PADDED_WIDTH_VERTS - 1; ++y) {
        { // Left
            constexpr ui32 x = 0;
            paddedHeightfield[y][x] = l->data[(y - 1) * HEIGHTMAP_VERT_WIDTH_PER_CHUNK + HEIGHTMAP_VERT_WIDTH_PER_CHUNK - 2];
        }
        { // Right
            constexpr ui32 x = TERRAIN_MESH_PADDED_WIDTH_VERTS - 1;
            paddedHeightfield[y][x] = r->data[(y - 1) * HEIGHTMAP_VERT_WIDTH_PER_CHUNK + 1];
        }
    }
    // Bottom
    memcpy(&paddedHeightfield[0][1], &b->data[HEIGHTMAP_VERT_SIZE_PER_CHUNK - 2 * HEIGHTMAP_VERT_WIDTH_PER_CHUNK], sizeof(f32) * HEIGHTMAP_VERT_WIDTH_PER_CHUNK);
    // Top
    memcpy(&paddedHeightfield[TERRAIN_MESH_PADDED_WIDTH_VERTS - 1][1], &t->data[HEIGHTMAP_VERT_WIDTH_PER_CHUNK], sizeof(f32) * HEIGHTMAP_VERT_WIDTH_PER_CHUNK);
    // 4 Corners
    // Bottom left
    paddedHeightfield[0][0] = bl->data[HEIGHTMAP_VERT_SIZE_PER_CHUNK - HEIGHTMAP_VERT_WIDTH_PER_CHUNK - 2];
    // Bottom right
    paddedHeightfield[0][TERRAIN_MESH_PADDED_WIDTH_VERTS - 1] = br->data[HEIGHTMAP_VERT_SIZE_PER_CHUNK - 2 * HEIGHTMAP_VERT_WIDTH_PER_CHUNK + 1];
    // Top Left
    paddedHeightfield[TERRAIN_MESH_PADDED_WIDTH_VERTS - 1][0] = tl->data[HEIGHTMAP_VERT_WIDTH_PER_CHUNK - 2];
    // Top Right
    paddedHeightfield[TERRAIN_MESH_PADDED_WIDTH_VERTS - 1][TERRAIN_MESH_PADDED_WIDTH_VERTS - 1] = tr->data[HEIGHTMAP_VERT_WIDTH_PER_CHUNK + 1];

    // Build
    mesh.setVertsFromPaddedHeightfield(paddedHeightfield);
    waterMesh.setVertsFromPaddedHeightfield(paddedHeightfield);
};

void HeightmapTerrainQuadtree::buildMeshForPatch(QuadtreePatch& patch, ui32 lod, ui32 patchIndex)
{
    bool hasAquired = true;
    if (!mTerrainMeshes[patchIndex]) {
        hasAquired = false; // If we dont have a mesh, we haven't aquired yet
        mTerrainMeshes[patchIndex] = std::make_unique<TerrainMesh>();
        mWaterMeshes[patchIndex] = std::make_unique<WaterMesh>();
        assert(patch.mStatus == QUADTREE_PATCH_STATUS_INVALID || patch.mStatus == QUADTREE_PATCH_STATUS_RECOMBINING);
    }
    ++mRefCount;
    assert(!patch.isCrossfading() && !patch.isMeshDirty() && patch.isActive());

    if (lod == FlatQuadtree<TERRAIN_QUADTREE_MAX_LOD, TERRAIN_QUADTREE_WIDTH>::HIGHEST_LOD) {
        // At highest LOD we ask the heightmap generator to handle it
        const ChunkID id = getChunkIDForPatchIndex(patchIndex);
        // Sentinal IDs never mesh
        if (id.isSentinelID()) {
            finishMeshes(patchIndex);
            return;
        }

        if (hasAquired || mWorldGrid->tryAquirePaddedHeightDataAt(id)) {
            // Instantly generate
            Services::Threadpool::ref().addTask([this, &patch, lod, patchIndex, id](ThreadPoolWorkerData*) {
                createMeshes(id, patchIndex, lod);
            }, [this, &patch, patchIndex]() {
                finishMeshes(patchIndex);
            });
        }
        else {
            // Wait for the terrain generator to generate our chunk
            mWorldGrid->requestPaddedHeightDataGenAndAquireAt(id, [this, &patch, lod, patchIndex, id]() {
                Services::Threadpool::ref().addTask([this, &patch, lod, patchIndex, id](ThreadPoolWorkerData*) {
                    createMeshes(id, patchIndex, lod);
                }, [this, &patch, patchIndex]() {
                    finishMeshes(patchIndex);
                });
            });
        }
    }
    else {
        // At lower LODs we have to regenerate every time
        // TODO: we actually shouldnt do this.. it ignores diffs
        Services::Threadpool::ref().addTask([this, &patch, lod, patchIndex](ThreadPoolWorkerData*) {
            createTerrainAndWaterMesh(*mTerrainMeshes[patchIndex], *mWaterMeshes[patchIndex], PATCH_POSITIONS.data[patchIndex].xy, lod, mWorldPos);
        }, [this, &patch, patchIndex]() {
            finishMeshes(patchIndex);
        });
    }
}

void HeightmapTerrainQuadtree::createMeshes(const ChunkID id, ui32 patchIndex, ui32 lod) {
    const HeightmapPatchData* paddedHeightData[9];
    mWorldGrid->getPaddedHeightDataAt(id, paddedHeightData);
    createTerrainAndWaterMesh(*mTerrainMeshes[patchIndex], *mWaterMeshes[patchIndex], PATCH_POSITIONS.data[patchIndex].xy, lod, mWorldPos, paddedHeightData);
}

void HeightmapTerrainQuadtree::finishMeshes(ui32 patchIndex) {
    mTerrainMeshes[patchIndex]->finishMesh(MeshDrawMode::STATIC);
    mWaterMeshes[patchIndex]->finishMesh(MeshDrawMode::STATIC);
    onMeshFinished(patchIndex, mTerrainMeshes[patchIndex]->isValid() || mWaterMeshes[patchIndex]->isValid());
    // Update refcount
    --mRefCount;
}



void HeightmapTerrainQuadtree::freeMeshForPatch(ui32 patchIndex)
{
    // Only highest LOD has reference to heightmap
    if (QUADTREE_LOD_FROM_INDEX[patchIndex] == FlatQuadtree<TERRAIN_QUADTREE_MAX_LOD, TERRAIN_QUADTREE_WIDTH>::HIGHEST_LOD) {
        const ChunkID id = getChunkIDForPatchIndex(patchIndex);
        mWorldGrid->releasePaddedHeightDataAt(id);
    }
    mTerrainMeshes[patchIndex].reset();
    mWaterMeshes[patchIndex].reset();
}
