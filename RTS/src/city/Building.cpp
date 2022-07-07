#include "stdafx.h"
#include "Building.h"

#include "rendering/mesh/Mesh.h"

BuildingRenderData::~BuildingRenderData() {

}

Building::Building() : mNavGraph(*this)
{

}

Building::~Building() {

}

void Building::updateNavGraph() {
    mNavGraph.update();
}
