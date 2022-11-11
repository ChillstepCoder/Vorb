#include "stdafx.h"
#include "ChunkMesher.h"

#include "services/Services.h"

#include "rendering/mesh/ProceduralMeshBuilder.h"
#include "rendering/mesh/BillboardMeshBuilder.h"
#include "rendering/RenderContext.h"
#include "rendering/mesh/TileMeshBuilderMethods.h"
#include "rendering/RenderThreadTasks.h"

#include "world/Chunk.h"

#include "tile/TileContainer.h"

void ChunkMesher::buildMeshAndPhysicsAsync(const Chunk& chunk, PhysicsWorld& physWorld) {
    TileContainer* chunkTileContainer = chunk.mTileContainer;
    // Always incref, will be decrefed in the task
    chunkTileContainer->incReadLockAndRef();

    TileContainerRenderData& tileRenderData = chunkTileContainer->getRenderData();
    tileRenderData.mHasMesh = true;

    Services::Threadpool::ref().addTask([chunkTileContainer](ThreadPoolWorkerData*) {

        PROFILE_SCOPE("ChunkMesh build");

        ProceduralMeshBuilder staticMeshBuilder(true);
        ProceduralMeshBuilder dynamicMeshBuilder(false);
        BillboardMeshBuilder billboardMeshBuilder;

        StaticPhysicsMesh& physicsMesh = chunkTileContainer->getStaticPhysicsMesh();

        // TODO: Memory pool for this data?
        constexpr ui32 RESERVE_VERT_COUNT_STATIC = 512; // Most chunks are less than this
        staticMeshBuilder.reserveVertexCount(RESERVE_VERT_COUNT_STATIC);
        billboardMeshBuilder.reserveBillboardCount(CHUNK_SIZE / 2);

        // ========================== Mesh Tiles ===============================
        TileMeshBuilderMethods::meshTileContainerStatic(staticMeshBuilder, &billboardMeshBuilder, *chunkTileContainer, nullptr /*physicsMesh*/);
        TileMeshBuilderMethods::meshTileContainerDynamic(dynamicMeshBuilder, *chunkTileContainer);

        staticMeshBuilder.computeBoundingSphere();
        dynamicMeshBuilder.computeBoundingSphere();
        billboardMeshBuilder.computeBoundingSphere();

        RenderThreadTasks::getInstance().addTileContainerMeshUpdateTask(chunkTileContainer, std::move(staticMeshBuilder), std::move(dynamicMeshBuilder), std::move(billboardMeshBuilder));
    }, nullptr);
}
