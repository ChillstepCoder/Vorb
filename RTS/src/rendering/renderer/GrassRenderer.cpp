#include "stdafx.h"
#include "GrassRenderer.h"

#include "rendering/GrassBillboardMesh.h"
#include "rendering/ChunkGrassQuadtree.h"
#include "resources/ResourceManager.h"
#include "resources/MaterialRepository.h"
#include "rendering/MaterialShaderManager.h"
#include "rendering/MaterialRenderer.h"
#include "rendering/RenderStats.h"

#include "options/DebugOptions.h"
#include "camera/Camera3D.h"

#include "resources/TileGrassRepository.h"

GrassRenderer::GrassRenderer() {
    const MaterialShaderManager& materialManager = Services::ResourceManager::ref().getMaterialShaderManager();
    mGrassMaterial = materialManager.getMaterialShader("grass");

    constexpr size_t RESERVE_COUNT = 128;
    for (int i = 0; i < e_count(TileGrassMeshType); ++i) {
        mVisibleMeshes[i].reserve(RESERVE_COUNT);
    }
}

void GrassRenderer::renderDefaultGrass(const Camera3D& camera, const f32v3& playerPos, const std::vector<GrassMeshRenderDataWithPos>& grassMeshes) {
    if (grassMeshes.empty()) {
        return;
    }
    
    MaterialRenderer::bindMaterialForRender(*mGrassMaterial);
    const vg::GLProgram& program = mGrassMaterial->mProgram;
    VGUniform positionUniform = program.getUniform("unPosition");
    VGUniform crossfadeAlphaUniform = program.getUniform("unCrossfadeAlpha");
    VGUniform crossfadeDirectionUniform = program.getUniform("unCrossfadeDirection");
    VGUniform tboSizeTypeUniform = program.getUniform("UnTboSizeType");
    VGUniform tboPositionUniform = program.getUniform("UnTboPosition");
    glUniform3fv(program.getUniform("unPlayerPos"), 1, &playerPos.x);
    glUniform1f(program.getUniform("unFadeDistance"), sDebugOptions.mGrassSettings.fadeDistance);
    glUniform2f(program.getUniform("unScale"), sDebugOptions.mGrassScale.x, sDebugOptions.mGrassScale.y);
    glUniform1f(program.getUniform("unLeanVariance"), sDebugOptions.mGrassLeanVariance);
    glUniform1f(program.getUniform("unDitherPower"), sDebugOptions.mGrassDitherPower);
    glUniform1f(program.getUniform("unColorMapScale"), sDebugOptions.mGrassColorMapScale);

    // TODO: cache this? UBO?
    const int MAX_GRASS = 32;
    const std::vector<TileGrassData>& grassData = Services::ResourceManager::ref().getTileGrassRepository().getAllGrassData();
    assert(grassData.size() < 32);

    int grassMaterials[MAX_GRASS];
    int cellCounts[MAX_GRASS];
    int useGradient[MAX_GRASS];
    float leanVariance[MAX_GRASS];
    f32v2 grassScale[MAX_GRASS];

    for (size_t i = 0; i < grassData.size(); ++i) {
        grassMaterials[i] = grassData[i].mMaterialID;
        cellCounts[i] = grassData[i].mNumTextures;
        useGradient[i] = (int)grassData[i].mUseGradientColor;
        leanVariance[i] = (float)grassData[i].mLeanVariance;
        grassScale[i] = grassData[i].mSizeMults;
    }

    // Upload grass materials
    glUniform1iv(program.getUniform("unGrassMaterials[0]"), grassData.size(), grassMaterials);

    // Upload material cell counts
    glUniform1iv(program.getUniform("unGrassMaterialCellCounts[0]"), grassData.size(), cellCounts);

    // Upload material cell counts
    glUniform1iv(program.getUniform("unShouldUseColorGradient[0]"), grassData.size(), useGradient);

    // Upload material cell counts
    glUniform1fv(program.getUniform("unGrassLeanVariance[0]"), grassData.size(), leanVariance);

    // Upload material cell counts
    glUniform2fv(program.getUniform("unGrassScale[0]"), grassData.size(), &grassScale[0].x);

    glUniform1i(tboSizeTypeUniform, GRASS_TBO_INSTANCE_DATA_BINDING);
    glUniform1i(tboPositionUniform, GRASS_TBO_POSITION_DATA_BINDING);

    glPatchParameteri(GL_PATCH_VERTICES, 3);

    for (auto&& grassMesh : grassMeshes) {
        const GrassBillboardMeshRenderData& renderData = grassMesh.renderData;
        glUniform3fv(positionUniform, 1, &grassMesh.pos.x);
        //TODO: Move out and cache
        //int crossfadeDir = grassMesh->mCrossfadeDir.load();
        //if (crossfadeDir != 0) {
        //    glUniform1f(crossfadeAlphaUniform, grassMesh->mCrossfadeAlpha.load() * 0.5f /* Constant that was selected via trial and error*/);
        //    glUniform1f(crossfadeDirectionUniform, (crossfadeDir > 0) ? 1.0f : 0.0f);
        //}
        //else {
        //    glUniform1f(crossfadeAlphaUniform, 0.0f);
        //    glUniform1f(crossfadeDirectionUniform, 0.0f);
        //}

        // Make sure we have been initialized
        assert(renderData.mVao);
        if (!renderData.mIndexCount) return;

        glBindVertexArray(renderData.mVao);

        // Bind textures
        glBindTextureUnit(GRASS_TBO_INSTANCE_DATA_BINDING, renderData.mTboInstanceData);
        glBindTextureUnit(GRASS_TBO_POSITION_DATA_BINDING, renderData.mTboPositionData);

        glDrawElements(GL_PATCHES, renderData.mIndexCount, GL_UNSIGNED_INT, (const GLvoid*)(0) /* offset */);
        RenderStats::recordDrawCall(renderData.mIndexCount / 3);
    };
}

void GrassRenderer::renderPlaneGrass(const Camera3D& camera, const f32v3& playerPos, const std::vector<GrassMeshRenderDataWithPos>& grassMeshes) {
    if (grassMeshes.empty()) {
        return;
    }
    assert(false);
}

void GrassRenderer::renderGrass(const Camera3D& camera, const f32v3& playerPos, const boost::container::flat_set<const GrassMesh*>& grassMeshes, TileGrassMeshType meshType) {
    
    for (int i = 0; i < e_count(TileGrassMeshType); ++i) {
        mVisibleMeshes[i].clear();
    }

    // CPU cull and gather
    for (auto&& grassMesh : grassMeshes) {
        const GrassBillboardMesh& mesh = grassMesh->mMesh;
        if (camera.sphereIsVisible(mesh.getBoundingSphere())) {
            for (int i = 0; i < e_count(TileGrassMeshType); ++i) {
                if (mesh.isValid((TileGrassMeshType)i)) {
                    mVisibleMeshes[i].emplace_back(GrassMeshRenderDataWithPos{ mesh.getRenderData((TileGrassMeshType)i), grassMesh->mPosition});
                }
            }
        }
    }

    // Render
    renderDefaultGrass(camera, playerPos, mVisibleMeshes[e_cast(TileGrassMeshType::DEFAULT)]);
    renderPlaneGrass(camera, playerPos, mVisibleMeshes[e_cast(TileGrassMeshType::PLANE)]);

    checkGlError("GrassRenderer::renderGrass");


}
