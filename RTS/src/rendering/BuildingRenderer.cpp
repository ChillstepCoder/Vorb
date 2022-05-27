#include "stdafx.h"
#include "BuildingRenderer.h"

#include "rendering/QuadMesh.h"
#include "rendering/TriangleMesh.h"

#include "options/DebugOptions.h"

#include "city/Building.h"
#include "rendering/MaterialManager.h"
#include "rendering/MaterialRenderer.h"
#include "rendering/BuildingMesher.h"
#include "ResourceManager.h"

#include "camera/Camera3D.h"

#include "DebugRenderer.h"

BuildingRenderer::BuildingRenderer(const MaterialRenderer& materialRenderer) :
    mMaterialRenderer(materialRenderer)
{
    const MaterialManager& materialManager = Services::ResourceManager::ref().getMaterialManager();
    mRoofMaterial = materialManager.getMaterial("standard_tile");
    mRoofBaseMaterial = materialManager.getMaterial("standard_tile");
    mRoofShadowMaterial = materialManager.getMaterial("shadow_mapper");
    mMesher = std::make_unique<BuildingMesher>();
}

BuildingRenderer::~BuildingRenderer()
{

}

void BuildingRenderer::renderBuildingRoof(const Building& building, const Camera3D& camera)
{

    if (sDebugOptions.mRoofDebug) {
        DebugRenderer::drawWireQuad(f32v3(building.mAABB.pos), f32v2(building.mAABB.dims), color4(1.0f, 0.0f, 0.0f, 1.0f));
    }

    if (building.mRenderData.mMeshDirty) {
        mMesher->buildMesh(building);
    }

    // TODO: Redundant binds here
    // bindMaterialForRender(material, nullptr);
    // mesh.draw(material.mProgram);
    // TODO: Fix invalid meshes
    if (building.mRenderData.mMesh->isValid()) {
        mMaterialRenderer.bindMaterialForRender(*mRoofMaterial);
        f32v3 offset = f32v3(building.mAABB.x, building.mAABB.y, 0.0f) - camera.getPosition();
        glUniform3fv(mRoofMaterial->getUniform("unOffset"), 1, &offset.x);
        building.mRenderData.mMesh->draw();
    }
    //if (building.mRenderData.mRoofMesh->isValid()) {
    //    mMaterialRenderer.renderMesh(*building.mRenderData.mRoofMesh, *mRoofBaseMaterial);
    //}
}

void BuildingRenderer::renderBuildingShadows(const Building& building, const Camera3D& camera) {
    // TODO: Fix invalid meshes
    if (building.mRenderData.mMesh->isValid()) {
        mMaterialRenderer.bindMaterialForRender(*mRoofShadowMaterial);
        f32v3 offset = f32v3(building.mAABB.x, building.mAABB.y, 0.0f) - camera.getPosition();
        glUniform3fv(mRoofShadowMaterial->getUniform("unOffset"), 1, &offset.x);
        building.mRenderData.mMesh->draw();
    }
}