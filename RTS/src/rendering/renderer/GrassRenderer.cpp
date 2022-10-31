#include "stdafx.h"
#include "GrassRenderer.h"

#include "rendering/ChunkGrassQuadtree.h"
#include "resources/ResourceManager.h"
#include "rendering/MaterialManager.h"
#include "rendering/MaterialRenderer.h"

#include "options/DebugOptions.h"
#include "camera/Camera3D.h"

GrassRenderer::GrassRenderer()
{
    const MaterialManager& materialManager = Services::ResourceManager::ref().getMaterialManager();
    mGrassMaterial = materialManager.getMaterial("grass");
}

void GrassRenderer::renderGrass(const std::set<const ChunkGrassQuadtree*>& grassQuadtrees, const Camera3D& camera, const f32v3& playerPos) {

    MaterialRenderer::bindMaterialForRender(*mGrassMaterial);
    VGUniform offsetUniform = mGrassMaterial->mProgram.getUniform("unOffset");
    VGUniform fadeUniform = mGrassMaterial->mProgram.getUniform("unFadeDistance");
    glUniform3fv(mGrassMaterial->mProgram.getUniform("unPlayerPos"), 1, &playerPos.x);
    glUniform1f(fadeUniform, sDebugOptions.mGrassSettings.fadeDistance);
    for (auto&& quadTree : grassQuadtrees) {
        f32v3 offset = quadTree->getPosition() - camera.getPosition();
        glUniform3fv(offsetUniform, 1, &offset.x);
        quadTree->render(camera, mGrassMaterial);
    };
}
