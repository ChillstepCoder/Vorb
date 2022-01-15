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

BuildingRenderer::BuildingRenderer(ResourceManager& resourceManager, const MaterialRenderer& materialRenderer) :
    mResourceManager(resourceManager),
    mMaterialRenderer(materialRenderer)
{
    mRoofMaterial = mResourceManager.getMaterialManager().getMaterial("standard_roof");
    mRoofBaseMaterial = mResourceManager.getMaterialManager().getMaterial("standard_tile");
    mRoofShadowMaterial = mResourceManager.getMaterialManager().getMaterial("shadow_mapper");
    mMesher = std::make_unique<BuildingMesher>(resourceManager);
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

    if (building.mRenderData.mRoofMeshDirty) {
        mMesher->buildRoofMesh(building);
    }

    // TODO: Redundant binds here
    // bindMaterialForRender(material, nullptr);
    // mesh.draw(material.mProgram);
    // TODO: Fix invalid meshes
    if (building.mRenderData.mRoofTriangleMesh->isValid()) {
        mMaterialRenderer.renderMesh(*building.mRenderData.mRoofTriangleMesh, *mRoofMaterial);
    }
    if (building.mRenderData.mRoofMesh->isValid()) {
        mMaterialRenderer.renderMesh(*building.mRenderData.mRoofMesh, *mRoofBaseMaterial);
    }
}

void BuildingRenderer::renderBuildingRoofShadows(const Building& building) {
    // TODO: Fix invalid meshes
    if (building.mRenderData.mRoofTriangleMesh->isValid()) {
        mMaterialRenderer.renderMesh(*building.mRenderData.mRoofTriangleMesh, *mRoofShadowMaterial);
    }
}