#include "stdafx.h"
#include "TileContainerRenderer.h"
#include "camera/Camera3D.h"
#include "world/Chunk.h"
#include "world/IWorld.h"
#include "world/cli/CliWorldInterface.h"
#include "resources/ResourceManager.h"
#include "rendering/mesh/Mesh.h"
#include "rendering/MaterialRenderer.h"
#include "rendering/MaterialShaderManager.h"
#include "rendering/RenderContext.h"
#include "rendering/ChunkGrassQuadtree.h"

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

constexpr float FLORA_RENDER_DISTANCE_2 = SQ(320.0f);
constexpr float FLORA_UNLOAD_DISTANCE_2 = SQ(340.0f);
static_assert(FLORA_UNLOAD_DISTANCE_2 > FLORA_RENDER_DISTANCE_2);

TileContainerRenderer::TileContainerRenderer()
{
    mCliWorld = dynamic_cast<CliWorldInterface*>(sWorld);
    assert(mCliWorld);

    const MaterialShaderManager& materialManager = Services::ResourceManager::ref().getMaterialManager();
    mStandardMaterial = materialManager.getMaterialShader("standard_tile");
    mBillboardMaterial = materialManager.getMaterialShader("billboard_ssbo");
    mShadowMapperMaterial = materialManager.getMaterialShader("shadow_mapper");
    mShadowMapperMaterialBillboard = materialManager.getMaterialShader("shadow_mapper");
}

TileContainerRenderer::~TileContainerRenderer() {
	
}

void TileContainerRenderer::renderStaticMeshes(const std::set<const Mesh*>& meshes, const Camera3D& camera) {
    // Tiles
    // TODO: Move this to MaterialRenderer::renderMeshes();
    MaterialRenderer::bindMaterialForRender(*mStandardMaterial);
    for (auto&& mesh : meshes) {
        if (camera.sphereIsVisible(mesh->getBoundingSphere())) {
            mesh->draw();
        }
    }
}

void TileContainerRenderer::renderBillboards(const std::set<const Mesh*>& meshes, const Camera3D& camera) {

    // TODO: Move this to MaterialRenderer::renderMeshes();
    MaterialRenderer::bindMaterialForRender(*mBillboardMaterial);
    for (auto&& mesh : meshes) {
        if (camera.sphereIsVisible(mesh->getBoundingSphere())) {
            assert(mesh->isValid());
            mesh->draw();
        }
    };
}

void TileContainerRenderer::renderWorldShadows(const std::set<const Mesh*>& meshes, const Camera3D& camera, f32 maxDistance) {
    const f32 maxDistSQ = SQ(maxDistance + CHUNK_WIDTH * 0.5f);

    MaterialRenderer::bindMaterialForRender(*mShadowMapperMaterial);
    for (auto&& mesh : meshes) {
        f32v3 offset = mesh->getPosition() - camera.getPosition();
        if (glm::length2(offset) <= maxDistSQ) {
            mesh->draw();
        }
    }
}
