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

VGBuffer GrassRenderer::sGrassUniformBuffer = 0;

GrassRenderer::GrassRenderer() {
    const MaterialShaderManager& materialManager = Services::ResourceManager::ref().getMaterialShaderManager();
    mMaterials[e_cast(TileGrassMeshType::DEFAULT)] = materialManager.getMaterialShader("grass");
    mMaterials[e_cast(TileGrassMeshType::PLANE)] = materialManager.getMaterialShader("grass_plane");
    static_assert(e_count(TileGrassMeshType) == 2);

    constexpr size_t RESERVE_COUNT = 128;
    for (int i = 0; i < e_count(TileGrassMeshType); ++i) {
        mVisibleMeshes[i].reserve(RESERVE_COUNT);
    }

    if (sGrassUniformBuffer == 0) {
        updateUniformBuffer();
    }
}

GrassRenderer::~GrassRenderer() {

}

void GrassRenderer::renderDefaultGrass(const Camera3D& camera, const f32v3& playerPos, const std::vector<GrassMeshRenderDataWithPos>& grassMeshes) {
    if (grassMeshes.empty()) {
        return;
    }
    
    const MaterialShader* grassMaterial = mMaterials[e_cast(TileGrassMeshType::DEFAULT)];
    MaterialRenderer::bindMaterialForRender(*grassMaterial);
    const vg::GLProgram& program = grassMaterial->mProgram;
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

    const MaterialShader* grassMaterial = mMaterials[e_cast(TileGrassMeshType::PLANE)];
    MaterialRenderer::bindMaterialForRender(*grassMaterial);
    const vg::GLProgram& program = grassMaterial->mProgram;
    VGUniform positionUniform = program.getUniform("unPosition");
    VGUniform crossfadeAlphaUniform = program.getUniform("unCrossfadeAlpha");
    VGUniform crossfadeDirectionUniform = program.getUniform("unCrossfadeDirection");
    VGUniform tboSizeTypeUniform = program.getUniform("UnTboSizeType");
    VGUniform tboPositionUniform = program.getUniform("UnTboPosition");
    //glUniform3fv(program.getUniform("unPlayerPos"), 1, &playerPos.x); // No player collision yet
    glUniform1f(program.getUniform("unFadeDistance"), sDebugOptions.mGrassSettings.fadeDistance);
    glUniform2f(program.getUniform("unScale"), sDebugOptions.mGrassScale.x, sDebugOptions.mGrassScale.y);
    glUniform1f(program.getUniform("unDitherPower"), sDebugOptions.mGrassDitherPower);
    glUniform1f(program.getUniform("unColorMapScale"), sDebugOptions.mGrassColorMapScale);

    glUniform1i(tboSizeTypeUniform, GRASS_TBO_INSTANCE_DATA_BINDING);
    glUniform1i(tboPositionUniform, GRASS_TBO_POSITION_DATA_BINDING);

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

        glDrawElements(GL_TRIANGLES, renderData.mIndexCount, GL_UNSIGNED_INT, (const GLvoid*)(0) /* offset */);
        RenderStats::recordDrawCall(renderData.mIndexCount / 3);
    };
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
    
    // Grass data
    glBindBufferBase(GL_UNIFORM_BUFFER, BUFFER_BASE_GRASS_UBO, sGrassUniformBuffer);

    // Render
    renderDefaultGrass(camera, playerPos, mVisibleMeshes[e_cast(TileGrassMeshType::DEFAULT)]);
    renderPlaneGrass(camera, playerPos, mVisibleMeshes[e_cast(TileGrassMeshType::PLANE)]);

    checkGlError("GrassRenderer::renderGrass");

}

void GrassRenderer::updateUniformBuffer() {

    // Match shader
    struct GrassUniformData {
        f32v2 grassScale;
        int material;
        int materialCellCount;
        int shouldUseColorGradient;
        float leanVariance;
        float padding[2]; // Padding to match the GLSL std140 layout
    };

    const int MAX_GRASS = 32;
    const std::vector<TileGrassData>& grassData = Services::ResourceManager::ref().getTileGrassRepository().getAllGrassData();
    assert(grassData.size() < 32);

    GrassUniformData uboData[MAX_GRASS];

    for (size_t i = 0; i < grassData.size(); ++i) {
        GrassUniformData& data = uboData[i];
        data.grassScale = grassData[i].mSizeMults;
        data.material = grassData[i].mMaterialID;
        data.materialCellCount = grassData[i].mNumTextures;
        data.shouldUseColorGradient = (int)grassData[i].mUseGradientColor;
        data.leanVariance = (float)grassData[i].mLeanVariance;
    }

    if (sGrassUniformBuffer) {
        glDeleteBuffers(1, &sGrassUniformBuffer);
    }
    glCreateBuffers(1, &sGrassUniformBuffer);
    glNamedBufferStorage(sGrassUniformBuffer, grassData.size() * sizeof(GrassUniformData), uboData, 0);
}
