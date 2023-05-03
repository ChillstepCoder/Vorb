#include "stdafx.h"
#include "TileContainerRenderer.h"
#include "camera/Camera3D.h"
#include "resources/ResourceManager.h"
#include "rendering/mesh/Mesh.h"
#include "rendering/MaterialRenderer.h"
#include "rendering/MaterialShaderManager.h"
#include "rendering/RenderContext.h"

#include "rendering/post_process/ShadowPassShaderData.h"

#include "options/DebugOptions.h"

constexpr float FLORA_RENDER_DISTANCE_2 = SQ(320.0f);
constexpr float FLORA_UNLOAD_DISTANCE_2 = SQ(340.0f);
static_assert(FLORA_UNLOAD_DISTANCE_2 > FLORA_RENDER_DISTANCE_2);

TileContainerRenderer::TileContainerRenderer() {

    const MaterialShaderManager& materialManager = Services::ResourceManager::ref().getMaterialShaderManager();
    mStandardMaterial = materialManager.getMaterialShader("standard_tile");
    mBillboardMaterial = materialManager.getMaterialShader("billboard_ssbo");
    mShadowMapperMaterial = materialManager.getMaterialShader("shadow_mapper");
    mShadowMapperMaterialBillboard = materialManager.getMaterialShader("shadow_mapper");
}

TileContainerRenderer::~TileContainerRenderer() {
	
}

void TileContainerRenderer::renderStaticMeshes(const boost::container::flat_set<const Mesh*>& meshes, const Camera3D& camera) {
    // Tiles
    // TODO: Move this to MaterialRenderer::renderMeshes();
    MaterialRenderer::bindMaterialForRender(*mStandardMaterial);
    VGUniform unPosition = mStandardMaterial->getUniform("unPosition");
    for (auto&& mesh : meshes) {
        if (camera.sphereIsVisible(mesh->getBoundingSphere())) {
            glUniform3fv(unPosition, 1, &mesh->getPosition().x);
            mesh->draw();
        }
    }
}

void TileContainerRenderer::renderBillboards(const boost::container::flat_set<const Mesh*>& meshes, const Camera3D& camera) {

    // TODO: Move this to MaterialRenderer::renderMeshes();
    MaterialRenderer::bindMaterialForRender(*mBillboardMaterial);
    for (auto&& mesh : meshes) {
        if (camera.sphereIsVisible(mesh->getBoundingSphere())) {
            assert(mesh->isValid());
            mesh->draw();
        }
    };
}

void TileContainerRenderer::renderWorldShadows(const boost::container::flat_set<const Mesh*>& meshes, const ShadowPassShaderData& shaderData, const Camera3D& camera, f32 maxDistance) {
    const f32 maxDistSQ = SQ(maxDistance + CHUNK_WIDTH * 0.5f);

    MaterialRenderer::bindMaterialForRender(*mShadowMapperMaterial);
    VGUniform unPosition = mShadowMapperMaterial->getUniform("unPosition");
    glUniformMatrix4fv(mShadowMapperMaterial->getUniform("unShadowFrustumMatrices[0]"), MAX_SHADOW_CASCADE_LEVELS, false, &(*shaderData.shadowFrustumMatrices)[0][0]);
    for (auto&& mesh : meshes) {
        f32v3 offset = mesh->getPosition() - camera.getPosition();
        if (glm::length2(offset) <= maxDistSQ) {
            glUniform3fv(unPosition, 1, &mesh->getPosition().x);
            mesh->draw();
        }
    }
}

