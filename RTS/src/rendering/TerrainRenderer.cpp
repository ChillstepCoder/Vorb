#include "stdafx.h"
#include "TerrainRenderer.h"

#include "rendering/MaterialRenderer.h"
#include "rendering/MaterialManager.h"
#include "rendering/MaterialUtils.h"

#include <Vorb/graphics/GBuffer.h>

#include "world/HeightmapTerrainQuadtree.h"

#include "resources/ResourceManager.h"
#include "camera/Camera3D.h"
#include "mesh/Mesh.h"

#include "options/DebugOptions.h"

TerrainRenderer::TerrainRenderer(const MaterialRenderer& materialRenderer) : mMaterialRenderer(materialRenderer)
{
    const MaterialManager& materialManager = Services::ResourceManager::ref().getMaterialManager();
    mTerrainMaterial = materialManager.getMaterial("terrain");
    mWaterMaterial = materialManager.getMaterial("water");
}

void TerrainRenderer::renderTerrain(const Camera3D& camera, const std::set<const TerrainMesh*>& terrainMeshes) {
    mMaterialRenderer.bindMaterialForRender(*mTerrainMaterial);
    // Terrain uniforms
    glUniform1f(mTerrainMaterial->mProgram.getUniform("unHeightMult"), sDebugOptions.mTerrainHeightColorMult);
    glUniform1f(mTerrainMaterial->mProgram.getUniform("unWavyMult"), sDebugOptions.mTerrainWavyColorMult);
    glUniform1f(mTerrainMaterial->mProgram.getUniform("unSquaresPeriod"), sDebugOptions.mTerrainSquaresColorPeriod);
    glUniform1f(mTerrainMaterial->mProgram.getUniform("unSquaresIntensity"), sDebugOptions.mTerrainSquaresIntensity);
    glUniform1f(mTerrainMaterial->mProgram.getUniform("unBlendMult"), sDebugOptions.mTerrainBlendMult);

    VGUniform crossfadeAlphaUniform = mTerrainMaterial->mProgram.getUniform("unCrossfadeAlpha");
    VGUniform crossfadeDirectionUniform = mTerrainMaterial->mProgram.getUniform("unCrossfadeDirection");
    VGUniform offsetUniform = mTerrainMaterial->mProgram.getUniform("unOffset");

    // TODO: Where is this getting unset?
    glEnable(GL_CULL_FACE);
    //for (auto&& terrainQuadtree : terrainQuadtrees) {
    //    terrainQuadtree.renderTerrain(camera, mTerrainMaterial->mProgram);
    //}

    for (auto&& terrainMesh : terrainMeshes) {
        const Mesh& mesh = terrainMesh->mMesh;
        f32v3 offset = mesh.getPosition() - camera.getPosition();
        glUniform3fv(offsetUniform, 1, &offset.x);

        ui32 lod = QUADTREE_LOD_FROM_INDEX[terrainMesh->mIndex];
        f32v2 centerPos = f32v2(HeightmapTerrainQuadtree::PATCH_POSITIONS.data[terrainMesh->mIndex].xy) + f32v2(HeightmapTerrainQuadtree::LOD_HALF_DIMS[lod].xy);
        f32v3 centerPos3d(centerPos.x, centerPos.y, 0.0f);
        int crossfadeDir = terrainMesh->mCrossfadeDir.load();
        if (crossfadeDir != 0) {
            glUniform1f(crossfadeAlphaUniform, terrainMesh->mCrossfadeAlpha.load() * 0.5f /* Constant that was selected via trial and error*/);
            glUniform1f(crossfadeDirectionUniform, (crossfadeDir > 0) ? 1.0f : 0.0f);
        }
        else {
            glUniform1f(crossfadeAlphaUniform, 0.0f);
            glUniform1f(crossfadeDirectionUniform, 0.0f);
        }
        const BoundingSphere& bounds = mesh.getBoundingSphere();
        if (camera.sphereIsVisible(bounds.center, bounds.radius)) {
            mesh.draw();
        }
    }
}

void TerrainRenderer::renderWater(const Camera3D& camera, const std::set<const TerrainMesh*>& waterMeshes)
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
    /* for (auto&& terrainQuadtree : terrainQuadtrees) {
         terrainQuadtree.renderWater(camera, mWaterMaterial->mProgram);
     }*/
}
