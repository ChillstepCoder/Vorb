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

class VisualLog;
class InstancedStaticModelGatherer;

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
class ProceduralMeshBuilder;
class BillboardMeshBuilder;
class PhysicsWorld;

struct RoofContourEdgeInfo {
    f32v3 v1;
    f32v3 parent1;
    f32v3 v2;
    f32v3 parent2;
    Cartesian dir;
};

class BuildingMesher
{
public:
    static void buildMeshAndPhysicsAsync(const Building& building);

private:
    static void buildMeshAndPhysicsInternal(const Building& building, ProceduralMeshBuilder& staticMeshBuilder, ProceduralMeshBuilder& dynamicMeshBuilder, BillboardMeshBuilder& billboardMeshBuilder, InstancedStaticModelGatherer& modelGatherer);
    static std::vector<SsPtr> buildRoofStraightSkeletons(const BitArray& ownedTiles, const Building& building, f32 zPos, VisualLog* visLog);
    static void buildMeshFromStraightSkeleton(SsPtr iss, const Building& building, ProceduralMeshBuilder& meshBuilder, std::vector<RoofContourEdgeInfo>& contourEdges, const SubTexture& rawWoodTexture, const SubTexture& shinglesTexture, ui32 floor, f32 zPos, VisualLog* visLog);
    static void triangulateRoofFacePolygons(bool isGable, ProceduralMeshBuilder& meshBuilder, const Building& building, const SubTexture& shinglesTexture, ui32 debugColorIndex, f32 zPos);
    static void addRoofTriangle(
        ProceduralMeshBuilder& meshBuilder,
        const f32v2 points[3],
        const Building& building,
        const SubTexture& texture,
        ui32 debugColorIndex,
        f32 zPos
    );
    static void meshRoofContourEdges(const std::vector<RoofContourEdgeInfo>& contourEdges, const Building& building, ProceduralMeshBuilder& meshBuilder, const SubTexture& shinglesTexture, const SubTexture& rawWoodTexture, f32 zPos, VisualLog* visLog);
    static void meshRoomCeilings(const Building& building, ProceduralMeshBuilder& meshBuilder, const SubTexture& rawWoodTexture);
    static void meshRoomSupports(const Building& building, ProceduralMeshBuilder& meshBuilder, const SubTexture& rawWoodTexture);
};

