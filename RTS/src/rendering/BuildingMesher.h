#pragma once

#include "city/Building.h"

class ResourceManager;
class Building;


constexpr ui32 ROOF_VERTEX_CORNER_TABLE_SIZE = 16; // 4^2

class BuildingMesher
{
public:
    BuildingMesher();

    void buildRoofMesh(const Building& building);

    void addRoofTriangle(
        const f32v2 points[3],
        f32v2& start,
        const Building& building,
        const SpriteData& spriteData,
        ui32 debugColorIndex,
        BuildingMesh& buildingMesh
    );

private:
    Cartesian mCornerNextEdgeLookupTable[ROOF_VERTEX_CORNER_TABLE_SIZE];
    CornerWinding mCornerTypeLookupTable[ROOF_VERTEX_CORNER_TABLE_SIZE];
};

