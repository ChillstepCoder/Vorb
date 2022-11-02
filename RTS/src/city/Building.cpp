#include "stdafx.h"
#include "Building.h"

#include "rendering/mesh/Mesh.h"

BuildingRenderData::~BuildingRenderData() {

}

Building::Building() : mNavGraph(*this)
{

}

Building::~Building() {
    if (mRenderData.mMesh && !IS_SHUTTING_DOWN) {

    }
}

void Building::updateNavGraph() {
    mNavGraph.update();
}
