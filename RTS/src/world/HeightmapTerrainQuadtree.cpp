#include "stdafx.h"
#include "HeightmapTerrainQuadtree.h"

#include "camera/Camera3D.h"
#include "options/DebugOptions.h"
#include "debugging/DebugRenderer.h"
#include "world/World.h"
#include "world/IHeightmapGrid.h"
#include "world/road/RoadGrid.h"
#include "rendering/renderdata/WorldRenderDataManager.h"
#include "rendering/mesh/mesher/builder/TerrainMeshBuilder.h"
#include "rendering/RenderContext.h"
#include "rendering/RenderThreadTasks.h"
#include "rendering/mesh/TerrainMeshManager.h"

#include "gamethread/GameThreadTasks.h"

#include "generation/ChunkGenerator.h"
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


HeightmapTerrainQuadtree::HeightmapTerrainQuadtree(World& world, i32v2 worldPosition)
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
    i32v2 posStart,
    ui32 lod,
    i32v2 worldPos
) {
    const ui32v2& dims = (ui32v2&)FlatQuadtree<TERRAIN_QUADTREE_MAX_LOD, TERRAIN_QUADTREE_WIDTH>::LOD_DIMS[lod];
    f32v2 quadDims = f32v2(dims) / f32v2(TERRAIN_MESH_WIDTH_QUADS);
    assert(dims.x == dims.y);

    // Generate heightfield
    RoadGrid& roadGrid = world.getRoadGrid();
    IHeightmapGrid& heightGrid = world.getHeightmapGrid();
    CompressedHeight paddedHeightfield[TERRAIN_MESH_PADDED_WIDTH_VERTS][TERRAIN_MESH_PADDED_WIDTH_VERTS];
    for (ui32 y = 0; y < TERRAIN_MESH_PADDED_WIDTH_VERTS; ++y) {
        for (ui32 x = 0; x < TERRAIN_MESH_PADDED_WIDTH_VERTS; ++x) {
            const f32v2 vertOffset = f32v2(posStart.x + ((f32)x - 1.0f) * quadDims.x, posStart.y + ((f32)y - 1.0f) * quadDims.y);
            const f32v2 trueWorldPos = f32v2(vertOffset.x + worldPos.x, vertOffset.y + worldPos.y);
            DTileCoord dTilePos = DTileCoord::fromTilePosRound(trueWorldPos);
            dTilePos.v = glm::clamp(dTilePos.v, 0, (i32)(world.getWidthDTiles() - 1));
            paddedHeightfield[y][x] = heightGrid.getCompressedHeightAtVert<true>(dTilePos);

            TerrainSurfaceData& surfaceData = terrainBuilder.mTerrainSurfaceLayers[y][x];
            surfaceData.baseTexture = roadGrid.getRoadPoint<true>(dTilePos).type;
        }
    }

    // Second pass, gather surface data densities for inner points
    // See TerrainRenderer::buildSurfaceDensityGradientMaps for details
    for (ui32 y = 1; y < TERRAIN_MESH_PADDED_WIDTH_VERTS - 1; ++y) {
        for (ui32 x = 1; x < TERRAIN_MESH_PADDED_WIDTH_VERTS - 1; ++x) {
            TerrainSurfaceData& surfaceData = terrainBuilder.mTerrainSurfaceLayers[y][x];
            surfaceData.baseDensityTextureID |= (ui8(terrainBuilder.mTerrainSurfaceLayers[y - 1][x - 1].baseTexture == surfaceData.baseTexture) << 0);
            surfaceData.baseDensityTextureID |= (ui8(terrainBuilder.mTerrainSurfaceLayers[y - 1][x].baseTexture == surfaceData.baseTexture) << 1);
            surfaceData.baseDensityTextureID |= (ui8(terrainBuilder.mTerrainSurfaceLayers[y - 1][x + 1].baseTexture == surfaceData.baseTexture) << 2);
            surfaceData.baseDensityTextureID |= (ui8(terrainBuilder.mTerrainSurfaceLayers[y][x - 1].baseTexture == surfaceData.baseTexture) << 3);
            surfaceData.baseDensityTextureID |= (ui8(terrainBuilder.mTerrainSurfaceLayers[y][x + 1].baseTexture == surfaceData.baseTexture) << 4);
            surfaceData.baseDensityTextureID |= (ui8(terrainBuilder.mTerrainSurfaceLayers[y + 1][x - 1].baseTexture == surfaceData.baseTexture) << 5);
            surfaceData.baseDensityTextureID |= (ui8(terrainBuilder.mTerrainSurfaceLayers[y + 1][x].baseTexture == surfaceData.baseTexture) << 6);
            surfaceData.baseDensityTextureID |= (ui8(terrainBuilder.mTerrainSurfaceLayers[y + 1][x + 1].baseTexture == surfaceData.baseTexture) << 7);
        }
    }

    // TODO: Properly calculate edge surface densitys!

    terrainBuilder.buildFromPaddedHeightfield(worldPos, posStart, dims.x, paddedHeightfield, world.getRoadGrid());
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
    ASSERT_RENDER_THREAD();
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
            GameThreadTasks::getInstance().addGenericTask([taskData]() {
                
                taskData->owner->onMeshFinished(taskData->patchIndex, taskData->isAnyMeshValid);
                // Free resources
                delete taskData;
            });
        }, taskData);
    });
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

void HeightmapTerrainQuadtree::freeMeshForPatch(ui32 patchIndex) {
    ASSERT_RENDER_THREAD();
    TerrainMeshManager& terrainMeshManager = RenderContext::getInstance().getRenderDataManagerForWorld(mWorld).getTerrainMeshManager();
    if (mTerrainMeshes[patchIndex]) {
        terrainMeshManager.removeTerrainMesh(mTerrainMeshes[patchIndex].get());
        mTerrainMeshes[patchIndex].reset();
    }
    if (mWaterMeshes[patchIndex]) {
        terrainMeshManager.removeTerrainWaterMesh(mWaterMeshes[patchIndex].get());
        mWaterMeshes[patchIndex].reset();
    }
}
