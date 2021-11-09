#include "stdafx.h"
#include "BuildingRenderer.h"

#include "rendering/QuadMesh.h"
#include "rendering/TriangleMesh.h"


#include "city/Building.h"
#include "rendering/MaterialManager.h"
#include "rendering/MaterialRenderer.h"
#include "rendering/BuildingMesher.h"
#include "ResourceManager.h"

BuildingRenderer::BuildingRenderer(ResourceManager& resourceManager, const MaterialRenderer& materialRenderer) :
    mResourceManager(resourceManager),
    mMaterialRenderer(materialRenderer)
{
    mRoofMaterial = mResourceManager.getMaterialManager().getMaterial("standard_roof");
    mRoofBaseMaterial = mResourceManager.getMaterialManager().getMaterial("standard_tile");
    mMesher = std::make_unique<BuildingMesher>(resourceManager);
}

BuildingRenderer::~BuildingRenderer()
{

}

void BuildingRenderer::renderBuildingRoof(const Building& building)
{
    if (building.mRenderData.mRoofMeshDirty) {
        mMesher->buildRoofMesh(building);
    }

    // TODO: Redundant binds here
    // bindMaterialForRender(material, nullptr);
    // mesh.draw(material.mProgram);

    mMaterialRenderer.renderMesh(*building.mRenderData.mRoofTriangleMesh, *mRoofMaterial);
    mMaterialRenderer.renderMesh(*building.mRenderData.mRoofMesh, *mRoofBaseMaterial);
}
