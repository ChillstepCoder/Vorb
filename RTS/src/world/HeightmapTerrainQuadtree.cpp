#include "stdafx.h"
#include "HeightmapTerrainQuadtree.h"

#include "camera/Camera3D.h"
#include "rendering/QuadMesh.h"
#include "options/DebugOptions.h"

#include "services/Services.h"
#include <Vorb/graphics/GLProgram.h>

constexpr f32 TERRAIN_SUBDIVIDE_DISTANCES_SQ[GRASS_QUADTREE_MAX_LOD] = { // sqrt(pow(WIDTH, 2) * 2) for diagonal distance widths
    SQ(4096.0f),
    SQ(2048.0f),
    SQ(512.0f),
    SQ(64.0f),
    -FLT_MAX // Never subdivide last
};


HeightmapTerrainQuadtree::HeightmapTerrainQuadtree() : FlatQuadtree(f32v2(0.0f), TERRAIN_SUBDIVIDE_DISTANCES_SQ, sDebugOptions.mTerrainLodDistanceOffset) {

}

void HeightmapTerrainQuadtree::init(const f32v2& worldPosition)
{
    mWorldPos = worldPosition;
}

void HeightmapTerrainQuadtree::render(const Camera3D& camera, const vg::GLProgram& program) const {
    const f32v3& cameraPos = camera.getPosition();
    const f32v2 cameraPos2Drelative = f32v2(cameraPos.x, cameraPos.y) - mWorldPos;
    //VGUniform crossfadeAlphaUniform = program.getUniform("unCrossfadeAlpha"); // TODO: Cache?
    //VGUniform crossfadeDirectionUniform = program.getUniform("unCrossfadeDirection");
    f32v3 pos3D(mWorldPos.x, mWorldPos.y, 0.0f);
    for (ui32 i = 0; i < mNumActiveNodes; ++i) {
        ui32 index = mActiveNodes[i];
        const QuadtreePatch& patch = mNodes[index];

        if (patch.canRender()) {
            auto& mesh = mMeshes[index];
            ui32 lod = QUADTREE_LOD_FROM_INDEX[index];
            f32v2 centerPos = f32v2(PATCH_POSITIONS.data[index].xy) + f32v2(LOD_HALF_DIMS[lod].xy);
            f32v3 centerPos3d(centerPos.x, centerPos.y, 0.0f);
            //if (patch.isCrossfading()) {
            //    glUniform1f(crossfadeAlphaUniform, mCrossfadeTable[patch.mCrossFadeTableIndex] * 0.5f /* Constant that was selected via trial and error*/);
            //    glUniform1f(crossfadeDirectionUniform, patch.mFlags & QUADTREE_PATCH_FLAG_CROSSFADING_IN ? 1.0f : 0.0f);
            //}
            //else {
            //    glUniform1f(crossfadeAlphaUniform, 0.0f);
            //    glUniform1f(crossfadeDirectionUniform, 0.0f);
            //}
            const f32 radius = LOD_RADIUS_DIMS[lod];
            if (camera.sphereIsVisible(centerPos3d + pos3D, radius)) {
                mesh->draw(program);
            }
        }
    }
}

void HeightmapTerrainQuadtree::buildMeshForPatch(QuadtreePatch& patch, ui32 lod, ui32 patchIndex)
{
    if (!mMeshes[patchIndex]) {
        mMeshes[patchIndex] = std::make_unique<QuadMesh>();
        assert(patch.mStatus == QUADTREE_PATCH_STATUS_INVALID || patch.mStatus == QUADTREE_PATCH_STATUS_RECOMBINING);
    }
    ++mRefCount;

    assert(!patch.isCrossfading() && /*!patch.isMeshing() &&*/ !patch.isMeshDirty() && patch.isActive());

    Services::Threadpool::ref().addTask([this, &patch, lod, patchIndex](ThreadPoolWorkerData*) {

        //PreciseTimer timer;

        //createGrassMesh(*mMeshes[patchIndex], mChunk, PATCH_POSITIONS.data[patchIndex].xy, lod);

        //std::cout << "TERRAIN: " << lod << " " << timer.stop() << std::endl;
    }, [this, &patch, patchIndex]() {

        mMeshes[patchIndex]->finishMesh(MeshDrawMode::STATIC);

        onMeshFinished(patchIndex, mMeshes[patchIndex]->isValid());

        // Update refcount
        --mRefCount;
    });
}

void HeightmapTerrainQuadtree::freeMeshForPatch(ui32 patchIndex)
{
    mMeshes[patchIndex].reset();
}
