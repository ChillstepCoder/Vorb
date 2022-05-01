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

void BuildingRenderer::renderBuildingRoof(const Building& building)
{

    if (sDebugOptions.mRoofDebug) {
        DebugRenderer::drawWireQuad(f32v3(building.mAABB.x, building.mAABB.y, building.mZPosFloor), f32v2(building.mAABB.dims), color4(1.0f, 0.0f, 0.0f, 1.0f));
        DebugRenderer::drawWireQuad(f32v3(building.mAABB.x, building.mAABB.y, building.mZPosRoof), f32v2(building.mAABB.dims), color4(1.0f, 0.0f, 0.0f, 1.0f));
    }

    if (building.mRenderData.mMeshDirty) {
        mMesher->buildRoofMesh(building);
    }

    // TODO: Redundant binds here
    // bindMaterialForRender(material, nullptr);
    // mesh.draw(material.mProgram);
    // TODO: Fix invalid meshes
    if (building.mRenderData.mMesh->isValid()) {
        mMaterialRenderer.renderMesh(*building.mRenderData.mMesh, *mRoofMaterial);
    }
    //if (building.mRenderData.mRoofMesh->isValid()) {
    //    mMaterialRenderer.renderMesh(*building.mRenderData.mRoofMesh, *mRoofBaseMaterial);
    //}
}

void BuildingRenderer::renderBuildingShadows(const Building& building) {
    // TODO: Fix invalid meshes
    if (building.mRenderData.mMesh->isValid()) {
        mMaterialRenderer.renderMesh(*building.mRenderData.mMesh, *mRoofShadowMaterial);
    }
}