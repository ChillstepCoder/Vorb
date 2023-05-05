#include "stdafx.h"
#include "ChunkGrassQuadtree.h"

#include "rendering/RenderContext.h"
#include "rendering/mesh/GrassMeshManager.h"
#include "renderdata/WorldRenderDataManager.h"
#include "world/IWorld.h"
#include "world/Chunk.h"
#include "world/IHeightmapGrid.h"
#include "camera/Camera3D.h"

#include "options/DebugOptions.h"

#include "generation/WorldGenerationData.h"
#include <Vorb/graphics/GLProgram.h>

#include "math/Random.h"
#include "debugging/DebugRenderer.h"

#include "rendering/mesh/mesher/builder/GrassMeshBuilder.h"

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
    GrassMeshTaskData(ChunkGrassQuadtree* owner, ui32 patchIndex) : owner(owner), patchIndex(patchIndex) {}

    void* operator new(size_t count);
    void operator delete(void* pointer, size_t size);

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


//void ChunkGrassQuadtree::render(const Camera3D& camera, const vg::GLProgram& program) const {
//    const f32v3& cameraPos = camera.getPosition();
//    const f32v2 cameraPos2Drelative = f32v2(cameraPos.x, cameraPos.y) - mWorldPos;
//    VGUniform crossfadeAlphaUniform = program.getUniform("unCrossfadeAlpha"); // TODO: Cache?
//    VGUniform crossfadeDirectionUniform = program.getUniform("unCrossfadeDirection");
//    f32v3 pos3D(mWorldPos.x, mWorldPos.y, 0.0f);
//    for (ui32 i = 0; i < mNumActiveNodes; ++i) {
//        ui32 index = mActiveNodes[i];
//        const QuadtreePatch& patch = mNodes[index];
//
//        if (patch.canRender()) {
//            auto& mesh = mMeshes[index];
//            ui32 lod = QUADTREE_LOD_FROM_INDEX[index];
//            f32v2 centerPos = f32v2(PATCH_POSITIONS.data[index].xy) + f32v2(LOD_HALF_DIMS[lod].xy);
//            f32v3 centerPos3d(centerPos.x, centerPos.y, 0.0f);
//            if (patch.isCrossfading()) {
//                glUniform1f(crossfadeAlphaUniform, mCrossfadeTable[patch.mCrossFadeTableIndex] * 0.5f /* Constant that was selected via trial and error*/);
//                glUniform1f(crossfadeDirectionUniform, patch.mFlags & QUADTREE_PATCH_FLAG_CROSSFADING_IN ? 1.0f : 0.0f);
//            }
//            else {
//                glUniform1f(crossfadeAlphaUniform, 0.0f);
//                glUniform1f(crossfadeDirectionUniform, 0.0f);
//            }
//            const f32 radius = LOD_RADIUS_DIMS[lod];
//            const BoundingSphere& bounds = mesh->getBoundingSphere();
//            if (camera.sphereIsVisible(bounds.center, bounds.radius) &&
//                isPatchInRange(centerPos, cameraPos2Drelative, radius)) {
//                mesh->draw(program);
//            }
//        }
//    }
//}

void ChunkGrassQuadtree::buildMeshForPatch(QuadtreePatch& patch, ui32 lod, ui32 patchIndex) {
    ASSERT_GAME_THREAD();
    bool hasAquired = true;
    if (!mMeshes[patchIndex]) {
        hasAquired = false;
        mMeshes[patchIndex] = std::make_unique<GrassMesh>(patchIndex);
    }
    ++mRefCount;
    mChunk.incRef();

    assert(!patch.isCrossfading() && !patch.isMeshDirty() && patch.isActive());

    const HeightmapPatchID id = getHeightmapPatchID(patchIndex);
    IHeightmapGrid& heightmapGrid = mChunk.getWorld().getHeightmapGrid();
    if (const HeightmapPatchData* heightData = heightmapGrid.tryGetHeightDataAt(id)) {
        if (!hasAquired) {
            heightmapGrid.aquireHeightData(id);
        }
        // Instantly generate
        Services::Threadpool::ref().addTask([this, &patch, lod, patchIndex, heightData](ThreadPoolWorkerData*) {

            //PreciseTimer timer;
            GrassMeshBuilder::createGrassMesh(mMeshes[patchIndex]->mMesh, mChunk, PATCH_POSITIONS.data[patchIndex].xy, lod, heightData);
            GrassMeshTaskData* taskData = new GrassMeshTaskData(this, patchIndex);

            // To render thread for upload
            RenderThreadTasks::getInstance().addGenericTask([](RenderContext& context, void* vTaskData) {
                GrassMeshTaskData* taskData = static_cast<GrassMeshTaskData*>(vTaskData);
                taskData->owner->finishMesh(taskData->patchIndex);

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
    else {
        assert(false); // This should be impossible because we depend on chunk already having aquired the terrain earlier
        //assert(!hasAquired);
        //// Wait for the terrain generator to generate our chunk
        //sHeightmapGrid->requestHeightDataGenAndAquireAt(id, [this, &patch, lod, patchIndex, id]() {
        //    const HeightmapPatchData* heightData = sHeightmapGrid->getHeightDataAt(id);
        //    Services::Threadpool::ref().addTask([this, &patch, lod, patchIndex, heightData](ThreadPoolWorkerData*) {

        //        //PreciseTimer timer;
        //        createGrassMesh(*mMeshes[patchIndex], mChunk, PATCH_POSITIONS.data[patchIndex].xy, lod, heightData);
        //        mChunk.decRef();
        //        //std::cout << "GRASS: " << lod << " " << timer.stop() << std::endl;
        //    }, [this, &patch, patchIndex]() {

        //        mMeshes[patchIndex]->finishMesh(MeshDrawMode::STATIC);
        //        onMeshFinished(patchIndex, mMeshes[patchIndex]->isValid());
        //        // Update refcount
        //        --mRefCount;
        //    });

        //});
    }
}

void ChunkGrassQuadtree::freeMeshForPatch(ui32 patchIndex) {
    if (mMeshes[patchIndex]) {
        const HeightmapPatchID id = getHeightmapPatchID(patchIndex);
        mChunk.getWorld().getHeightmapGrid().releaseHeightDataAt(id);

        struct GrassMeshFreeTask {
            GrassMeshFreeTask(std::unique_ptr<GrassMesh>&& grassMesh, IWorld& world) : grassMesh(std::move(grassMesh)), world(world) {}

            std::unique_ptr<GrassMesh> grassMesh;
            IWorld& world;
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

void ChunkGrassQuadtree::finishMesh(ui32 patchIndex) {
    std::unique_ptr<GrassMesh>& mesh = mMeshes[patchIndex];
    mesh->mPosition = getWorldPos3D();
    mesh->mMesh.finishMesh(MeshDrawMode::STATIC);

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
