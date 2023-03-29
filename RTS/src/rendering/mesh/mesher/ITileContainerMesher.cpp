#include "stdafx.h"
#include "ITileContainerMesher.h"

#include "services/Services.h"

#include "rendering/mesh/mesher/builder/ContainerMeshBuilders.h"
#include "rendering/mesh/mesher/builder/ProceduralMeshBuilder.h"
#include "rendering/mesh/mesher/builder/BillboardMeshBuilder.h"
#include "rendering/RenderContext.h"
#include "rendering/mesh/mesher/builder/TileMeshBuilderMethods.h"
#include "rendering/RenderThreadTasks.h"
#include "rendering/model/InstancedStaticModelGatherer.h"
#include "rendering/TileContainerRenderer.h"

#include "gamethread/GameThreadTasks.h"
#include "physics/StaticPhysicsMeshBuilder.h"

#include "tile/TileContainer.h"


void ITileContainerMesher::initMeshAndPhysicsAsyncInternal(const TileContainer& container, const f32* heightData, bool staticMeshIsOnlyQuads, ui32 reserveStaticVertexCount, const void* userData) const {
    assert(IS_GAME_THREAD()); // Game thread makes the request

    // Always incref, will be decrefed in the task
    // TODO: Non terrain is handled differently???
    //assert(!container.isTerrain() || container.getState() == TileContainerState::WAITING_MESH_AND_PHYSICS);
    // We will incref twice, once for mesh and once for static physics
    container.incRef();
    container.incRef();

    Services::Threadpool::ref().addTask([&container, heightData, staticMeshIsOnlyQuads, reserveStaticVertexCount, this, userData](ThreadPoolWorkerData*) {

        TileContainerID containerId = container.getId();
        PROFILE_SCOPE("ChunkMesh build");

        ContainerMeshBuilders builders(container, staticMeshIsOnlyQuads);
        StaticPhysicsMeshBuilder physicsBuilder(containerId);

        // TODO: Memory pool for this data?
        builders.staticBuilder.reserveVertexCount(reserveStaticVertexCount);
        if (!staticMeshIsOnlyQuads) {
            builders.staticBuilder.reserveIndexCount((ui32)(reserveStaticVertexCount * 1.5f)); // Approx
        }

        TileMeshBuilderMethods::meshTileContainer(builders, physicsBuilder, heightData);

        // Custom per-builder stuff, such as building roofs
        addCustomMeshData(builders, physicsBuilder, userData);

        builders.computeBoundingSpheres();

        mRenderer.updateMeshFromBuilders(&container, std::move(builders));

        if (physicsBuilder.hasAnyCollision()) {
            GameThreadTasks::getInstance().addTileContainerStaticPhysicsMeshInitTask(containerId, std::move(physicsBuilder));
        }
        else {
            container.setDidInitPhysics();
            container.decRef();
        }

        // Allocated with new
        if (heightData) {
            delete[] heightData;
        }
    }, nullptr);
}
