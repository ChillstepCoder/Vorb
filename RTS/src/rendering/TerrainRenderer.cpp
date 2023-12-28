#include "stdafx.h"
#include "TerrainRenderer.h"

#include "rendering/MaterialRenderer.h"
#include "rendering/MaterialShaderRepository.h"
#include "rendering/MaterialUtils.h"
#include "definitions/rendering/CubemapDef.h"

#include "rendering/material/BrdfLUT.h"

#include <Vorb/graphics/GBuffer.h>
#include <Vorb/graphics/BlendState.h>

#include "world/HeightmapTerrainQuadtree.h"
#include "world/World.h"
#include "world/biome/BiomeGrid.h"

#include "resources/ResourceManager.h"
#include "resources/BiomeRepository.h"
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

    VGUniform positionUniform = mTerrainMaterial->mProgram.getUniform("unPosition");
    VGUniform crossfadeAlphaUniform = mTerrainMaterial->mProgram.getUniform("unCrossfadeAlpha");
    VGUniform crossfadeDirectionUniform = mTerrainMaterial->mProgram.getUniform("unCrossfadeDirection");
    VGUniform uvRootUniform = mTerrainMaterial->mProgram.getUniform("unUVRoot");
    glUniform1i(mTerrainMaterial->mProgram.getUniform("unBiomeTexture"), nextTextureUnit);
    glBindTextureUnit(nextTextureUnit, mBiomeTexture);

    ++nextTextureUnit;
    glUniform1i(mTerrainMaterial->mProgram.getUniform("unBiomeColorMapsTexture"), nextTextureUnit);
    glBindTextureUnit(nextTextureUnit, BiomeRepository::get().getBiomeColorMapsArrayTexture());

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
            f32v3 position = terrainMesh->getPosition();
            glUniform3fv(positionUniform, 1, &position.x);
            glUniform2fv(uvRootUniform, 1, &terrainMesh->mUVRoot.x);
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
