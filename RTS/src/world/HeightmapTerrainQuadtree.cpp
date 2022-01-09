#include "stdafx.h"
#include "HeightmapTerrainQuadtree.h"

#include "camera/Camera3D.h"
#include "options/DebugOptions.h"
#include "world/WorldGrid.h"

#include "services/Services.h"
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

void HeightmapTerrainQuadtree::render(const Camera3D& camera, const vg::GLProgram& program) const {
    const f32v3& cameraPos = camera.getPosition();
    const f32v2 cameraPos2Drelative = f32v2(cameraPos.x, cameraPos.y) - mWorldPos;
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
            auto& mesh = mMeshes[index];
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
            const f32 radius = LOD_RADIUS_DIMS[lod];
            if (camera.sphereIsVisible(centerPos3d + pos3D, radius)) {
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

void createTerrainMesh(
    TerrainMesh& mesh,
    const ui32v2& posStart,
    ui32 lod,
    const f32v2& worldPos
) {
    const ui32v2& dims = (ui32v2&)FlatQuadtree<TERRAIN_QUADTREE_MAX_LOD, TERRAIN_QUADTREE_WIDTH>::LOD_DIMS[lod];
    f32v2 quadDims = f32v2(dims) / f32v2(TERRAIN_MESH_WIDTH_QUADS);
    mesh.beginMesh(posStart, dims.x);
    // Generate heightfield
    f32 paddedHeightfield[TERRAIN_MESH_PADDED_WIDTH_VERTS][TERRAIN_MESH_PADDED_WIDTH_VERTS];
    for (ui32 y = 0; y < TERRAIN_MESH_PADDED_WIDTH_VERTS; ++y) {
        for (ui32 x = 0; x < TERRAIN_MESH_PADDED_WIDTH_VERTS; ++x) {
            const f32v2 vertPos = f32v2(posStart.x + ((f32)x - 1.0f) * quadDims.x, posStart.y + ((f32)y - 1.0f) * quadDims.y);
            f32 height = sWorldGen.getHeightAtPos(f32v2(vertPos.x + worldPos.x, vertPos.y + worldPos.y));
            paddedHeightfield[y][x] = height;
        }
    }
    mesh.setVertsFromPaddedHeightfield(paddedHeightfield);
};

void createTerrainMesh(
    TerrainMesh& mesh,
    const ui32v2& posStart,
    ui32 lod,
    const f32v2& worldPos,
    const f32* heightData
) {
    const ui32v2& dims = (ui32v2&)FlatQuadtree<TERRAIN_QUADTREE_MAX_LOD, TERRAIN_QUADTREE_WIDTH>::LOD_DIMS[lod];
    f32v2 quadDims = f32v2(dims) / f32v2(TERRAIN_MESH_WIDTH_QUADS);
    mesh.beginMesh(posStart, dims.x);

    f32 paddedHeightfield[TERRAIN_MESH_PADDED_WIDTH_VERTS][TERRAIN_MESH_PADDED_WIDTH_VERTS];

    // Center memcopy row by row
    for (ui32 y = 0; y < TERRAIN_MESH_WIDTH_VERTS; ++y) {
        memcpy(&paddedHeightfield[y + 1][1], &heightData[y * HEIGHTMAP_VERT_WIDTH_PER_CHUNK], sizeof(f32) * HEIGHTMAP_VERT_WIDTH_PER_CHUNK);
    }
    static_assert(TERRAIN_MESH_WIDTH_VERTS == HEIGHTMAP_VERT_WIDTH_PER_CHUNK);

    // === Generate edges ===
    // Left and right edge
    for (int y = 0; y < TERRAIN_MESH_PADDED_WIDTH_VERTS; ++y) {
        { // Left
            constexpr ui32 x = 0;
            const f32v2 vertPos = f32v2(posStart.x + ((f32)x - 1.0f) * quadDims.x, posStart.y + ((f32)y - 1.0f) * quadDims.y);
            paddedHeightfield[y][x] = sWorldGen.getHeightAtPos(f32v2(vertPos.x + worldPos.x, vertPos.y + worldPos.y));
        }
        { // Right
            constexpr ui32 x = TERRAIN_MESH_PADDED_WIDTH_VERTS - 1;
            const f32v2 vertPos = f32v2(posStart.x + ((f32)x - 1.0f) * quadDims.x, posStart.y + ((f32)y - 1.0f) * quadDims.y);
            paddedHeightfield[y][x] = sWorldGen.getHeightAtPos(f32v2(vertPos.x + worldPos.x, vertPos.y + worldPos.y));
        }
    }
    // Bottom and top 
    // -1 cause corners were got by left and right
    for (int x = 1; x < TERRAIN_MESH_PADDED_WIDTH_VERTS - 1; ++x) {
        { // Bottom
            constexpr ui32 y = 0;
            const f32v2 vertPos = f32v2(posStart.x + ((f32)x - 1.0f) * quadDims.x, posStart.y + ((f32)y - 1.0f) * quadDims.y);
            paddedHeightfield[y][x] = sWorldGen.getHeightAtPos(f32v2(vertPos.x + worldPos.x, vertPos.y + worldPos.y));
        }
        { // Top
            constexpr ui32 y = TERRAIN_MESH_PADDED_WIDTH_VERTS - 1;
            const f32v2 vertPos = f32v2(posStart.x + ((f32)x - 1.0f) * quadDims.x, posStart.y + ((f32)y - 1.0f) * quadDims.y);
            paddedHeightfield[y][x] = sWorldGen.getHeightAtPos(f32v2(vertPos.x + worldPos.x, vertPos.y + worldPos.y));
        }
    }
    mesh.setVertsFromPaddedHeightfield(paddedHeightfield);
};

void HeightmapTerrainQuadtree::buildMeshForPatch(QuadtreePatch& patch, ui32 lod, ui32 patchIndex)
{
    if (!mMeshes[patchIndex]) {
        mMeshes[patchIndex] = std::make_unique<TerrainMesh>();
        assert(patch.mStatus == QUADTREE_PATCH_STATUS_INVALID || patch.mStatus == QUADTREE_PATCH_STATUS_RECOMBINING);
    }
    ++mRefCount;
    assert(!patch.isCrossfading() && /*!patch.isMeshing() &&*/ !patch.isMeshDirty() && patch.isActive());

    if (lod == FlatQuadtree<TERRAIN_QUADTREE_MAX_LOD, TERRAIN_QUADTREE_WIDTH>::HIGHEST_LOD) {
        // At highest LOD we ask the heightmap generator to handle it
        const ChunkID id = getChunkIDForPatchIndex(patchIndex);
        if (const f32* heightData = mWorldGrid->tryGetHeightDataAt(id)) {
            mWorldGrid->aquireHeightData(id);
            // Instantly generate
            Services::Threadpool::ref().addTask([this, &patch, lod, patchIndex, id, heightData](ThreadPoolWorkerData*) {
                // PreciseTimer timer;
                createTerrainMesh(*mMeshes[patchIndex], PATCH_POSITIONS.data[patchIndex].xy, lod, mWorldPos, heightData);
                // std::cout << "TERRAIN 2: " << timer.stop() << std::endl;
            }, [this, &patch, patchIndex]() {

                mMeshes[patchIndex]->finishMesh(MeshDrawMode::STATIC);
                onMeshFinished(patchIndex, mMeshes[patchIndex]->isValid());
                // Update refcount
                --mRefCount;
            });
        }
        else {
            // Wait for the terrain generator to generate our chunk
            mWorldGrid->requestHeightDataGenAndAquireAt(id, [this, &patch, lod, patchIndex, id]() {
                const f32* heightData = mWorldGrid->getHeightDataAt(id);
                Services::Threadpool::ref().addTask([this, &patch, lod, patchIndex, id, heightData](ThreadPoolWorkerData*) {
                   // PreciseTimer timer;
                    createTerrainMesh(*mMeshes[patchIndex], PATCH_POSITIONS.data[patchIndex].xy, lod, mWorldPos, heightData);
                   // std::cout << "TERRAIN 2: " << timer.stop() << std::endl;
                }, [this, &patch, patchIndex]() {

                    mMeshes[patchIndex]->finishMesh(MeshDrawMode::STATIC);
                    onMeshFinished(patchIndex, mMeshes[patchIndex]->isValid());
                    // Update refcount
                    --mRefCount;
                });
            });
        }
    }
    else {
        // At lower LODs we have to regenerate every time
        Services::Threadpool::ref().addTask([this, &patch, lod, patchIndex](ThreadPoolWorkerData*) {


           // PreciseTimer timer;
            createTerrainMesh(*mMeshes[patchIndex], PATCH_POSITIONS.data[patchIndex].xy, lod, mWorldPos);
            //std::cout << "TERRAIN 1: " << timer.stop() << std::endl;
        }, [this, &patch, patchIndex]() {

            mMeshes[patchIndex]->finishMesh(MeshDrawMode::STATIC);
            onMeshFinished(patchIndex, mMeshes[patchIndex]->isValid());
            // Update refcount
            --mRefCount;
        });
    }
}

void HeightmapTerrainQuadtree::freeMeshForPatch(ui32 patchIndex)
{
    // Lowest level has reference to heightmap
    if (QUADTREE_LOD_FROM_INDEX[patchIndex] == FlatQuadtree<TERRAIN_QUADTREE_MAX_LOD, TERRAIN_QUADTREE_WIDTH>::HIGHEST_LOD) {
        const ChunkID id = getChunkIDForPatchIndex(patchIndex);
        mWorldGrid->releaseHeightDataAt(id);
    }
    mMeshes[patchIndex].reset();
}
