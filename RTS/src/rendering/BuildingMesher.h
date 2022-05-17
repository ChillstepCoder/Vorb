#pragma once

#include "city/Building.h"

//CGal
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Polygon_2.h>
#include <CGAL/create_straight_skeleton_2.h>
#include <boost/shared_ptr.hpp>
#include <CGAL/Triangulation_2.h>
#include <CGAL/partition_2.h>
#include <CGAL/Partition_traits_2.h>

typedef CGAL::Exact_predicates_inexact_constructions_kernel K;
typedef K::Point_2                   CgalPoint;
typedef CGAL::Polygon_2<K>           Polygon_2;
typedef CGAL::Straight_skeleton_2<K> StraightSkeleton;
typedef boost::shared_ptr<StraightSkeleton> SsPtr;
typedef CGAL::Triangulation_2<K>         Triangulation;
typedef Triangulation::Vertex_circulator Vertex_circulator;
typedef Triangulation::Point             TriangulationPoint;

class ResourceManager;
class Building;
class MeshBuilder;

struct RoofContourEdgeInfo {
    f32v3 v1;
    f32v3 parent1;
    f32v3 v2;
    f32v3 parent2;
};

constexpr ui32 ROOF_VERTEX_CORNER_TABLE_SIZE = 16; // 4^2

class BuildingMesher
{
public:
    BuildingMesher();

    void buildMesh(const Building& building);

private:
    void meshTiles(const Building& building, MeshBuilder& meshBuilder);
    void buildMeshFromStraightSkeleton(SsPtr iss, const Building& building, MeshBuilder& meshBuilder, std::vector<RoofContourEdgeInfo>& contourEdges, const SubTexture& rawWoodTexture, const SubTexture& shinglesTexture);
    void triangulateRoofFacePolygons(bool isGable, MeshBuilder& meshBuilder, const Building& building, const SubTexture& shinglesTexture, ui32 debugColorIndex);
    void addRoofTriangle(
        MeshBuilder& meshBuilder,
        const f32v2 points[3],
        const Building& building,
        const SubTexture& texture,
        ui32 debugColorIndex
    );
    void meshRoofContourEdges(const std::vector<RoofContourEdgeInfo>& contourEdges, const Building& building, MeshBuilder& meshBuilder, const SubTexture& shinglesTexture, const SubTexture& rawWoodTexture);

    Cartesian mCornerNextEdgeLookupTable[ROOF_VERTEX_CORNER_TABLE_SIZE];
    CornerWinding mCornerTypeLookupTable[ROOF_VERTEX_CORNER_TABLE_SIZE];
};

