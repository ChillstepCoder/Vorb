#pragma once

#include "city/Building.h"

class ResourceManager;
class Building;


constexpr ui32 ROOF_VERTEX_CORNER_TABLE_SIZE = 16; // 4^2

class BuildingMesher
{
public:
    BuildingMesher(ResourceManager& resourceManager);

    void buildRoofMesh(const Building& building);

private:
    ResourceManager& mResourceManager;
    Cartesian mCornerNextEdgeLookupTable[ROOF_VERTEX_CORNER_TABLE_SIZE];
    CornerWinding mCornerTypeLookupTable[ROOF_VERTEX_CORNER_TABLE_SIZE];
};

