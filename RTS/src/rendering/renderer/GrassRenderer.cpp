#include "stdafx.h"
#include "GrassRenderer.h"

#include "rendering/GrassBillboardMesh.h"
#include "rendering/ChunkGrassQuadtree.h"
#include "resources/MaterialRepository.h"
#include "rendering/MaterialShaderRepository.h"
#include "rendering/MaterialRenderer.h"
#include "rendering/RenderStats.h"

#include "weather/WeatherManager.h"

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

void GrassRenderer::setActiveWorld(World& world) {
    mBiomeTexture = world.getBiomeGrid().getBiomeTexture();
    mInverseWorldWidth = (f32)(1.0 / (f64)world.getWidthTiles());
    mWeatherManager = &world.getWeatherManager();
}

void GrassRenderer::renderDefaultGrass(const Camera3D& camera, const f32v3& playerPos, const std::vector<GrassMeshFrameRenderData>& grassMeshes) {
    if (grassMeshes.empty()) {
        return;
    }
    if (!mShaderAssets.areAllAssetsLoaded()) {
        return;
    }

    // TODO: Move into lazy init with other uniforms?
    const MaterialShaderDef* grassMaterial = mMaterials[e_cast(TileGrassMeshType::DEFAULT)];
    const vg::GLProgram& program = grassMaterial->mProgram;
    cacheUniforms(program);

    ui32 nextTextureUnit = 0;
    MaterialRenderer::bindMaterialShaderForRender(*grassMaterial, &nextTextureUnit);
    VGUniform tboNormalUniform = program.getUniform("UnTboNormal");
    VGUniform uvRootUniform = program.getUniform("unUVRoot");
    glUniform3fv(program.getUniform("unPlayerPos"), 1, &playerPos.x);
    glUniform1f(program.getUniform("unLeanVariance"), sDebugOptions.mGrassLeanVariance);
    glUniform1i(tboNormalUniform, GRASS_TBO_NORMAL_DATA_BINDING);

    uploadSharedUniforms(program, nextTextureUnit);

    glPatchParameteri(GL_PATCH_VERTICES, 3);

    for (auto&& grassMesh : grassMeshes) {
        const GrassBillboardMeshRenderData& renderData = grassMesh.renderData;

        // Make sure we have been initialized
        assert(renderData.mVao);
        if (!renderData.mIndexCount) return;

        { // Compute with high precision to avoid precision issues in shader
            f64v2 rootUVDouble = f64v2(grassMesh.pos.x, grassMesh.pos.y) * f64(sDebugOptions.mGrassColorMapScale);
            f64 intpart;
            f32v2 rootUv;
            rootUv.x = (f32)modf(rootUVDouble.x, &intpart);
            rootUv.y = (f32)modf(rootUVDouble.y, &intpart);
            glUniform2fv(uvRootUniform, 1, &rootUv.x);
        }

        uploadGrassMeshUniforms(grassMesh);

        glBindVertexArray(renderData.mVao);

        // Bind textures
        glBindTextureUnit(GRASS_TBO_INSTANCE_DATA_BINDING, renderData.mTboInstanceData);
        glBindTextureUnit(GRASS_TBO_POSITION_DATA_BINDING, renderData.mTboPositionData);
        glBindTextureUnit(GRASS_TBO_NORMAL_DATA_BINDING, renderData.mTboNormalData);

        glDrawElements(GL_PATCHES, renderData.mIndexCount, GL_UNSIGNED_INT, (const GLvoid*)(0) /* offset */);
        RenderStats::recordDrawCall(renderData.mIndexCount / 3);
    };
}

void GrassRenderer::renderPlaneGrass(const Camera3D& camera, const f32v3& playerPos, const std::vector<GrassMeshFrameRenderData>& grassMeshes) {
    if (grassMeshes.empty()) {
        return;
    }
    if (!mShaderAssets.areAllAssetsLoaded()) {
        return;
    }

    const MaterialShaderDef* grassMaterial = mMaterials[e_cast(TileGrassMeshType::PLANE)];
    ui32 nextTextureUnit = 0;
    MaterialRenderer::bindMaterialShaderForRender(*grassMaterial, &nextTextureUnit);
    const vg::GLProgram& program = grassMaterial->mProgram;
    cacheUniforms(program);


    VGUniform positionUniform = program.getUniform("unPosition");
    VGUniform tboNormalUniform = program.getUniform("UnTboNormal");
    uploadSharedUniforms(program, nextTextureUnit);
    glUniform1i(tboNormalUniform, GRASS_TBO_NORMAL_DATA_BINDING);

    for (auto&& grassMesh : grassMeshes) {
        const GrassBillboardMeshRenderData& renderData = grassMesh.renderData;

        // Make sure we have been initialized
        assert(renderData.mVao);
        if (!renderData.mIndexCount) return;

        uploadGrassMeshUniforms(grassMesh);

        glBindVertexArray(renderData.mVao);

        // Bind textures
        glBindTextureUnit(GRASS_TBO_INSTANCE_DATA_BINDING, renderData.mTboInstanceData);
        glBindTextureUnit(GRASS_TBO_POSITION_DATA_BINDING, renderData.mTboPositionData);
        glBindTextureUnit(GRASS_TBO_NORMAL_DATA_BINDING, renderData.mTboNormalData);

        glDrawElements(GL_TRIANGLES, renderData.mIndexCount, GL_UNSIGNED_INT, (const GLvoid*)(0) /* offset */);
        RenderStats::recordDrawCall(renderData.mIndexCount / 3);
    };
}

void GrassRenderer::renderBillboardGrass(const Camera3D& camera, const f32v3& playerPos, const std::vector<GrassMeshFrameRenderData>& grassMeshes) {
    if (grassMeshes.empty()) {
        return;
    }
    if (!mShaderAssets.areAllAssetsLoaded()) {
        return;
    }

    const MaterialShaderDef* grassMaterial = mMaterials[e_cast(TileGrassMeshType::BILLBOARD)];
    ui32 nextTextureUnit = 0;
    MaterialRenderer::bindMaterialShaderForRender(*grassMaterial, &nextTextureUnit);
    const vg::GLProgram& program = grassMaterial->mProgram;
    cacheUniforms(program);

    uploadSharedUniforms(program, nextTextureUnit);
    //VGUniform tboNormalUniform = program.getUniform("UnTboNormal");
    //glUniform3fv(program.getUniform("unPlayerPos"), 1, &playerPos.x); // No player collision yet
   // glUniform1i(tboNormalUniform, GRASS_TBO_NORMAL_DATA_BINDING);

    for (auto&& grassMesh : grassMeshes) {
        const GrassBillboardMeshRenderData& renderData = grassMesh.renderData;

        // Make sure we have been initialized
        assert(renderData.mVao);
        if (!renderData.mIndexCount) return;

        uploadGrassMeshUniforms(grassMesh);

        glBindVertexArray(renderData.mVao);

        // Bind textures
        glBindTextureUnit(GRASS_TBO_INSTANCE_DATA_BINDING, renderData.mTboInstanceData);
        glBindTextureUnit(GRASS_TBO_POSITION_DATA_BINDING, renderData.mTboPositionData);
        //glBindTextureUnit(GRASS_TBO_NORMAL_DATA_BINDING, renderData.mTboNormalData);

        glDrawElements(GL_TRIANGLES, renderData.mIndexCount, GL_UNSIGNED_INT, (const GLvoid*)(0) /* offset */);
        RenderStats::recordDrawCall(renderData.mIndexCount / 3);
    };
}

void GrassRenderer::uploadSharedUniforms(const vg::GLProgram& program, ui32& nextTextureUnit) {

    glUniform1f(program.getUniform("unFadeDistance"), sDebugOptions.mGrassSettings.fadeDistance);
    glUniform2f(program.getUniform("unScale"), sDebugOptions.mGrassScale.x, sDebugOptions.mGrassScale.y);
    glUniform1f(program.getUniform("unColorMapScale"), sDebugOptions.mGrassColorMapScale);

    glUniform1f(program.getUniform("unInverseWorldWidth"), mInverseWorldWidth);
    if (mWeatherManager) [[likely]] {
        glUniform1f(program.getUniform("unSnowLevel"), mWeatherManager->mSnowLevel);
    }

    glUniform1i(program.getUniform("unBiomeTexture"), nextTextureUnit);
    glBindTextureUnit(nextTextureUnit, mBiomeTexture);

    ++nextTextureUnit;
    glUniform1i(program.getUniform("unBiomeColorMapsTexture"), nextTextureUnit);
    glBindTextureUnit(nextTextureUnit, BiomeRepository::get().getBiomeColorMapsArrayTexture());

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_BASE_TERRAIN_COLOR_MAPS_SSBO, BiomeRepository::get().getBiomeColorMapsShaderLookupBuffer());

    VGUniform tboSizeTypeUniform = program.getUniform("UnTboSizeType");
    VGUniform tboPositionUniform = program.getUniform("UnTboPosition");

    glUniform1i(tboSizeTypeUniform, GRASS_TBO_INSTANCE_DATA_BINDING);
    glUniform1i(tboPositionUniform, GRASS_TBO_POSITION_DATA_BINDING);
}

void GrassRenderer::uploadGrassMeshUniforms(const GrassMeshFrameRenderData& grassMesh) {
    glUniform3fv(mPositionUniform, 1, &grassMesh.pos.x);
    int crossfadeDir = grassMesh.crossfadeDir;
    if (crossfadeDir != 0) {
        glUniform1f(mCrossfadeAlphaUniform, crossfadeDir * grassMesh.crossfadeAlpha);
    }
    else {
        glUniform1f(mCrossfadeAlphaUniform, -MATH_EPSILON);
    }
}

void GrassRenderer::cacheUniforms(const vg::GLProgram& program) {
    mPositionUniform = program.getUniform("unPosition");
    mCrossfadeAlphaUniform = program.getUniform("unCrossfadeAlpha");
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
                    mVisibleMeshes[i].emplace_back(GrassMeshFrameRenderData{ 
                        .renderData = mesh.getRenderData((TileGrassMeshType)i),
                        .pos = grassMesh->mPosition,
                        .crossfadeDir = grassMesh->mCrossfadeDir,
                        .crossfadeAlpha = grassMesh->mCrossfadeAlpha});
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
