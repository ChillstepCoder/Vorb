#include "stdafx.h"
#include "TerrainRenderer.h"

#include "rendering/MaterialRenderer.h"
#include "rendering/MaterialManager.h"
#include "rendering/MaterialUtils.h"

#include <Vorb/graphics/GBuffer.h>

#include "world/HeightmapTerrainQuadtree.h"

#include "ResourceManager.h"

#include "options/DebugOptions.h"

TerrainRenderer::TerrainRenderer(ResourceManager& resourceManager, const MaterialRenderer& materialRenderer) : mMaterialRenderer(materialRenderer)
{
    mTerrainMaterial = resourceManager.getMaterialManager().getMaterial("terrain");
    mWaterMaterial = resourceManager.getMaterialManager().getMaterial("water");
}

void TerrainRenderer::renderTerrain(const Camera3D& camera, const std::vector<HeightmapTerrainQuadtree>& terrainQuadtrees)
{
    mMaterialRenderer.bindMaterialForRender(*mTerrainMaterial);
    // Terrain uniforms
    glUniform1f(mTerrainMaterial->mProgram.getUniform("unHeightMult"), sDebugOptions.mTerrainHeightColorMult);
    glUniform1f(mTerrainMaterial->mProgram.getUniform("unWavyMult"), sDebugOptions.mTerrainWavyColorMult);
    glUniform1f(mTerrainMaterial->mProgram.getUniform("unSquaresPeriod"), sDebugOptions.mTerrainSquaresColorPeriod);
    glUniform1f(mTerrainMaterial->mProgram.getUniform("unSquaresIntensity"), sDebugOptions.mTerrainSquaresIntensity);
    glUniform1f(mTerrainMaterial->mProgram.getUniform("unBlendMult"), sDebugOptions.mTerrainBlendMult);

    // TODO: Where is this getting unset?
    glEnable(GL_CULL_FACE);
    for (auto&& terrainQuadtree : terrainQuadtrees) {
        terrainQuadtree.renderTerrain(camera, mTerrainMaterial->mProgram);
    }
}

void TerrainRenderer::renderWater(const Camera3D& camera, const std::vector<HeightmapTerrainQuadtree>& terrainQuadtrees)
{
    glDisable(GL_CULL_FACE);
    vg::DepthState::READ.set();
    mMaterialRenderer.bindMaterialForRender(*mWaterMaterial);
    // TODO: UBO?
    // Water uniforms
    glUniform4fv(mWaterMaterial->mProgram.getUniform("unShallowColor"), 1, &sDebugOptions.mShallowWaterColor.x);
    glUniform4fv(mWaterMaterial->mProgram.getUniform("unDeepColor"), 1, &sDebugOptions.mDeepWaterColor.x);
    glUniform4fv(mWaterMaterial->mProgram.getUniform("unFoamColor"), 1, &sDebugOptions.mWaterFoamColor.x);
    glUniform1f(mWaterMaterial->mProgram.getUniform("unSurfaceDistortAmount"), sDebugOptions.mWaterSurfaceDistortAmount);
    glUniform1f(mWaterMaterial->mProgram.getUniform("unSurfaceMoveSpeed"), sDebugOptions.mWaterSurfaceMoveSpeed);
    glUniform2fv(mWaterMaterial->mProgram.getUniform("unFoamDistanceRange"), 1, &sDebugOptions.mWaterFoamDistanceRange.x);
    glUniform1f(mWaterMaterial->mProgram.getUniform("unSurfaceNoiseCutoff"), sDebugOptions.mWaterSurfaceNoiseCutoff);
    glUniform1f(mWaterMaterial->mProgram.getUniform("unSmoothstepAA"), sDebugOptions.mWaterSmoothstepAA);
    glUniform1f(mWaterMaterial->mProgram.getUniform("unColorNoiseIntensity"), sDebugOptions.mWaterColorNoiseIntensity);
    glUniform1f(mWaterMaterial->mProgram.getUniform("unDistortTiling"), sDebugOptions.mWaterDistortTiling);
    glUniform1f(mWaterMaterial->mProgram.getUniform("unNoiseTiling"), sDebugOptions.mWaterNoiseTiling);
    MaterialUtils::uploadLightingUniforms(*mWaterMaterial);
    for (auto&& terrainQuadtree : terrainQuadtrees) {
        terrainQuadtree.renderWater(camera, mWaterMaterial->mProgram);
    }
}
