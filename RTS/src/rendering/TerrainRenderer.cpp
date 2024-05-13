#include "stdafx.h"
#include "TerrainRenderer.h"

#include "rendering/MaterialRenderer.h"
#include "rendering/MaterialShaderRepository.h"
#include "rendering/MaterialUtils.h"
#include "definitions/rendering/CubemapDef.h"

#include "rendering/texture/TextureConvert.h"
#include "rendering/material/BrdfLUT.h"

#include "filesystem/FileSystem.h"

#include <Vorb/graphics/GBuffer.h>
#include <Vorb/graphics/BlendState.h>

#include "world/HeightmapTerrainQuadtree.h"
#include "world/World.h"
#include "world/biome/BiomeGrid.h"

#include "resources/ResourceManager.h"
#include "resources/BiomeRepository.h"
#include "resources/MaterialRepository.h"
#include "camera/Camera3D.h"
#include "mesh/Mesh.h"
#include "mesh/MeshDrawer.h"

#include "weather/WeatherManager.h"

#include "options/LightingOptions.h"
#include "options/DebugOptions.h"

TerrainRenderer::TerrainRenderer() {
    mTerrainMaterial = AssetUtil::addAssetToBundleAndGetUnloaded<MaterialShaderDef>(mShaderAssets, CStrToken("terrain"));
    mWaterMaterial = AssetUtil::addAssetToBundleAndGetUnloaded<MaterialShaderDef>(mShaderAssets, CStrToken("water"));
    mWaterPbrMaterial = AssetUtil::addAssetToBundleAndGetUnloaded<MaterialShaderDef>(mShaderAssets, CStrToken("water_pbr"));

    // Terrain layer materials are stored in a texture, with resolution equal to the vertex resolution.
    // To achieve blending, we pre-compute 256 gradient textures on the CPU representing all possible blend
    // combinations, where neighboring 1 bits represent the same texture, and 0 represents a different texture.
    // We store which gradients we use in a texture at same resolution as layer materials,
    // which is updated when the terrain updates.
    // Note that some textures will be duplicate... thats ok. For example:
    // 0 1  will have the same gradient from perspective of top right as 0 1
    // 0 0                                                               1 0

    // TODO: Data drive layers!
    mMaterialsLookup[e_cast(TerrainTextureType::None)] = 0;
    mMaterialsLookup[e_cast(TerrainTextureType::Dirt)] = MaterialRepository::get().getMaterialId(CStrToken("dirt_road"));
    mMaterialsLookup[e_cast(TerrainTextureType::FarmPlot)] = MaterialRepository::get().getMaterialId(CStrToken("farm_plot_2"));

    buildSurfaceDensityGradientMaps();
}

void TerrainRenderer::setActiveWorld(World& world) {
    mBiomeTexture = world.getBiomeGrid().getBiomeTexture();
    mInverseWorldWidth = (f32)(1.0 / (f64)world.getWidthTiles());
    mWeatherManager = &world.getWeatherManager();
}

void TerrainRenderer::renderTerrain(const Camera3D& camera, const boost::container::flat_set<const TerrainMesh*>& terrainMeshes) {
    if (!mShaderAssets.areAllAssetsLoaded()) {
        return;
    }

    glEnable(GL_CULL_FACE);
    //vg::BlendState::set(vorb::graphics::BlendStateType::REPLACE);

    ui32 nextTextureUnit = 0;
    MaterialRenderer::bindMaterialShaderForRender(*mTerrainMaterial, &nextTextureUnit);
    // Terrain uniforms
    glUniform1f(mTerrainMaterial->mProgram.getUniform("unInverseWorldWidth"), mInverseWorldWidth);
    glUniform1f(mTerrainMaterial->mProgram.getUniform("unHeightMult"), sDebugOptions.mTerrainHeightColorMult);
    glUniform1f(mTerrainMaterial->mProgram.getUniform("unWavyMult"), sDebugOptions.mTerrainWavyColorMult);
    glUniform1f(mTerrainMaterial->mProgram.getUniform("unSquaresPeriod"), sDebugOptions.mTerrainSquaresColorPeriod);
    glUniform1f(mTerrainMaterial->mProgram.getUniform("unSquaresIntensity"), sDebugOptions.mTerrainSquaresIntensity);
    glUniform1f(mTerrainMaterial->mProgram.getUniform("unBlendMult"), sDebugOptions.mTerrainBlendMult);
    glUniform1f(mTerrainMaterial->mProgram.getUniform("unColorMapScale"), sDebugOptions.mGrassColorMapScale);
    glUniform1f(mTerrainMaterial->mProgram.getUniform("unBiomeBlendScale"), sDebugOptions.mBiomeBlendScale);
    glUniform1f(mTerrainMaterial->mProgram.getUniform("unBiomeBlendFrequency"), sDebugOptions.mBiomeBlendFrequency);
    glUniform1f(mTerrainMaterial->mProgram.getUniform("unDetailTextureStrength"), sDebugOptions.mTerrainDetailTextureStrength);
    glUniform1f(mTerrainMaterial->mProgram.getUniform("unSnowLevel"), mWeatherManager->mSnowLevel);
    glUniform1f(mTerrainMaterial->mProgram.getUniform("unCliffBlendHardness"), sDebugOptions.mTerrainCliffBlendHardness);
    glUniform1f(mTerrainMaterial->mProgram.getUniform("unCliffAmount"), sDebugOptions.mTerrainCliffAmount);
    glUniform1f(mTerrainMaterial->mProgram.getUniform("unCliffZMult"), sDebugOptions.mTerrainCliffZMult);

    const VGUniform unSplatMaterialsUniform = mTerrainMaterial->mProgram.getUniform("unSplatMaterials[0]");
    glUniform1uiv(unSplatMaterialsUniform, e_count(TerrainTextureType), mMaterialsLookup);

    const VGUniform patchWidthUniform = mTerrainMaterial->mProgram.getUniform("unPatchWidth");
    const VGUniform positionUniform = mTerrainMaterial->mProgram.getUniform("unPosition");
    const VGUniform crossfadeAlphaUniform = mTerrainMaterial->mProgram.getUniform("unCrossfadeAlpha");
    const VGUniform crossfadeDirectionUniform = mTerrainMaterial->mProgram.getUniform("unCrossfadeDirection");
    const VGUniform uvRootUniform = mTerrainMaterial->mProgram.getUniform("unUVRoot");
    glUniform1i(mTerrainMaterial->mProgram.getUniform("unBiomeTexture"), nextTextureUnit);
    glBindTextureUnit(nextTextureUnit, mBiomeTexture);

    ++nextTextureUnit;
    glUniform1i(mTerrainMaterial->mProgram.getUniform("unBiomeColorMapsTexture"), nextTextureUnit);
    glBindTextureUnit(nextTextureUnit, BiomeRepository::get().getBiomeColorMapsArrayTexture());

    ++nextTextureUnit;
    glUniform1i(mTerrainMaterial->mProgram.getUniform("unSurfaceDensityGradientMaps"), nextTextureUnit);
    glBindTextureUnit(nextTextureUnit, mSurfaceDensityGradientMapsArray);
    
    const ui32 splatTextureUnit = ++nextTextureUnit;
    glUniform1i(mTerrainMaterial->mProgram.getUniform("unSurfaceTextures"), splatTextureUnit);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_BASE_TERRAIN_COLOR_MAPS_SSBO, BiomeRepository::get().getBiomeColorMapsShaderLookupBuffer());

    for (auto&& terrainMesh : terrainMeshes) {
        const BoundingSphere& bounds = terrainMesh->getBoundingSphere();
        if (camera.sphereIsVisible(bounds.center, bounds.radius)) {
            const ui32 lod = QUADTREE_LOD_FROM_INDEX[terrainMesh->mIndex];
            /*  f32v2 centerPos = f32v2(HeightmapTerrainQuadtree::PATCH_POSITIONS.data[terrainMesh->mIndex].xy) + f32v2(HeightmapTerrainQuadtree::LOD_HALF_DIMS[lod].xy);
              f32v3 centerPos3d(centerPos.x, centerPos.y, 0.0f);*/
            int crossfadeDir = terrainMesh->mCrossfadeDir.load();
            if (crossfadeDir != 0) {
                glUniform1f(crossfadeAlphaUniform, terrainMesh->mCrossfadeAlpha.load() * 0.5f /* Constant that was selected via trial and error*/);
                glUniform1f(crossfadeDirectionUniform, (crossfadeDir > 0) ? 1.0f : 0.0f);
            }
            else {
                glUniform1f(crossfadeAlphaUniform, 0.0f);
                glUniform1f(crossfadeDirectionUniform, 0.0f);
            }
            glUniform1f(patchWidthUniform, (f32)(HeightmapTerrainQuadtree::LOD_HALF_DIMS[lod].x << 1));
            glUniform3fv(positionUniform, 1, &terrainMesh->getPosition().x);
            glUniform2fv(uvRootUniform, 1, &terrainMesh->mUVRoot.x);

            glBindTextureUnit(splatTextureUnit, terrainMesh->mTerrainSplatTexture);

            MeshDrawer::draw(terrainMesh->mGpuData);
        }
    }
}

void TerrainRenderer::renderWater(const Camera3D& camera, const boost::container::flat_set<const TerrainMesh*>& waterMeshes, const CubemapDef& skyCubeMap) {
    if (!mShaderAssets.areAllAssetsLoaded()) {
        return;
    }

    glDisable(GL_CULL_FACE);
    vg::DepthState::READ.set();
    vg::BlendState::set(vorb::graphics::BlendStateType::ALPHA);

    const MaterialShaderDef* shader;
    if (sDebugOptions.mUsingPBR) {
        shader = mWaterPbrMaterial;
        ui32 textureUnit;
        MaterialRenderer::bindMaterialShaderForRender(*shader, &textureUnit);
        glUniform1i(shader->getUniform("unIrradianceMap"), textureUnit);
        glBindTextureUnit(textureUnit++, skyCubeMap.getIrradianceTexture());
        glUniform1i(shader->getUniform("unPrefilterMap"), textureUnit);
        glBindTextureUnit(textureUnit++, skyCubeMap.getPrefilterMap());
        glUniform1i(shader->getUniform("unBrdfLUT"), textureUnit);
        glBindTextureUnit(textureUnit++, BrdfLUT::getTexture());

        glUniform1f(shader->getUniform("unWaterMetallic"), sDebugOptions.mWaterMetallic);
        glUniform1f(shader->getUniform("unWaterRoughness"), sDebugOptions.mWaterRoughness);


        LightingOptions& optionsLeft = *sDebugOptions.mLightingOptions;
        LightingOptions& optionsRight = *sDebugOptions.mLightingOptionsSplit;
        glUniform2f(shader->getUniform("unAmbient"), optionsLeft.mAmbient, optionsRight.mAmbient);
        glUniform2f(shader->getUniform("unExposure"), optionsLeft.mExposure, optionsRight.mExposure);
        glUniform2f(shader->getUniform("unHazeExponent"), optionsLeft.mHazeExponent, optionsRight.mHazeExponent);
        if (sDebugOptions.mIsCameraUnderwater) {
            glUniform2f(shader->getUniform("unHazeDivisor"), sDebugOptions.mUnderwaterHazeDivisor, sDebugOptions.mUnderwaterHazeDivisor);
        }
        else {
            glUniform2f(shader->getUniform("unHazeDivisor"), optionsLeft.mHazeDivisor, optionsRight.mHazeDivisor);
        }
        glUniform2f(shader->getUniform("unSunIntensity"), optionsLeft.mSunIntensity, optionsRight.mSunIntensity);
        if (sDebugOptions.mLightPresetSplitView) {
            glUniform1f(shader->getUniform("unLightingSplit"), sDebugOptions.mLightPresetSplitAmount);
        }
        else {
            glUniform1f(shader->getUniform("unLightingSplit"), 1.0f);
        }
    }
    else {
        shader = mWaterMaterial;
        MaterialRenderer::bindMaterialShaderForRender(*shader);
        MaterialUtils::uploadLightingUniforms(*shader);
    }

    // TODO: UBO?
    // Water uniforms
    glUniform4fv(shader->mProgram.getUniform("unShallowColor"), 1, &sDebugOptions.mShallowWaterColor.x);
    glUniform4fv(shader->mProgram.getUniform("unDeepColor"), 1, &sDebugOptions.mDeepWaterColor.x);
    glUniform4fv(shader->mProgram.getUniform("unFoamColor"), 1, &sDebugOptions.mWaterFoamColor.x);
    glUniform1f(shader->mProgram.getUniform("unSurfaceDistortAmount"), sDebugOptions.mWaterSurfaceDistortAmount);
    glUniform1f(shader->mProgram.getUniform("unSurfaceMoveSpeed"), sDebugOptions.mWaterSurfaceMoveSpeed);
    glUniform2fv(shader->mProgram.getUniform("unFoamDistanceRange"), 1, &sDebugOptions.mWaterFoamDistanceRange.x);
    glUniform1f(shader->mProgram.getUniform("unSurfaceNoiseCutoff"), sDebugOptions.mWaterSurfaceNoiseCutoff);
    glUniform1f(shader->mProgram.getUniform("unSmoothstepAA"), sDebugOptions.mWaterSmoothstepAA);
    glUniform1f(shader->mProgram.getUniform("unColorNoiseIntensity"), sDebugOptions.mWaterColorNoiseIntensity);
    glUniform1f(shader->mProgram.getUniform("unDistortTiling"), sDebugOptions.mWaterDistortTiling);
    glUniform1f(shader->mProgram.getUniform("unNoiseTiling"), sDebugOptions.mWaterNoiseTiling);
    VGUniform offsetUniform = shader->mProgram.getUniform("unOffset");
    for (auto&& waterMesh : waterMeshes) {
        f32v3 offset = waterMesh->getPosition() - camera.getPosition();;
        glUniform3fv(offsetUniform, 1, &offset.x);

        const ui32 lod = QUADTREE_LOD_FROM_INDEX[waterMesh->mIndex];
        /* f32v2 centerPos = f32v2(HeightmapTerrainQuadtree::PATCH_POSITIONS.data[terrainMesh->mIndex].xy) + f32v2(HeightmapTerrainQuadtree::LOD_HALF_DIMS[lod].xy);
            f32v3 centerPos3d(centerPos.x, centerPos.y, 0.0f);*/
        int crossfadeDir = waterMesh->mCrossfadeDir.load();
        const BoundingSphere& bounds = waterMesh->getBoundingSphere();
        if (camera.sphereIsVisible(bounds.center, bounds.radius)) {
            MeshDrawer::draw(waterMesh->mGpuData);
        }
    }


    vg::BlendState::restorePrevious();
}

void TerrainRenderer::buildSurfaceDensityGradientMaps() {
    ASSERT_RENDER_THREAD();
    // Build adjacency density gradient maps for the surfaces based on
    // 1 being same texture at an adjacent grid position, and 0 being a different texture

    constexpr int MAP_RESOLUTION = 256;
    constexpr int HALF_MAP_RESOLUTION = MAP_RESOLUTION / 2;
    constexpr f32 BLEND_RADIUS = 75.0f;
    static_assert((int)BLEND_RADIUS < HALF_MAP_RESOLUTION);
    constexpr int MAX_COORDINATE = MAP_RESOLUTION - 1;
    constexpr int NUM_LAYERS = UINT8_MAX + 1;

    PreciseTimer timer;
    assert(mSurfaceDensityGradientMapsArray == 0);

    std::vector<gli::texture2d> gradientData;
    gradientData.resize(NUM_LAYERS);

    auto isBitZero = [](int i, int b) {
        return (i & (1 << b)) == 0;
    };

    LOG_INFO("Loading surface density gradient maps");

    std::filesystem::path densityTextureFolder = ResourceManager::get().getResourceRoot().getStdPath() / "textures/terrain/density_grad/";
    FileSystem::createDirectory(densityTextureFolder.string());

    const int mipLevels = 1 + (int)std::floor(std::log2(MAP_RESOLUTION));

    glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &mSurfaceDensityGradientMapsArray);
    glTextureStorage3D(
        mSurfaceDensityGradientMapsArray,
        mipLevels,
        GL_COMPRESSED_RED_RGTC1,
        MAP_RESOLUTION,
        MAP_RESOLUTION,
        NUM_LAYERS
    );

    auto uploadLayer = [&](gli::texture2d& loadedLayer, int layer) {
        for (gli::texture2d::size_type level = 0; level < mipLevels; ++level) {
            // Get extent of the current mip level
            const gli::extent2d levelExtent = loadedLayer.extent(level);

            // Upload the compressed texture data for this mip level to OpenGL
            glCompressedTextureSubImage3D(
                mSurfaceDensityGradientMapsArray,
                static_cast<GLint>(level),
                0, 0,
                layer,
                levelExtent.x,
                levelExtent.y,
                1,
                GL_COMPRESSED_RED_RGTC1,
                static_cast<GLsizei>(loadedLayer.size(level)),
                loadedLayer.data(0, 0, level)
            );
        }
    };

    // Generation is inefficient but should never be done at runtime after the first time, as we
    // will cache to DDS on disk. DDS load is fast
    // 5 6 7
    // 3   4
    // 0 1 2
    for (int i = 0; i <= UINT8_MAX; ++i) {
        const std::filesystem::path ddsPath = densityTextureFolder / ("sfd" + std::to_string(i) + ".dds");
        if (FileSystem::exists(ddsPath.string())) {
            gli::texture2d loadedLayer = static_cast<gli::texture2d>(gli::load(ddsPath.string()));
            assert(loadedLayer.format() == gli::FORMAT_R_ATI1N_UNORM_BLOCK8);
            assert(loadedLayer.extent().x == MAP_RESOLUTION && loadedLayer.extent().y == MAP_RESOLUTION);
            assert(loadedLayer.levels() == mipLevels);
            uploadLayer(loadedLayer, i);
        }
        else {

            // Should not happen at user run time unless they deleted the files!
            LOG_ERROR("Surface density map {} missing from disk, generating now...", ddsPath.string());
            gli::texture2d& data = gradientData[i];
            data = gli::texture2d(gli::format::FORMAT_R8_UNORM_PACK8, gli::extent2d(MAP_RESOLUTION, MAP_RESOLUTION));
            // Treating +y as up in the map as well as in world space
            for (int y = 0; y < MAP_RESOLUTION; ++y) {
                for (int x = 0; x < MAP_RESOLUTION; ++x) {

                    f32 lowestDensity = 1.0f;
                    // Quadrants
                    if (x < HALF_MAP_RESOLUTION) {
                        if (y < HALF_MAP_RESOLUTION) {
                            // Bottom left
                            if (isBitZero(i, 3)) {
                                lowestDensity = std::min(lowestDensity, x / BLEND_RADIUS);
                            }
                            if (isBitZero(i, 1)) {
                                lowestDensity = std::min(lowestDensity, y / BLEND_RADIUS);
                            }
                            if (isBitZero(i, 0)) {
                                lowestDensity = std::min(lowestDensity, f32(sqrt(SQ(x) + SQ(y)) / BLEND_RADIUS));
                            }
                        }
                        else {
                            // Top left
                            if (isBitZero(i, 3)) {
                                lowestDensity = std::min(lowestDensity, x / BLEND_RADIUS);
                            }
                            if (isBitZero(i, 6)) {
                                lowestDensity = std::min(lowestDensity, (MAX_COORDINATE - y) / BLEND_RADIUS);
                            }
                            if (isBitZero(i, 5)) {
                                lowestDensity = std::min(lowestDensity, f32(sqrt(SQ(x) + SQ(MAX_COORDINATE - y)) / BLEND_RADIUS));
                            }
                        }
                    }
                    else {
                        if (y < HALF_MAP_RESOLUTION) {
                            // Bottom right
                            if (isBitZero(i, 4)) {
                                lowestDensity = std::min(lowestDensity, (MAX_COORDINATE - x) / BLEND_RADIUS);
                            }
                            if (isBitZero(i, 1)) {
                                lowestDensity = std::min(lowestDensity, y / BLEND_RADIUS);
                            }
                            if (isBitZero(i, 2)) {
                                lowestDensity = std::min(lowestDensity, f32(sqrt(SQ(MAX_COORDINATE - x) + SQ(y)) / BLEND_RADIUS));
                            }
                        }
                        else {
                            // Top right
                            if (isBitZero(i, 4)) {
                                lowestDensity = std::min(lowestDensity, (MAX_COORDINATE - x) / BLEND_RADIUS);
                            }
                            if (isBitZero(i, 6)) {
                                lowestDensity = std::min(lowestDensity, (MAX_COORDINATE - y) / BLEND_RADIUS);
                            }
                            if (isBitZero(i, 7)) {
                                lowestDensity = std::min(lowestDensity, f32(sqrt(SQ(MAX_COORDINATE - x) + SQ(MAX_COORDINATE - y)) / BLEND_RADIUS));
                            }
                        }
                    }
                    data.store(gli::texture2d::extent_type(x, y), 0, (ui8)round(lowestDensity * UINT8_MAX));
                }
            }
            LOG_INFO("Compressing {}...", i);
            gli::texture2d compressed = TextureConvert::convertToDDS(data, true /*generateMipmaps*/);
            uploadLayer(compressed, i);
            gli::save(compressed, ddsPath.string());
        }
    }
    vg::sSamplerStates.LINEAR_CLAMP_MIPMAP.setForTexture(mSurfaceDensityGradientMapsArray);
    LOG_INFO("Loaded surface density gradient maps in {} ms", timer.stop());
}
