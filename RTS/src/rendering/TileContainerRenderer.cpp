#include "stdafx.h"
#include "TileContainerRenderer.h"
#include "camera/Camera3D.h"
#include "rendering/mesh/Mesh.h"
#include "rendering/mesh/MeshDrawer.h"
#include "rendering/MaterialRenderer.h"
#include "rendering/MaterialShaderRepository.h"
#include "rendering/RenderContext.h"

#include "rendering/post_process/ShadowPassShaderData.h"

#include "options/DebugOptions.h"

constexpr float FLORA_RENDER_DISTANCE_2 = SQ(320.0f);
constexpr float FLORA_UNLOAD_DISTANCE_2 = SQ(340.0f);
static_assert(FLORA_UNLOAD_DISTANCE_2 > FLORA_RENDER_DISTANCE_2);

TileContainerRenderer::TileContainerRenderer() {

    mStandardMaterial = AssetUtil::addAssetToBundleAndGetUnloaded<MaterialShaderDef>(mShaderAssets, StrToken("standard_tile", 0));
    mBillboardMaterial = AssetUtil::addAssetToBundleAndGetUnloaded<MaterialShaderDef>(mShaderAssets, StrToken("billboard_ssbo", 0));
    mShadowMapperMaterial = AssetUtil::addAssetToBundleAndGetUnloaded<MaterialShaderDef>(mShaderAssets, StrToken("shadow_mapper", 0));
    mShadowMapperMaterialBillboard = mShadowMapperMaterial;
}

TileContainerRenderer::~TileContainerRenderer() {
	
}

void TileContainerRenderer::renderStaticMeshes(const boost::container::flat_set<const Mesh*>& meshes, const Camera3D& camera) {
    if (!mShaderAssets.areAllAssetsLoaded()) {
        return;
    }
    // Tiles
    // TODO: Move this to MaterialRenderer::renderMeshes();
    MaterialRenderer::bindMaterialShaderForRender(*mStandardMaterial);
    VGUniform unPosition = mStandardMaterial->getUniform("unPosition");
    for (auto&& mesh : meshes) {
        if (camera.sphereIsVisible(mesh->getBoundingSphere())) {
            glUniform3fv(unPosition, 1, &mesh->getPosition().x);
            MeshDrawer::draw(mesh->mMainMesh);
        }
    }
}

void TileContainerRenderer::renderBillboards(const boost::container::flat_set<const Mesh*>& meshes, const Camera3D& camera) {
    if (!mShaderAssets.areAllAssetsLoaded()) {
        return;
    }
    // TODO: Move this to MaterialRenderer::renderMeshes();
    MaterialRenderer::bindMaterialShaderForRender(*mBillboardMaterial);
    for (auto&& mesh : meshes) {
        if (camera.sphereIsVisible(mesh->getBoundingSphere())) {
            assert(mesh->isValid());
            MeshDrawer::draw(mesh->mMainMesh);
        }
    };
}

void TileContainerRenderer::renderWorldShadows(const boost::container::flat_set<const Mesh*>& meshes, const ShadowPassShaderData& shaderData, const Camera3D& camera, f32 maxDistance) {
    const f32 maxDistSQ = SQ(maxDistance + CHUNK_WIDTH * 0.5f);
    if (!mShaderAssets.areAllAssetsLoaded()) {
        return;
    }
    MaterialRenderer::bindMaterialShaderForRender(*mShadowMapperMaterial);
    VGUniform unPosition = mShadowMapperMaterial->getUniform("unPosition");
    glUniformMatrix4fv(mShadowMapperMaterial->getUniform("unShadowFrustumMatrices[0]"), MAX_SHADOW_CASCADE_LEVELS, false, &(*shaderData.shadowFrustumMatrices)[0][0]);
    for (auto&& mesh : meshes) {
        f32v3 offset = mesh->getPosition() - camera.getPosition();
        if (glm::length2(offset) <= maxDistSQ) {
            glUniform3fv(unPosition, 1, &mesh->getPosition().x);
            MeshDrawer::draw(mesh->mMainMesh);
        }
    }
}

