#include "stdafx.h"
#include "HeightmapTerrainQuadtree.h"

#include "camera/Camera3D.h"
#include "options/DebugOptions.h"
#include "debugging/DebugRenderer.h"
#include "world/IWorld.h"
#include "world/IHeightmapGrid.h"
#include "rendering/mesh/mesher/builder/TerrainMeshBuilder.h"
#include "rendering/RenderContext.h"
#include "rendering/RenderThreadTasks.h"

#include "gamethread/GameThreadTasks.h"

#include "generation/WorldGeneration.h"
#include <Vorb/graphics/GLProgram.h>

constexpr f32 TERRAIN_SUBDIVIDE_DISTANCES_SQ[TERRAIN_QUADTREE_MAX_LOD] = { // sqrt(pow(WIDTH, 2) * 2) for diagonal distance widths
    SQ(6000.0f),
    SQ(3000.0f),
    SQ(600.0f),
    SQ(64.0f),
    -FLT_MAX // Never subdivide last
};

struct TerrainMeshTaskData {
    TerrainMeshTaskData(HeightmapTerrainQuadtree* owner, ui32 patchIndex) : owner(owner), patchIndex(patchIndex) {}

    TerrainMeshBuilder terrainBuilder;
    HeightmapTerrainQuadtree* owner;
    f32 paddedHeightfield[TERRAIN_MESH_PADDED_WIDTH_VERTS][TERRAIN_MESH_PADDED_WIDTH_VERTS];
    ui32 patchIndex;
};

struct TerrainMeshGenTaskData {
    TerrainMeshGenTaskData(HeightmapTerrainQuadtree* owner, ui32 patchIndex) : owner(owner), patchIndex(patchIndex) {}

    TerrainMeshBuilder terrainBuilder;
    HeightmapTerrainQuadtree* owner;
    ui32 patchIndex;
    bool isAnyMeshValid;
};

struct TerrainMeshFreeTask {
    TerrainMeshFreeTask(std::unique_ptr<TerrainMesh>&& terrainMesh, std::unique_ptr<TerrainMesh>&& waterMesh) : terrainMesh(std::move(terrainMesh)), waterMesh(std::move(waterMesh)) {}

    std::unique_ptr<TerrainMesh> terrainMesh;
    std::unique_ptr<TerrainMesh> waterMesh;
};

HeightmapTerrainQuadtree::HeightmapTerrainQuadtree() : FlatQuadtree(f32v2(0.0f), TERRAIN_SUBDIVIDE_DISTANCES_SQ, sDebugOptions.mTerrainLodDistanceOffset) {

}

HeightmapTerrainQuadtree::~HeightmapTerrainQuadtree() {

}

void HeightmapTerrainQuadtree::init(const f32v2& worldPosition) {
    mWorldPos = worldPosition;
}

void HeightmapTerrainQuadtree::markDirty() {
    for (ui32 i = 0; i < mNumActiveNodes; ++i) {
        ui32 index = mActiveNodes[i];
        QuadtreePatch& patch = mNodes[index];
        patch.mFlags |= QUADTREE_PATCH_FLAG_DIRTY_MESH;
    }
}


void createTerrainAndWaterMeshFromGen(
    TerrainMeshBuilder& terrainBuilder,
    const ui32v2& posStart,
    ui32 lod,
    const f32v2& worldPos
) {
    const ui32v2& dims = (ui32v2&)FlatQuadtree<TERRAIN_QUADTREE_MAX_LOD, TERRAIN_QUADTREE_WIDTH>::LOD_DIMS[lod];
    f32v2 quadDims = f32v2(dims) / f32v2(TERRAIN_MESH_WIDTH_QUADS);
    assert(dims.x == dims.y);

    // Generate heightfield
    f32 paddedHeightfield[TERRAIN_MESH_PADDED_WIDTH_VERTS][TERRAIN_MESH_PADDED_WIDTH_VERTS];
    for (ui32 y = 0; y < TERRAIN_MESH_PADDED_WIDTH_VERTS; ++y) {
        for (ui32 x = 0; x < TERRAIN_MESH_PADDED_WIDTH_VERTS; ++x) {
            const f32v2 vertPos = f32v2(posStart.x + ((f32)x - 1.0f) * quadDims.x, posStart.y + ((f32)y - 1.0f) * quadDims.y);
            f32 zPos = sWorldGen.getHeightAtPos(f32v2(vertPos.x + worldPos.x, vertPos.y + worldPos.y));
            paddedHeightfield[y][x] = zPos;
        }
    }
    terrainBuilder.buildFromPaddedHeightfield(posStart, dims.x, paddedHeightfield);
};

void createTerrainAndWaterMesh(
    TerrainMeshBuilder& terrainBuilder,
    const ui32v2& posStart,
    ui32 lod,
    const f32v2& worldPos,
    const f32 paddedHeightfield[TERRAIN_MESH_PADDED_WIDTH_VERTS][TERRAIN_MESH_PADDED_WIDTH_VERTS]
) {
    const ui32v2& dims = (ui32v2&)FlatQuadtree<TERRAIN_QUADTREE_MAX_LOD, TERRAIN_QUADTREE_WIDTH>::LOD_DIMS[lod];
    f32v2 patchWorldPos = worldPos + f32v2(posStart);

    // Build
    terrainBuilder.buildFromPaddedHeightfield(posStart, (f32)dims.x, paddedHeightfield);
};

void HeightmapTerrainQuadtree::resetCrossfadeRenderForPatch(ui32 patchIndex, int crossfadeDir, f32 crossfadeAlpha) {
    auto& mesh = mTerrainMeshes[patchIndex];
    assert(mesh);
    mesh->mCrossfadeAlpha = crossfadeAlpha;
    mesh->mCrossfadeDir = crossfadeDir;
}

void HeightmapTerrainQuadtree::updateCrossfadeRenderForPatch(ui32 patchIndex, f32 crossfadeAlpha) {
    auto& mesh = mTerrainMeshes[patchIndex];
    assert(mesh);
    mesh->mCrossfadeAlpha = crossfadeAlpha;
}

void HeightmapTerrainQuadtree::buildMeshForPatch(QuadtreePatch& patch, ui32 lod, ui32 patchIndex)
{
    assert(IS_GAME_THREAD());
    bool hasAquired = true;
    if (!mTerrainMeshes[patchIndex] || !mWaterMeshes[patchIndex]) {
        hasAquired = false; // If we dont have a mesh, we haven't aquired yet
        mTerrainMeshes[patchIndex] = std::make_unique<TerrainMesh>(patchIndex);
        mWaterMeshes[patchIndex] = std::make_unique<TerrainMesh>(patchIndex);
        assert(patch.mStatus == QUADTREE_PATCH_STATUS_INVALID || patch.mStatus == QUADTREE_PATCH_STATUS_RECOMBINING);
    }
    assert(!patch.isCrossfading() && !patch.isMeshDirty() && patch.isActive());

    if (lod == FlatQuadtree<TERRAIN_QUADTREE_MAX_LOD, TERRAIN_QUADTREE_WIDTH>::HIGHEST_LOD) {
        // At highest LOD we ask the heightmap generator to handle it
        const HeightmapPatchID id = getHeightmapPatchID(patchIndex);
        // Sentinal IDs never mesh
        if (id.isSentinelID()) {
            onMeshFinished(patchIndex, false);
            return;
        }

        TerrainMeshTaskData* taskData = new TerrainMeshTaskData(this, patchIndex);

        if (hasAquired || sHeightmapGrid->tryAquirePaddedHeightDataAt(id)) {
            createMeshesHighestLOD(taskData);
        }
        else {
            // Wait for the terrain generator to generate our chunk
            // TODO: No std::function
            sHeightmapGrid->requestPaddedHeightDataGenAndAquireAt(id, [this, taskData]() {
                createMeshesHighestLOD(taskData);
            });
        }
    }
    else {
        TerrainMeshGenTaskData* taskData = new TerrainMeshGenTaskData(this, patchIndex);
        // At lower LODs we have to regenerate every time
        // TODO: we actually shouldnt do this.. it ignores diffs
        // Generate mesh data on worker thread
        Services::Threadpool::ref().addTask([this, lod, taskData](ThreadPoolWorkerData*) {
            createTerrainAndWaterMeshFromGen(taskData->terrainBuilder, PATCH_POSITIONS.data[taskData->patchIndex].xy, lod, mWorldPos);

            // To render thread for upload
            RenderThreadTasks::getInstance().addGenericTask([](RenderContext& context, void* vTaskData) {
                TerrainMeshGenTaskData* taskData = static_cast<TerrainMeshGenTaskData*>(vTaskData);
                HeightmapTerrainQuadtree* owner = taskData->owner;
                owner->finishMeshes(taskData->terrainBuilder, taskData->patchIndex);
                taskData->isAnyMeshValid = owner->mTerrainMeshes[taskData->patchIndex] || owner->mWaterMeshes[taskData->patchIndex];

                // Back to the main thread to update state
                GameThreadTasks::getInstance().addGenericTask([](GameThread&, void* vTaskData) {
                    TerrainMeshGenTaskData* taskData = static_cast<TerrainMeshGenTaskData*>(vTaskData);
                    taskData->owner->onMeshFinished(taskData->patchIndex, taskData->isAnyMeshValid);
                    // Free resources
                    delete taskData;
                }, taskData);
            }, taskData);
        }, nullptr);
    }
}

void HeightmapTerrainQuadtree::createMeshesHighestLOD(TerrainMeshTaskData* taskData) {

    f32v2 patchWorldPos = mWorldPos + f32v2(PATCH_POSITIONS.data[taskData->patchIndex].xy);
    ui32v2 intWorldPos(glm::round(patchWorldPos));

    const ui32 quadWidth = HEIGHTMAP_QUAD_SIZE;
    intWorldPos -= quadWidth; // Padding so we start on the side
    for (int y = 0; y < TERRAIN_MESH_PADDED_WIDTH_VERTS; ++y) {
        sHeightmapGrid->copyHeightRowToBuffer(taskData->paddedHeightfield[y], intWorldPos, TERRAIN_MESH_PADDED_WIDTH_VERTS);
        intWorldPos.y += quadWidth;
    }

    // Generate mesh data on worker thread
    Services::Threadpool::ref().addTask([this, taskData](ThreadPoolWorkerData*) {
        createTerrainAndWaterMesh(taskData->terrainBuilder, PATCH_POSITIONS.data[taskData->patchIndex].xy, FlatQuadtree<TERRAIN_QUADTREE_MAX_LOD, TERRAIN_QUADTREE_WIDTH>::HIGHEST_LOD, mWorldPos, taskData->paddedHeightfield);

        // To render thread to upload
        RenderThreadTasks::getInstance().addGenericTask([](RenderContext& context, void* vTaskData) {
            TerrainMeshTaskData* taskData = static_cast<TerrainMeshTaskData*>(vTaskData);
            taskData->owner->finishMeshes(taskData->terrainBuilder, taskData->patchIndex);

            // Back to the main thread to update state
            GameThreadTasks::getInstance().addGenericTask([](GameThread&, void* vTaskData) {
                TerrainMeshTaskData* taskData = static_cast<TerrainMeshTaskData*>(vTaskData);
                HeightmapTerrainQuadtree* owner = taskData->owner;
                const bool isMeshValid = owner->mTerrainMeshes[taskData->patchIndex] || owner->mWaterMeshes[taskData->patchIndex];
                taskData->owner->onMeshFinished(taskData->patchIndex, isMeshValid);
                // Free resources
                delete taskData;
            }, taskData);
        }, taskData);
    }, nullptr);

}

void HeightmapTerrainQuadtree::finishMeshes(TerrainMeshBuilder& terrainBuilder, ui32 patchIndex) {
    const f32v3& worldPos = f32v3(mWorldPos.x, mWorldPos.y, 0.0f);
    const bool hadTerrain = mTerrainMeshes[patchIndex]->mMesh.isValid();
    const bool hadWater = mWaterMeshes[patchIndex]->mMesh.isValid();
    terrainBuilder.finishMeshes(mTerrainMeshes[patchIndex]->mMesh, mWaterMeshes[patchIndex]->mMesh, worldPos);
    // TODO: if this can happen, we need to store a "has acquired" bit since right now we are using existence of a mesh to determine if we acquired
    assert(mTerrainMeshes[patchIndex] || mWaterMeshes[patchIndex]);

    if (mTerrainMeshes[patchIndex]->mMesh.isValid()) {
        if (!hadTerrain) {
            RenderContext::getInstance().addTerrainMesh(mTerrainMeshes[patchIndex].get());
        }
    }
    else if (hadTerrain) {
        RenderContext::getInstance().removeTerrainMesh(mTerrainMeshes[patchIndex].get());
    }

    if (mWaterMeshes[patchIndex]->mMesh.isValid()) {
        if (!hadTerrain) {
            RenderContext::getInstance().addTerrainWaterMesh(mWaterMeshes[patchIndex].get());
        }
    }
    else if (hadTerrain) {
        RenderContext::getInstance().removeTerrainWaterMesh(mWaterMeshes[patchIndex].get());
    }
}

void HeightmapTerrainQuadtree::freeMeshForPatch(ui32 patchIndex)
{
    assert(IS_GAME_THREAD());
    // Only highest LOD has reference to heightmap
    if (QUADTREE_LOD_FROM_INDEX[patchIndex] == FlatQuadtree<TERRAIN_QUADTREE_MAX_LOD, TERRAIN_QUADTREE_WIDTH>::HIGHEST_LOD) {
        const HeightmapPatchID id = getHeightmapPatchID(patchIndex);
        sHeightmapGrid->releasePaddedHeightDataAt(id);
    }
    TerrainMeshFreeTask* freeTask = new TerrainMeshFreeTask(std::move(mTerrainMeshes[patchIndex]), std::move(mWaterMeshes[patchIndex]));
    RenderThreadTasks::getInstance().addGenericTask([](RenderContext& context, void* vTaskData) {
        TerrainMeshFreeTask* taskData = static_cast<TerrainMeshFreeTask*>(vTaskData);
        if (taskData->terrainMesh) {
            RenderContext::getInstance().removeTerrainMesh(taskData->terrainMesh.get());
        }
        if (taskData->waterMesh) {
            RenderContext::getInstance().removeTerrainWaterMesh(taskData->waterMesh.get());
        }
        delete taskData;
    }, freeTask);
}
