#include "stdafx.h"
#include "ChunkGrassQuadtree.h"

#include "rendering/RenderContext.h"
#include "world/Chunk.h"
#include "world/IHeightmapGrid.h"
#include "camera/Camera3D.h"

#include "options/DebugOptions.h"

#include "generation/WorldGeneration.h"
#include <Vorb/graphics/GLProgram.h>

#include "math/Random.h"
#include "debugging/DebugRenderer.h"

#include "rendering/RenderThreadTasks.h"
#include "gamethread/GameThreadTasks.h"

#include <boost/pool/singleton_pool.hpp>

constexpr int GRASS_LOD_DETAIL[GRASS_QUADTREE_MAX_LOD] = {
    0,
    1,
    3,
    6,
    12,
};

constexpr f32 GRASS_BLADE_WIDTHS[GRASS_QUADTREE_MAX_LOD] = {
    0.0f,
    1.00f,
    0.5f,
    0.12f,
    0.05f,
};

constexpr f32 GRASS_SUBDIVIDE_DISTANCES_SQ[GRASS_QUADTREE_MAX_LOD] = { // sqrt(pow(WIDTH, 2) * 2) for diagonal distance widths
    FLT_MAX,
    SQ(91.0f),
    SQ(46.0f),
    SQ(23.0f),
    -FLT_MAX // Never subdivide last
};

struct GrassMeshFreeTask {
    GrassMeshFreeTask(std::unique_ptr<GrassMesh>&& grassMesh) : grassMesh(std::move(grassMesh)) {}

    std::unique_ptr<GrassMesh> grassMesh;
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



ChunkGrassQuadtree::ChunkGrassQuadtree(const Chunk& chunk) : mChunk(chunk), FlatQuadtree(chunk.getWorldPos(), GRASS_SUBDIVIDE_DISTANCES_SQ, sDebugOptions.mGrassSettings.lodDistanceOffset) {
    mWorldPos = mChunk.getWorldPos();
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

void createGrassMesh(
    GrassBillboardMesh& grassMesh,
    const Chunk& chunk,
    const ui32v2& tilePosStart,
    ui32 lod,
    const HeightmapPatchData* heightData
) {
    PROFILE_FUNCTION();
    const ui32v2& dims = (ui32v2&)ChunkGrassFlatQuadtree::LOD_DIMS[lod];
    const ui32 density = GRASS_LOD_DETAIL[lod];
    const f32 bladeWidth = GRASS_BLADE_WIDTHS[lod];
    grassMesh.reserveQuadCount((size_t)dims.x * dims.y * density * density);

    // Bounding sphere
    // TODO: This isn't accurate for slopey surfaces! We need a proper AABB
    BoundingSphere boundingSphere;
    // A little algebra ;P
    const f32 halfDims = dims.x * 0.5f;
    boundingSphere.radius = sqrt(2.0f * halfDims * halfDims);
    const f32v2 chunkWorldPos = chunk.getWorldPos();
    boundingSphere.center.x = chunkWorldPos.x + tilePosStart.x + halfDims;
    boundingSphere.center.y = chunkWorldPos.y + tilePosStart.y + halfDims;
    
    { // Read lock
        std::shared_lock lock(heightData->mMutex);
        
        // Sample bounding sphere from heightmap
        boundingSphere.center.z = sHeightmapGrid->computeHeightAtChunkOffset(heightData->data, chunk.getChunkID(), f32v2(tilePosStart.x + halfDims, tilePosStart.y + halfDims));

        // TODO: Optimize redundant math
        for (ui32 y = 0; y < dims.y; ++y) {
            for (ui32 x = 0; x < dims.x; ++x) {
                assert(tilePosStart.x + x < CHUNK_WIDTH&& tilePosStart.y + y < CHUNK_WIDTH);
                const ui32 tx = tilePosStart.x + x;
                const ui32 ty = tilePosStart.y + y;
                TileIndex tileIndex = chunk.getTileContainer()->getTileIndexFromXYZOffset(tx, ty, 0u);

                ui8 grassVal = chunk.getGrassAt(tileIndex);
                if (grassVal == 0) {
                    continue;
                }

                const f32v2 tileWorldPos = f32v2(tx, ty);

                /*Tile neighbors[8];
                chunk.getTileNeighbors(tileIndex, neighbors);

                const int zPosition = tile.groundZPosition + ((spriteData.flags & SPRITEDATA_FLAG_OPAQUE) ? 1 : 0);
                const int bottomHeightDiff = zPosition - getTileHeight(neighbors[(int)NeighborIndex::BOTTOM], layerIndex);
                const int topHeightDiff = zPosition - getTileHeight(neighbors[(int)NeighborIndex::TOP], layerIndex);*/

                // Allow overlap when adjacent tiles are the same
                //const float rightXMult = (rightTile.groundZPosition != tile.groundZPosition || tileId != rightTile.layers[layerIndex]) ? 1.0f : 0.0f;
                //const float topXMult = (topTile.groundZPosition != tile.groundZPosition || tileId != topTile.layers[layerIndex]) ? 1.0f : 0.0f;

                // Handle variant UVs
                constexpr int NUM_GRASS_TYPES = 4;

                // TODO: Determine edge

                // Generate blades
                for (int y2 = 0; y2 < (int)density; ++y2) {
                    for (int x2 = 0; x2 < (int)density; ++x2) {
                        const f32 rnd = Random::getCachedRandomfSpecific(x2 + CHUNK_SIZE * y2 - tx - ty * CHUNK_SIZE) * 0.9f;
                        const float xo = (x2 + rnd) / (float)density;
                        const float yo = (y2 - rnd) / (float)density;
                        float rsize = lerp(0.2f, 0.4f, rnd);
                        const f32 grassNoise = -sWorldGen.mGrassNoise.compute((f64)tileWorldPos.x + xo + chunk.getWorldPos().x, (f64)tileWorldPos.y + yo + chunk.getWorldPos().y);
                        rsize += -grassNoise * 0.4f;
                        const ui8 variantIndex = (ui8)((grassNoise + 1.0f) * SQ(NUM_GRASS_TYPES)) % NUM_GRASS_TYPES;
                        f32v2 truePos(tileWorldPos.x + xo, tileWorldPos.y + yo);
                        const f32 zPos = sHeightmapGrid->computeHeightAtChunkOffset(heightData->data, chunk.getChunkID(), truePos);
                        grassMesh.addBladeQuad(
                            f32v3(truePos.x, truePos.y, zPos), // TODO: new height
                            f32v2(bladeWidth, rsize),
                            variantIndex
                        );
                    }
                }
            }
        }
    } // Read lock end
    grassMesh.setBoundingSphere(boundingSphere);
};

void ChunkGrassQuadtree::buildMeshForPatch(QuadtreePatch& patch, ui32 lod, ui32 patchIndex) {
    assert(IS_GAME_THREAD());
    bool hasAquired = true;
    if (!mMeshes[patchIndex]) {
        hasAquired = false;
        mMeshes[patchIndex] = std::make_unique<GrassMesh>(patchIndex);
    }
    ++mRefCount;
    mChunk.incRef();

    assert(!patch.isCrossfading() && !patch.isMeshDirty() && patch.isActive());

    const HeightmapPatchID id = getHeightmapPatchID(patchIndex);
    if (const HeightmapPatchData* heightData = sHeightmapGrid->tryGetHeightDataAt(id)) {
        if (!hasAquired) {
            sHeightmapGrid->aquireHeightData(id);
        }
        // Instantly generate
        Services::Threadpool::ref().addTask([this, &patch, lod, patchIndex, heightData](ThreadPoolWorkerData*) {

            //PreciseTimer timer;
            createGrassMesh(mMeshes[patchIndex]->mMesh, mChunk, PATCH_POSITIONS.data[patchIndex].xy, lod, heightData);
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
        sHeightmapGrid->releaseHeightDataAt(id);

        assert(IS_GAME_THREAD());
        GrassMeshFreeTask* freeTask = new GrassMeshFreeTask(std::move(mMeshes[patchIndex]));
        RenderThreadTasks::getInstance().addGenericTask([](RenderContext& context, void* vTaskData) {
            GrassMeshFreeTask* taskData = static_cast<GrassMeshFreeTask*>(vTaskData);
            RenderContext::getInstance().removeGrassMesh(taskData->grassMesh.get());
            delete taskData;
        }, freeTask);
    }
}

void ChunkGrassQuadtree::finishMesh(ui32 patchIndex) {
    std::unique_ptr<GrassMesh>& mesh = mMeshes[patchIndex];
    mesh->mPosition = getWorldPos3D();
    mesh->mMesh.finishMesh(MeshDrawMode::STATIC);

    if (mesh->mMesh.isValid()) {
        if (!mesh->mHadMesh) {
            assert(mesh->mIndex < ChunkGrassFlatQuadtree::NODE_COUNT);
            RenderContext::getInstance().addGrassMesh(mesh.get());
            mesh->mHadMesh = true;
        }
    }
    else if (mesh->mHadMesh) {
        RenderContext::getInstance().removeGrassMesh(mesh.get());
        mesh.reset();
    }
}
