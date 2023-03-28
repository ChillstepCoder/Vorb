#include "stdafx.h"
#include "TileContainerRenderer.h"
#include "camera/Camera3D.h"
#include "world/Chunk.h"
#include "world/IWorld.h"
#include "resources/ResourceManager.h"
#include "rendering/mesh/Mesh.h"
#include "rendering/MaterialRenderer.h"
#include "rendering/MaterialShaderManager.h"
#include "rendering/RenderContext.h"
#include "rendering/ChunkGrassQuadtree.h"
#include "rendering/RenderThreadTasks.h"

#include "rendering/mesh/mesher/ChunkMesher.h"
#include "rendering/mesh/mesher/BuildingMesher.h"
#include "rendering/mesh/mesher/builder/ContainerMeshBuilders.h"

// TODO: Remove
#include "util/Utils.h"

#include "options/DebugOptions.h"

#include <Vorb/graphics/SpriteBatch.h>
#include <Vorb/graphics/SamplerState.h>
#include <Vorb/graphics/DepthState.h>

#define ENABLE_DEBUG_RENDER 1
#if ENABLE_DEBUG_RENDER == 1
#include <Vorb/ui/InputDispatcher.h>
#endif

#include <boost/pool/singleton_pool.hpp>

#include "tasks/MeshTask.inl"

constexpr float FLORA_RENDER_DISTANCE_2 = SQ(320.0f);
constexpr float FLORA_UNLOAD_DISTANCE_2 = SQ(340.0f);
static_assert(FLORA_UNLOAD_DISTANCE_2 > FLORA_RENDER_DISTANCE_2);

TileContainerRenderer::TileContainerRenderer(InstancedStaticModelRenderer& instancedStaticModelRenderer) : mInstancedStaticModelRenderer(instancedStaticModelRenderer)
{

    const MaterialShaderManager& materialManager = Services::ResourceManager::ref().getMaterialShaderManager();
    mStandardMaterial = materialManager.getMaterialShader("standard_tile");
    mBillboardMaterial = materialManager.getMaterialShader("billboard_ssbo");
    mShadowMapperMaterial = materialManager.getMaterialShader("shadow_mapper");
    mShadowMapperMaterialBillboard = materialManager.getMaterialShader("shadow_mapper");
}

TileContainerRenderer::~TileContainerRenderer() {
	
}

void TileContainerRenderer::updateMeshFromBuilders(const TileContainer* containerToMesh, ContainerMeshBuilders&& builders) {
    assert(!IS_RENDER_THREAD());

    MeshTaskData* taskData =
        new MeshTaskData(
            std::move(builders)
        );

    //assert(containerToMesh->getState() == TileContainerState::WAITING_MESH_AND_PHYSICS);
    // Pass result to the render thread
    RenderThreadTasks::getInstance().addGenericTask([](RenderContext& context, void* meshTaskData) {
        PROFILE_SCOPE("TileContainerRenderer::updateMeshFromBuilders");

        TileContainerRenderer& renderer = context.getTileContainerRenderer();
        MeshTaskData* taskData = static_cast<MeshTaskData*>(meshTaskData);
        const TileContainer& tileContainer = taskData->builders.container;
        const TileContainerID id = tileContainer.getId();

        TileContainerMeshData& meshData = renderer.mTileContainerMeshData[id];

        // Remove existing meshes
        if (meshData.mStaticMesh != nullptr) {
            renderer.removeStaticMesh(meshData.mStaticMesh.get());
        }
        if (meshData.mDynamicMesh != nullptr) {
            renderer.removeDynamicMesh(meshData.mDynamicMesh.get());
        }
        if (meshData.mBillboardMesh != nullptr) {
            renderer.removeBillboardMesh(meshData.mBillboardMesh.get());
        }

        // Upload mesh buffers
        taskData->builders.staticBuilder.finishMesh(meshData.mStaticMesh, tileContainer.getWorldPos3D());
        taskData->builders.dynamicBuilder.finishMesh(meshData.mDynamicMesh, tileContainer.getWorldPos3D());
        taskData->builders.billboardBuilder.finishMesh(meshData.mBillboardMesh, tileContainer.getWorldPos3D(), 0 /*bufferFlags*/);

        // Static
        if (meshData.mStaticMesh) {
            renderer.addStaticMesh(meshData.mStaticMesh.get());
        }

        // Dynamic
        if (meshData.mDynamicMesh) {
            renderer.addDynamicMesh(meshData.mDynamicMesh.get());
        }

        // Billboard
        if (meshData.mBillboardMesh) {
            renderer.addBillboardMesh(meshData.mBillboardMesh.get());
        }

        // Model instances
        context.addStaticModelInstancesFromGatherer(taskData->builders.modelGatherer);

        // Release
        tileContainer.setDidInitMesh();
        tileContainer.decRef();

        delete taskData;
    }, taskData);
}

void TileContainerRenderer::renderStaticMeshes(const Camera3D& camera) {
    // Tiles
    // TODO: Move this to MaterialRenderer::renderMeshes();
    MaterialRenderer::bindMaterialForRender(*mStandardMaterial);
    VGUniform unPosition = mStandardMaterial->getUniform("unPosition");
    for (auto&& mesh : mStaticMeshes) {
        if (camera.sphereIsVisible(mesh->getBoundingSphere())) {
            glUniform3fv(unPosition, 1, &mesh->getPosition().x);
            mesh->draw();
        }
    }
}

void TileContainerRenderer::renderBillboards(const Camera3D& camera) {

    // TODO: Move this to MaterialRenderer::renderMeshes();
    MaterialRenderer::bindMaterialForRender(*mBillboardMaterial);
    for (auto&& mesh : mBillboardMeshes) {
        if (camera.sphereIsVisible(mesh->getBoundingSphere())) {
            assert(mesh->isValid());
            mesh->draw();
        }
    };
}

void TileContainerRenderer::renderWorldShadows(const Camera3D& camera, f32 maxDistance) {
    const f32 maxDistSQ = SQ(maxDistance + CHUNK_WIDTH * 0.5f);

    MaterialRenderer::bindMaterialForRender(*mShadowMapperMaterial);
    VGUniform unPosition = mShadowMapperMaterial->getUniform("unPosition");
    for (auto&& mesh : mStaticMeshes) {
        f32v3 offset = mesh->getPosition() - camera.getPosition();
        if (glm::length2(offset) <= maxDistSQ) {
            glUniform3fv(unPosition, 1, &mesh->getPosition().x);
            mesh->draw();
        }
    }
}
