#include "stdafx.h"
#include "TerrainRenderer.h"

#include "rendering/MaterialRenderer.h"
#include "rendering/MaterialShaderManager.h"
#include "rendering/MaterialUtils.h"

#include "rendering/texture/Cubemap.h"
#include "rendering/material/BrdfLUT.h"

#include <Vorb/graphics/GBuffer.h>

#include "world/HeightmapTerrainQuadtree.h"

#include "resources/ResourceManager.h"
#include "camera/Camera3D.h"
#include "mesh/Mesh.h"

#include "options/DebugOptions.h"

TerrainRenderer::TerrainRenderer()
{
    const MaterialShaderManager& materialManager = Services::ResourceManager::ref().getMaterialShaderManager();
    mTerrainMaterial = materialManager.getMaterialShader("terrain");
    mWaterMaterial = materialManager.getMaterialShader("water");
    mWaterPbrMaterial = materialManager.getMaterialShader("water_pbr");
}

void TerrainRenderer::renderTerrain(const Camera3D& camera, const std::set<const TerrainMesh*>& terrainMeshes) {

    glEnable(GL_CULL_FACE);

    MaterialRenderer::bindMaterialForRender(*mTerrainMaterial);
    // Terrain uniforms
    glUniform1f(mTerrainMaterial->mProgram.getUniform("unHeightMult"), sDebugOptions.mTerrainHeightColorMult);
    glUniform1f(mTerrainMaterial->mProgram.getUniform("unWavyMult"), sDebugOptions.mTerrainWavyColorMult);
    glUniform1f(mTerrainMaterial->mProgram.getUniform("unSquaresPeriod"), sDebugOptions.mTerrainSquaresColorPeriod);
    glUniform1f(mTerrainMaterial->mProgram.getUniform("unSquaresIntensity"), sDebugOptions.mTerrainSquaresIntensity);
    glUniform1f(mTerrainMaterial->mProgram.getUniform("unBlendMult"), sDebugOptions.mTerrainBlendMult);
    glUniform1f(mTerrainMaterial->mProgram.getUniform("unGrassColorV"), sDebugOptions.mTerrainGrassColorV);
    VGUniform positionUniform = mTerrainMaterial->mProgram.getUniform("unPosition");
    VGUniform crossfadeAlphaUniform = mTerrainMaterial->mProgram.getUniform("unCrossfadeAlpha");
    VGUniform crossfadeDirectionUniform = mTerrainMaterial->mProgram.getUniform("unCrossfadeDirection");

    for (auto&& terrainMesh : terrainMeshes) {
        const Mesh& mesh = terrainMesh->mMesh;
        const BoundingSphere& bounds = mesh.getBoundingSphere();
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
            f32v3 position = mesh.getPosition();
            glUniform3fv(positionUniform, 1, &position.x);
            mesh.draw();
        }
    }
}

void TerrainRenderer::renderWater(const Camera3D& camera, const std::set<const TerrainMesh*>& waterMeshes, const Cubemap& skyCubeMap)
{
    glDisable(GL_CULL_FACE);
    vg::DepthState::READ.set();
    const MaterialShader* shader;
    if (sDebugOptions.mUsingPBR) {
        shader = mWaterPbrMaterial;
        ui32 textureUnit;
        MaterialRenderer::bindMaterialForRender(*shader, &textureUnit);
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
        glUniform2f(shader->getUniform("unHazeDivisor"), optionsLeft.mHazeDivisor, optionsRight.mHazeDivisor);
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
        MaterialRenderer::bindMaterialForRender(*shader);
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
        const Mesh& mesh = waterMesh->mMesh;
        f32v3 offset = mesh.getPosition() - camera.getPosition();;
        glUniform3fv(offsetUniform, 1, &offset.x);

        const ui32 lod = QUADTREE_LOD_FROM_INDEX[waterMesh->mIndex];
        /* f32v2 centerPos = f32v2(HeightmapTerrainQuadtree::PATCH_POSITIONS.data[terrainMesh->mIndex].xy) + f32v2(HeightmapTerrainQuadtree::LOD_HALF_DIMS[lod].xy);
            f32v3 centerPos3d(centerPos.x, centerPos.y, 0.0f);*/
        int crossfadeDir = waterMesh->mCrossfadeDir.load();
        const BoundingSphere& bounds = mesh.getBoundingSphere();
        if (camera.sphereIsVisible(bounds.center, bounds.radius)) {
            mesh.draw();
        }
    }
}
