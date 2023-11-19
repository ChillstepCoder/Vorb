#include "stdafx.h"
#include "ChunkGrassQuadtree.h"

#include "rendering/RenderContext.h"
#include "rendering/mesh/GrassMeshManager.h"
#include "renderdata/WorldRenderDataManager.h"
#include "world/World.h"
#include "world/Chunk.h"
#include "world/IHeightmapGrid.h"
#include "camera/Camera3D.h"

#include "options/DebugOptions.h"

#include "generation/WorldGenerationData.h"
#include <Vorb/graphics/GLProgram.h>

#include "math/Random.h"
#include "debugging/DebugRenderer.h"

#include "rendering/mesh/mesher/builder/GrassMeshBuilderMethods.h"

#include "rendering/RenderThreadTasks.h"
#include "gamethread/GameThreadTasks.h"

#include <boost/pool/singleton_pool.hpp>

constexpr f32 GRASS_SUBDIVIDE_DISTANCES_SQ[GRASS_QUADTREE_MAX_LOD] = { // sqrt(pow(WIDTH, 2) * 2) for diagonal distance widths
    FLT_MAX,
    SQ(91.0f),
    SQ(46.0f),
    SQ(23.0f),
    -FLT_MAX // Never subdivide last
};


struct GrassMeshTaskData {
    GrassMeshTaskData(ChunkGrassQuadtree* owner, ui32 patchIndex, GrassBillboardMesh& mesh) : meshBuilder(mesh), owner(owner), patchIndex(patchIndex) {}

    void* operator new(size_t count);
    void operator delete(void* pointer, size_t size);
    
    GrassBillboardMeshBuilder meshBuilder;
    ChunkGrassQuadtree* owner;
    f32* heightField;
    ui32 patchIndex;
};

// TODO: We should make sure we dont build this on dedicated server as it initializes some memory
struct grass_mesh_task_pool {};
using grass_mesh_singleton_task_pool = boost::singleton_pool<grass_mesh_task_pool, sizeof(GrassMeshTaskData), boost::default_user_allocator_new_delete, boost::details::pool::default_mutex, 64u>;

void* GrassMeshTaskData::operator new(size_t count) {
    UNUSED(count);
    return grass_mesh_singleton_task_pool::malloc();
}

void GrassMeshTaskData::operator delete(void* pointer, size_t size) {
    UNUSED(size);
    return grass_mesh_singleton_task_pool::free(pointer);
}

ChunkGrassQuadtree::ChunkGrassQuadtree(const Chunk& chunk) : mChunk(chunk), FlatQuadtree(chunk.getWorld().getHeightmapGrid(), chunk.getWorldPos(), GRASS_SUBDIVIDE_DISTANCES_SQ, sDebugOptions.mGrassSettings.lodDistanceOffset) {
    mChunk.incRef();
}

ChunkGrassQuadtree::~ChunkGrassQuadtree()
{
    mChunk.decRef();
    // Free all meshes on active nodes
    for (auto&& i : mActiveNodes) {
        freeMeshForPatch(i);
    }
}

bool isPatchInRange(const f32v2& centerPos, const f32v2& cameraPos, f32 radius) {
    return (length2(centerPos - cameraPos) - SQ(radius)) <= sDebugOptions.mGrassSettings.distanceSq - SQ(CHUNK_WIDTH * 0.5f); // SQ chunkwidth half will make it fit more closely (for some reason?)
}

void ChunkGrassQuadtree::buildMeshForPatch(QuadtreePatch& patch, ui32 lod, ui32 patchIndex) {
    ASSERT_GAME_THREAD();
    if (!mMeshes[patchIndex]) {
        mMeshes[patchIndex] = std::make_unique<GrassMesh>(patchIndex);
    }
    ++mRefCount;
    mChunk.incRef();

    assert(!patch.isCrossfading() && !patch.isMeshDirty() && patch.isActive());

    const HeightmapPatchID id = getHeightmapPatchID(patchIndex);
    IHeightmapGrid& heightmapGrid = mChunk.getWorld().getHeightmapGrid();
    const HeightmapPatchData* heightData = heightmapGrid.getHeightDataAt(id);
    // Instantly generate
    Services::Threadpool::ref().addTask([this, &patch, lod, patchIndex, heightData]() {

        //PreciseTimer timer;
        GrassMeshTaskData* taskData = new GrassMeshTaskData(this, patchIndex, mMeshes[patchIndex]->mMesh);
        GrassMeshBuilderMethods::createGrassMesh(taskData->meshBuilder, mChunk, PATCH_POSITIONS.data[patchIndex].xy, lod, heightData);

        // To render thread for upload
        RenderThreadTasks::getInstance().addGenericTask([](RenderContext& context, void* vTaskData) {
            GrassMeshTaskData* taskData = static_cast<GrassMeshTaskData*>(vTaskData);
            taskData->owner->finishMesh(taskData->meshBuilder, taskData->patchIndex);

            // Back to the main thread to update state
            GameThreadTasks::getInstance().addGenericTask([](GameThread&, void* vTaskData) {
                GrassMeshTaskData* taskData = static_cast<GrassMeshTaskData*>(vTaskData);
                ChunkGrassQuadtree* owner = taskData->owner;
                const ui32 patchIndex = taskData->patchIndex;
                owner->onMeshFinished(patchIndex, owner->mMeshes[patchIndex] != nullptr);

                owner->mChunk.decRef();
                --owner->mRefCount;
                // Free resources
                delete taskData;
            }, taskData);
        }, taskData);
    }, nullptr);
   
}

void ChunkGrassQuadtree::freeMeshForPatch(ui32 patchIndex) {
    if (mMeshes[patchIndex]) {

        struct GrassMeshFreeTask {
            GrassMeshFreeTask(std::unique_ptr<GrassMesh>&& grassMesh, World& world) : grassMesh(std::move(grassMesh)), world(world) {}

            std::unique_ptr<GrassMesh> grassMesh;
            World& world;
        };

        ASSERT_GAME_THREAD();
        GrassMeshFreeTask* freeTask = new GrassMeshFreeTask(std::move(mMeshes[patchIndex]), mChunk.getWorld());
        RenderThreadTasks::getInstance().addGenericTask([](RenderContext& context, void* vTaskData) {
            GrassMeshFreeTask* taskData = static_cast<GrassMeshFreeTask*>(vTaskData);
            RenderContext::getInstance().getRenderDataManagerForWorld(taskData->world).getGrassMeshManager().removeGrassMesh(taskData->grassMesh.get());
            delete taskData;
        }, freeTask);
    }
}

void ChunkGrassQuadtree::finishMesh(GrassBillboardMeshBuilder& meshBuilder, ui32 patchIndex) {
    std::unique_ptr<GrassMesh>& mesh = mMeshes[patchIndex];
    assert(&meshBuilder.getMesh() == &mesh->mMesh);
    mesh->mPosition = getWorldPos3D();
    meshBuilder.finishMesh();

    GrassMeshManager& grassMeshManager = RenderContext::getInstance().getRenderDataManagerForWorld(mChunk.getWorld()).getGrassMeshManager();
    if (mesh->mMesh.isValid()) {
        if (!mesh->mHadMesh) {
            assert(mesh->mIndex < ChunkGrassFlatQuadtree::NODE_COUNT);
            grassMeshManager.addGrassMesh(mesh.get());
            mesh->mHadMesh = true;
        }
    }
    else if (mesh->mHadMesh) {
        grassMeshManager.removeGrassMesh(mesh.get());
        mesh.reset();
    }
}
