#include "stdafx.h"
#include "HeightmapTerrainQuadtree.h"

#include "camera/Camera3D.h"
#include "options/DebugOptions.h"
#include "debugging/DebugRenderer.h"
#include "world/World.h"
#include "world/IHeightmapGrid.h"
#include "rendering/renderdata/WorldRenderDataManager.h"
#include "rendering/mesh/mesher/builder/TerrainMeshBuilder.h"
#include "rendering/RenderContext.h"
#include "rendering/RenderThreadTasks.h"
#include "rendering/mesh/TerrainMeshManager.h"

#include "gamethread/GameThreadTasks.h"

#include "generation/IWorldGenerator.h"
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
    ui32 patchIndex;
};

struct TerrainMeshGenTaskData {
    TerrainMeshGenTaskData(World& world, HeightmapTerrainQuadtree* owner, ui32 patchIndex) : world(world), owner(owner), patchIndex(patchIndex) {}

    World& world;
    TerrainMeshBuilder terrainBuilder;
    HeightmapTerrainQuadtree* owner;
    ui32 patchIndex;
    bool isAnyMeshValid;
};


HeightmapTerrainQuadtree::HeightmapTerrainQuadtree(World& world, const f32v2& worldPosition)
    : mWorld(world), FlatQuadtree(world.getHeightmapGrid(), worldPosition, TERRAIN_SUBDIVIDE_DISTANCES_SQ, sDebugOptions.mTerrainLodDistanceOffset) {

}

HeightmapTerrainQuadtree::~HeightmapTerrainQuadtree() {

}

void HeightmapTerrainQuadtree::markDirty() {
    for (ui32 i = 0; i < mNumActiveNodes; ++i) {
        ui32 index = mActiveNodes[i];
        QuadtreePatch& patch = mNodes[index];
        patch.mFlags |= QUADTREE_PATCH_FLAG_DIRTY_MESH;
    }
}

void createTerrainAndWaterMesh(
    World& world,
    TerrainMeshBuilder& terrainBuilder,
    const ui32v2& posStart,
    ui32 lod,
    const f32v2& worldPos
) {
    const ui32v2& dims = (ui32v2&)FlatQuadtree<TERRAIN_QUADTREE_MAX_LOD, TERRAIN_QUADTREE_WIDTH>::LOD_DIMS[lod];
    f32v2 quadDims = f32v2(dims) / f32v2(TERRAIN_MESH_WIDTH_QUADS);
    assert(dims.x == dims.y);

    // Generate heightfield
    IHeightmapGrid& heightGrid = world.getHeightmapGrid();
    CompressedHeight paddedHeightfield[TERRAIN_MESH_PADDED_WIDTH_VERTS][TERRAIN_MESH_PADDED_WIDTH_VERTS];
    for (ui32 y = 0; y < TERRAIN_MESH_PADDED_WIDTH_VERTS; ++y) {
        for (ui32 x = 0; x < TERRAIN_MESH_PADDED_WIDTH_VERTS; ++x) {
            const f32v2 vertPos = f32v2(posStart.x + ((f32)x - 1.0f) * quadDims.x, posStart.y + ((f32)y - 1.0f) * quadDims.y);
            const f32 zPos = heightGrid.computeHeightAtPoint<true>(f32v2(vertPos.x + worldPos.x, vertPos.y + worldPos.y));
            paddedHeightfield[y][x] = compressHeight(zPos);
        }
    }
    terrainBuilder.buildFromPaddedHeightfield(worldPos, posStart, dims.x, paddedHeightfield);
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
    ASSERT_GAME_THREAD();
    if (!mTerrainMeshes[patchIndex] || !mWaterMeshes[patchIndex]) {
        mTerrainMeshes[patchIndex] = std::make_unique<TerrainMesh>(patchIndex);
        mWaterMeshes[patchIndex] = std::make_unique<TerrainMesh>(patchIndex);
        assert(patch.mStatus == QUADTREE_PATCH_STATUS_INVALID || patch.mStatus == QUADTREE_PATCH_STATUS_RECOMBINING);
    }
    assert(!patch.isCrossfading() && !patch.isMeshDirty() && patch.isActive());

    TerrainMeshGenTaskData* taskData = new TerrainMeshGenTaskData(mWorld, this, patchIndex);
    Services::Threadpool::ref().addTask([this, lod, taskData]() {
        createTerrainAndWaterMesh(taskData->world, taskData->terrainBuilder, PATCH_POSITIONS.data[taskData->patchIndex].xy, lod, mWorldPos);

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

void HeightmapTerrainQuadtree::finishMeshes(TerrainMeshBuilder& terrainBuilder, ui32 patchIndex) {
    const f32v3& worldPos = f32v3(mWorldPos.x, mWorldPos.y, 0.0f);
    const bool hadTerrain = mTerrainMeshes[patchIndex]->isValid();
    const bool hadWater = mWaterMeshes[patchIndex]->isValid();
    terrainBuilder.finishMeshes(*mTerrainMeshes[patchIndex], *mWaterMeshes[patchIndex], worldPos);
    // TODO: if this can happen, we need to store a "has acquired" bit since right now we are using existence of a mesh to determine if we acquired
    assert(mTerrainMeshes[patchIndex] || mWaterMeshes[patchIndex]);

    TerrainMeshManager& terrainMeshManager = RenderContext::getInstance().getRenderDataManagerForWorld(mWorld).getTerrainMeshManager();
    if (mTerrainMeshes[patchIndex]->isValid()) {
        if (!hadTerrain) {
            terrainMeshManager.addTerrainMesh(mTerrainMeshes[patchIndex].get());
        }
    }
    else if (hadTerrain) {
        terrainMeshManager.removeTerrainMesh(mTerrainMeshes[patchIndex].get());
    }

    if (mWaterMeshes[patchIndex]->isValid()) {
        if (!hadTerrain) {
            terrainMeshManager.addTerrainWaterMesh(mWaterMeshes[patchIndex].get());
        }
    }
    else if (hadTerrain) {
        terrainMeshManager.removeTerrainWaterMesh(mWaterMeshes[patchIndex].get());
    }
}

void HeightmapTerrainQuadtree::freeMeshForPatch(ui32 patchIndex)
{
    ASSERT_GAME_THREAD();

    struct TerrainMeshFreeTask {
        TerrainMeshFreeTask(std::unique_ptr<TerrainMesh>&& terrainMesh, std::unique_ptr<TerrainMesh>&& waterMesh, World& world) : terrainMesh(std::move(terrainMesh)), waterMesh(std::move(waterMesh)), world(world) {}

        std::unique_ptr<TerrainMesh> terrainMesh;
        std::unique_ptr<TerrainMesh> waterMesh;
        World& world;
    };

    TerrainMeshFreeTask* freeTask = new TerrainMeshFreeTask(std::move(mTerrainMeshes[patchIndex]), std::move(mWaterMeshes[patchIndex]), mWorld);
    RenderThreadTasks::getInstance().addGenericTask([](RenderContext& context, void* vTaskData) {
        TerrainMeshFreeTask* taskData = static_cast<TerrainMeshFreeTask*>(vTaskData);
        TerrainMeshManager& terrainMeshManager = RenderContext::getInstance().getRenderDataManagerForWorld(taskData->world).getTerrainMeshManager();
        if (taskData->terrainMesh) {
            terrainMeshManager.removeTerrainMesh(taskData->terrainMesh.get());
        }
        if (taskData->waterMesh) {
            terrainMeshManager.removeTerrainWaterMesh(taskData->waterMesh.get());
        }
        delete taskData;
    }, freeTask);
}
