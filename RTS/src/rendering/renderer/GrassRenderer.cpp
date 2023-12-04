#include "stdafx.h"
#include "GrassRenderer.h"

#include "rendering/GrassBillboardMesh.h"
#include "rendering/ChunkGrassQuadtree.h"
#include "resources/MaterialRepository.h"
#include "rendering/MaterialShaderRepository.h"
#include "rendering/MaterialRenderer.h"
#include "rendering/RenderStats.h"

#include "world/World.h"
#include "world/biome/BiomeGrid.h"
#include "resources/BiomeRepository.h"

#include "options/DebugOptions.h"
#include "camera/Camera3D.h"

#include "resources/TileGrassRepository.h"

VGBuffer GrassRenderer::sGrassUniformBuffer = 0;

GrassRenderer::GrassRenderer() {

    mMaterials[e_cast(TileGrassMeshType::DEFAULT)] = AssetUtil::addAssetToBundleAndGetUnloaded<MaterialShaderDef>(mShaderAssets, CStrToken("grass"));
    mMaterials[e_cast(TileGrassMeshType::PLANE)] = AssetUtil::addAssetToBundleAndGetUnloaded<MaterialShaderDef>(mShaderAssets, CStrToken("grass_plane"));
    mMaterials[e_cast(TileGrassMeshType::BILLBOARD)] = AssetUtil::addAssetToBundleAndGetUnloaded<MaterialShaderDef>(mShaderAssets, CStrToken("grass_billboard"));

    static_assert(e_count(TileGrassMeshType) == 3);

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

void GrassRenderer::onWorldBegin(World& world) {
    mBiomeTexture = world.getBiomeGrid().getBiomeTexture();
    mInverseWorldWidth = (f32)(1.0 / (f64)world.getWidthTiles());
}

void GrassRenderer::renderDefaultGrass(const Camera3D& camera, const f32v3& playerPos, const std::vector<GrassMeshRenderDataWithPos>& grassMeshes) {
    if (grassMeshes.empty()) {
        return;
    }
    if (!mShaderAssets.areAllAssetsLoaded()) {
        return;
    }
    
    const MaterialShaderDef* grassMaterial = mMaterials[e_cast(TileGrassMeshType::DEFAULT)];
    ui32 nextTextureUnit = 0;
    MaterialRenderer::bindMaterialShaderForRender(*grassMaterial, &nextTextureUnit);
    const vg::GLProgram& program = grassMaterial->mProgram;
    VGUniform positionUniform = program.getUniform("unPosition");
    VGUniform crossfadeAlphaUniform = program.getUniform("unCrossfadeAlpha");
    VGUniform crossfadeDirectionUniform = program.getUniform("unCrossfadeDirection");
    VGUniform tboSizeTypeUniform = program.getUniform("UnTboSizeType");
    VGUniform tboPositionUniform = program.getUniform("UnTboPosition");
    VGUniform tboNormalUniform = program.getUniform("UnTboNormal");
    VGUniform uvRootUniform = program.getUniform("unUVRoot");
    glUniform3fv(program.getUniform("unPlayerPos"), 1, &playerPos.x);
    glUniform1f(program.getUniform("unFadeDistance"), sDebugOptions.mGrassSettings.fadeDistance);
    glUniform2f(program.getUniform("unScale"), sDebugOptions.mGrassScale.x, sDebugOptions.mGrassScale.y);
    glUniform1f(program.getUniform("unLeanVariance"), sDebugOptions.mGrassLeanVariance);
    glUniform1f(program.getUniform("unDitherPower"), sDebugOptions.mGrassDitherPower);
    glUniform1f(program.getUniform("unColorMapScale"), sDebugOptions.mGrassColorMapScale);

    glUniform1f(program.getUniform("unInverseWorldWidth"), mInverseWorldWidth);

    glUniform1i(program.getUniform("unBiomeTexture"), nextTextureUnit);
    glBindTextureUnit(nextTextureUnit, mBiomeTexture);

    ++nextTextureUnit;
    glUniform1i(program.getUniform("unBiomeColorMapsTexture"), nextTextureUnit);
    glBindTextureUnit(nextTextureUnit, BiomeRepository::get().getBiomeColorMapsArrayTexture());

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_BASE_TERRAIN_COLOR_MAPS_SSBO, BiomeRepository::get().getBiomeColorMapsShaderLookupBuffer());

    glUniform1i(tboSizeTypeUniform, GRASS_TBO_INSTANCE_DATA_BINDING);
    glUniform1i(tboPositionUniform, GRASS_TBO_POSITION_DATA_BINDING);
    glUniform1i(tboNormalUniform, GRASS_TBO_NORMAL_DATA_BINDING);

    glPatchParameteri(GL_PATCH_VERTICES, 3);

    for (auto&& grassMesh : grassMeshes) {
        const GrassBillboardMeshRenderData& renderData = grassMesh.renderData;

        { // Compute with high precision to avoid precision issues in shader
            f64v2 rootUVDouble = f64v2(grassMesh.pos.x, grassMesh.pos.y) * f64(sDebugOptions.mGrassColorMapScale);
            f64 intpart;
            f32v2 rootUv;
            rootUv.x = (f32)modf(rootUVDouble.x, &intpart);
            rootUv.y = (f32)modf(rootUVDouble.y, &intpart);
            glUniform2fv(uvRootUniform, 1, &rootUv.x);
        }
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
        glBindTextureUnit(GRASS_TBO_NORMAL_DATA_BINDING, renderData.mTboNormalData);

        glDrawElements(GL_PATCHES, renderData.mIndexCount, GL_UNSIGNED_INT, (const GLvoid*)(0) /* offset */);
        RenderStats::recordDrawCall(renderData.mIndexCount / 3);
    };
}

void GrassRenderer::renderPlaneGrass(const Camera3D& camera, const f32v3& playerPos, const std::vector<GrassMeshRenderDataWithPos>& grassMeshes) {
    if (grassMeshes.empty()) {
        return;
    }
    if (!mShaderAssets.areAllAssetsLoaded()) {
        return;
    }

    const MaterialShaderDef* grassMaterial = mMaterials[e_cast(TileGrassMeshType::PLANE)];
    MaterialRenderer::bindMaterialShaderForRender(*grassMaterial);
    const vg::GLProgram& program = grassMaterial->mProgram;
    VGUniform positionUniform = program.getUniform("unPosition");
    VGUniform crossfadeAlphaUniform = program.getUniform("unCrossfadeAlpha");
    VGUniform crossfadeDirectionUniform = program.getUniform("unCrossfadeDirection");
    VGUniform tboSizeTypeUniform = program.getUniform("UnTboSizeType");
    VGUniform tboPositionUniform = program.getUniform("UnTboPosition");
    VGUniform tboNormalUniform = program.getUniform("UnTboNormal");
    //glUniform3fv(program.getUniform("unPlayerPos"), 1, &playerPos.x); // No player collision yet
    glUniform1f(program.getUniform("unFadeDistance"), sDebugOptions.mGrassSettings.fadeDistance);
    glUniform2f(program.getUniform("unScale"), sDebugOptions.mGrassScale.x, sDebugOptions.mGrassScale.y);
    glUniform1f(program.getUniform("unDitherPower"), sDebugOptions.mGrassDitherPower);
    glUniform1f(program.getUniform("unColorMapScale"), sDebugOptions.mGrassColorMapScale);

    glUniform1i(tboSizeTypeUniform, GRASS_TBO_INSTANCE_DATA_BINDING);
    glUniform1i(tboPositionUniform, GRASS_TBO_POSITION_DATA_BINDING);
    glUniform1i(tboNormalUniform, GRASS_TBO_NORMAL_DATA_BINDING);

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
        glBindTextureUnit(GRASS_TBO_NORMAL_DATA_BINDING, renderData.mTboNormalData);

        glDrawElements(GL_TRIANGLES, renderData.mIndexCount, GL_UNSIGNED_INT, (const GLvoid*)(0) /* offset */);
        RenderStats::recordDrawCall(renderData.mIndexCount / 3);
    };
}

void GrassRenderer::renderBillboardGrass(const Camera3D& camera, const f32v3& playerPos, const std::vector<GrassMeshRenderDataWithPos>& grassMeshes) {
    if (grassMeshes.empty()) {
        return;
    }
    if (!mShaderAssets.areAllAssetsLoaded()) {
        return;
    }

    const MaterialShaderDef* grassMaterial = mMaterials[e_cast(TileGrassMeshType::BILLBOARD)];
    MaterialRenderer::bindMaterialShaderForRender(*grassMaterial);
    const vg::GLProgram& program = grassMaterial->mProgram;
    VGUniform positionUniform = program.getUniform("unPosition");
    VGUniform crossfadeAlphaUniform = program.getUniform("unCrossfadeAlpha");
    VGUniform crossfadeDirectionUniform = program.getUniform("unCrossfadeDirection");
    VGUniform tboSizeTypeUniform = program.getUniform("UnTboSizeType");
    VGUniform tboPositionUniform = program.getUniform("UnTboPosition");
    //VGUniform tboNormalUniform = program.getUniform("UnTboNormal");
    //glUniform3fv(program.getUniform("unPlayerPos"), 1, &playerPos.x); // No player collision yet
    glUniform1f(program.getUniform("unFadeDistance"), sDebugOptions.mGrassSettings.fadeDistance);
    glUniform2f(program.getUniform("unScale"), sDebugOptions.mGrassScale.x, sDebugOptions.mGrassScale.y);
    glUniform1f(program.getUniform("unDitherPower"), sDebugOptions.mGrassDitherPower);
    glUniform1f(program.getUniform("unColorMapScale"), sDebugOptions.mGrassColorMapScale);

    glUniform1i(tboSizeTypeUniform, GRASS_TBO_INSTANCE_DATA_BINDING);
    glUniform1i(tboPositionUniform, GRASS_TBO_POSITION_DATA_BINDING);
   // glUniform1i(tboNormalUniform, GRASS_TBO_NORMAL_DATA_BINDING);

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
        //glBindTextureUnit(GRASS_TBO_NORMAL_DATA_BINDING, renderData.mTboNormalData);

        glDrawElements(GL_TRIANGLES, renderData.mIndexCount, GL_UNSIGNED_INT, (const GLvoid*)(0) /* offset */);
        RenderStats::recordDrawCall(renderData.mIndexCount / 3);
    };
}

void GrassRenderer::renderGrass(const Camera3D& camera, const f32v3& playerPos, const boost::container::flat_set<const GrassMesh*>& grassMeshes) {
    
    for (int i = 0; i < e_count(TileGrassMeshType); ++i) {
        mVisibleMeshes[i].clear();
    }
    if (!mShaderAssets.areAllAssetsLoaded()) {
        return;
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
    renderBillboardGrass(camera, playerPos, mVisibleMeshes[e_cast(TileGrassMeshType::BILLBOARD)]);

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
    TileGrassRepository& grassRepo = TileGrassRepository::get();
    size_t count = grassRepo.getNumRegisteredAssets();
    assert(count < MAX_GRASS);

    GrassUniformData uboData[MAX_GRASS];

    size_t i = 0;
    grassRepo.forEachLoadedOrUnloadedAsset([&](TileGrassDef& def, const AssetMetadata& entry) {
        GrassUniformData& data = uboData[i];
        data.grassScale = def.mSizeMults;
        data.material = def.mMaterialID;
        data.materialCellCount = def.mNumTextures;
        data.shouldUseColorGradient = (int)def.mUseGradientColor;
        data.leanVariance = (float)def.mLeanVariance;
        ++i;
        return false;
    });
    if (sGrassUniformBuffer) {
        glDeleteBuffers(1, &sGrassUniformBuffer);
    }
    glCreateBuffers(1, &sGrassUniformBuffer);
    glNamedBufferStorage(sGrassUniformBuffer, count * sizeof(GrassUniformData), uboData, 0);
}
