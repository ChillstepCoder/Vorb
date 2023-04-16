#include "stdafx.h"
#include "GrassRenderer.h"

#include "rendering/GrassBillboardMesh.h"
#include "rendering/ChunkGrassQuadtree.h"
#include "resources/ResourceManager.h"
#include "rendering/MaterialShaderManager.h"
#include "rendering/MaterialRenderer.h"

#include "options/DebugOptions.h"
#include "camera/Camera3D.h"

GrassRenderer::GrassRenderer()
{
    const MaterialShaderManager& materialManager = Services::ResourceManager::ref().getMaterialShaderManager();
    mGrassMaterial = materialManager.getMaterialShader("grass");
}

void GrassRenderer::renderGrass(const Camera3D& camera, const f32v3& playerPos, const std::set<const GrassMesh*>& grassMeshes) {

    MaterialRenderer::bindMaterialForRender(*mGrassMaterial);
    const vg::GLProgram& program = mGrassMaterial->mProgram;
    VGUniform offsetUniform = program.getUniform("unOffset");
    VGUniform crossfadeAlphaUniform = program.getUniform("unCrossfadeAlpha");
    VGUniform crossfadeDirectionUniform = program.getUniform("unCrossfadeDirection");
    VGUniform tboSizeTypeUniform = program.getUniform("UnTboSizeType");
    VGUniform tboPositionUniform = program.getUniform("UnTboPosition");
    glUniform3fv(program.getUniform("unPlayerPos"), 1, &playerPos.x);
    glUniform1f(program.getUniform("unFadeDistance"), sDebugOptions.mGrassSettings.fadeDistance);
    glUniform2f(program.getUniform("unGrassScale"), sDebugOptions.mGrassScale.x, sDebugOptions.mGrassScale.y);
    glUniform1f(program.getUniform("unLeanVariance"), sDebugOptions.mGrassLeanVariance);
    glUniform1f(program.getUniform("unDitherPower"), sDebugOptions.mGrassDitherPower);
    for (auto&& grassMesh : grassMeshes) {
        const GrassBillboardMesh& mesh = grassMesh->mMesh;
        f32v3 offset = grassMesh->mPosition - camera.getPosition();
        glUniform3fv(offsetUniform, 1, &offset.x);

        ui32 lod = QUADTREE_LOD_FROM_INDEX[grassMesh->mIndex];
        f32v2 centerPos = f32v2(ChunkGrassQuadtree::PATCH_POSITIONS.data[grassMesh->mIndex].xy) + f32v2(ChunkGrassQuadtree::LOD_HALF_DIMS[lod].xy);
        f32v3 centerPos3d(centerPos.x, centerPos.y, 0.0f);
        int crossfadeDir = grassMesh->mCrossfadeDir.load();
        if (crossfadeDir != 0) {
            glUniform1f(crossfadeAlphaUniform, grassMesh->mCrossfadeAlpha.load() * 0.5f /* Constant that was selected via trial and error*/);
            glUniform1f(crossfadeDirectionUniform, (crossfadeDir > 0) ? 1.0f : 0.0f);
        }
        else {
            glUniform1f(crossfadeAlphaUniform, 0.0f);
            glUniform1f(crossfadeDirectionUniform, 0.0f);
        }
        if (camera.sphereIsVisible(mesh.getBoundingSphere())) {
            mesh.draw(tboSizeTypeUniform, tboPositionUniform);
        }
    };
}
