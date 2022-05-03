#pragma once

#include "city/Building.h"

class ResourceManager;
class Building;
class MeshBuilder;


constexpr ui32 ROOF_VERTEX_CORNER_TABLE_SIZE = 16; // 4^2

class BuildingMesher
{
public:
    BuildingMesher();

    void buildRoofMesh(const Building& building);

    void addRoofTriangle(
        MeshBuilder& meshBuilder,
        const f32v2 points[3],
        const Building& building,
        const SubTexture& texture,
        ui32 debugColorIndex,
        Mesh& buildingMesh
    );

private:
    Cartesian mCornerNextEdgeLookupTable[ROOF_VERTEX_CORNER_TABLE_SIZE];
    CornerWinding mCornerTypeLookupTable[ROOF_VERTEX_CORNER_TABLE_SIZE];
};

