#include "stdafx.h"
#include "TileContainerMesher.h"

#include "services/Services.h"

#include "rendering/mesh/ProceduralMeshBuilder.h"
#include "rendering/mesh/BillboardMeshBuilder.h"
#include "rendering/RenderContext.h"
#include "rendering/mesh/TileMeshBuilderMethods.h"
#include "rendering/RenderThreadTasks.h"
#include "rendering/model/InstancedStaticModelGatherer.h"

#include "gamethread/GameThreadTasks.h"
#include "physics/StaticPhysicsMeshBuilder.h"

#include "world/Chunk.h"

#include "tile/TileContainer.h"

void TileContainerMesher::initMeshAndPhysicsAsync(TileContainer& container, const f32* heightData) {
    assert(IS_GAME_THREAD()); // Game thread makes the request

    // Always incref, will be decrefed in the task
    assert(container.getState() == TileContainerState::LOADING);
    // We will incref twice, once for mesh and once for static physics
    container.incRef();
    container.incRef();
    container.setState(TileContainerState::WAITING_MESH_AND_PHYSICS);

    Services::Threadpool::ref().addTask([&container, heightData](ThreadPoolWorkerData*) {

        TileContainerID containerId = container.getId();
        PROFILE_SCOPE("ChunkMesh build");

        ProceduralMeshBuilder staticMeshBuilder(true);
        ProceduralMeshBuilder dynamicMeshBuilder(false);
        BillboardMeshBuilder billboardMeshBuilder;
        InstancedStaticModelGatherer modelGatherer(containerId, f32v3(container.getWorldPos3D()));
        StaticPhysicsMeshBuilder physicsBuilder(containerId);

        // TODO: Memory pool for this data?
        constexpr ui32 RESERVE_VERT_COUNT_STATIC = 512; // Most chunks are less than this
        staticMeshBuilder.reserveVertexCount(RESERVE_VERT_COUNT_STATIC);
        billboardMeshBuilder.reserveBillboardCount(CHUNK_SIZE / 2);

        // ========================== Mesh Tiles ===============================
        TileMeshBuilderMethods::meshTileContainerStatic(staticMeshBuilder, &billboardMeshBuilder, modelGatherer, container, &physicsBuilder, heightData);
        TileMeshBuilderMethods::meshTileContainerDynamic(dynamicMeshBuilder, container);

        staticMeshBuilder.computeBoundingSphere();
        dynamicMeshBuilder.computeBoundingSphere();
        billboardMeshBuilder.computeBoundingSphere();

        RenderThreadTasks::getInstance().addTileContainerMeshInitTask(&container, std::move(staticMeshBuilder), std::move(dynamicMeshBuilder), std::move(billboardMeshBuilder), std::move(modelGatherer));

        if (physicsBuilder.hasAnyCollision()) {
            GameThreadTasks::getInstance().addTileContainerStaticPhysicsMeshInitTask(containerId, std::move(physicsBuilder));
        }
        else {
            container.setDidInitPhysics();
            container.decRef();
        }
    }, nullptr);
}
