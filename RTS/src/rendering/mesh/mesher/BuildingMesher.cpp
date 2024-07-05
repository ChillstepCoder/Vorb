#include "stdafx.h"
#include "BuildingMesher.h"

#include "building/building.h"
#include "resources/ResourceManager.h"
#include "resources/MaterialRepository.h"

#include "debugging/DebugRenderer.h"
#include "debugging/VisualLogger.h"

#include "util/IntersectionUtil.h"

#include "rendering/mesh/Mesh.h"
#include "rendering/mesh/mesher/builder/ContainerMeshBuilders.h"

#include "math/Random.h"

#include "tile/TileHandle.h"
#include "resources/TileRepository.h"
#include "rendering/mesh/mesher/builder/TileMeshBuilderMethods.h"

#include "util/GridEdgeFinder.h"

#include "physics/StaticPhysicsMeshBuilder.h"

//CGal
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Polygon_2.h>
#include <CGAL/create_straight_skeleton_2.h>
#include <CGAL/Triangulation_2.h>
#include <CGAL/partition_2.h>
#include <CGAL/Partition_traits_2.h>

#include <boost/container/flat_set.hpp>

typedef CGAL::Exact_predicates_inexact_constructions_kernel K;
typedef K::Point_2                   CgalPoint;
typedef CGAL::Polygon_2<K>           Polygon_2;
typedef CGAL::Straight_skeleton_2<K> StraightSkeleton;
typedef boost::shared_ptr<StraightSkeleton> SsPtr;
typedef CGAL::Triangulation_2<K>         Triangulation;
typedef Triangulation::Vertex_circulator Vertex_circulator;
typedef Triangulation::Point             TriangulationPoint;

constexpr f32 ROOF_THICKNESS = 0.04f;
constexpr f32 TRIM_BOARD_HALF_THICKNESS = 0.06f;
constexpr f32 ROOF_EXTRUDE_DISTANCE = 0.95f;
constexpr f32 ROOF_EXTRUDE_DISTANCE_CLIPPING_REDUCE_MULT = 0.35f; // How much we reduce by if our extrude position is clipping with another
constexpr f32 ROOF_HEIGHT_MULT = 0.5f; // 0.3
constexpr int BUILDING_DEBUG_LIFETIME = 50000;
constexpr ui32 DEBUG_COLOR_ARRAY_SIZE = 12;
color4 DEBUG_COLOR_ARRAY[DEBUG_COLOR_ARRAY_SIZE] = {
    COLOR_WHITE,
    color4(1.0f, 0.0f, 0.0f),
    color4(0.0f, 1.0f, 0.0f),
    color4(0.0f, 0.0f, 1.0f),
    color4(1.0f, 1.0f, 0.0f),
    color4(0.0f, 1.0f, 1.0f),
    color4(1.0f, 0.0f, 1.0f),
    color4(0.5f, 0.1f, 1.0f),
    color4(0.1f, 0.7f, 0.5f),
    color4(0.7f, 0.5f, 0.1f),
    color4(0.5f, 0.5f, 0.5f),
    color4(0.1f, 0.1f, 0.1f),
};
struct RoofStyle {
    const MaterialDesc& shinglesMaterial;
    const MaterialDesc& primaryBoardMaterial;
};

// Helper forward declare
std::vector<SsPtr> buildRoofStraightSkeletons(const BitArray& floorRoofedTiles, const TileSpatialGrid& spatialGrid, f32 zPos, VisualLog* visLog);
void buildMeshFromStraightSkeleton(const BitArray& floorRoofedTiles, SsPtr iss, const TileSpatialGrid& spatialGrid, ProceduralMeshBuilder& meshBuilder, std::vector<RoofContourEdgeInfo>& contourEdges, const RoofStyle& roofStyle, ui32 floor, f32 zPos, VisualLog* visLog);
void triangulateRoofFacePolygons(ProceduralMeshBuilder& meshBuilder, const RoofStyle& roofStyle, ui32 debugColorIndex, f32 zPos, VisualLog* visLog);
void addRoofTriangle(
    ProceduralMeshBuilder& meshBuilder,
    const f32v2 points[3],
    const MaterialDesc& materialData,
    f32 zPos
);
void meshRoofContourEdges(const std::vector<RoofContourEdgeInfo>& contourEdges, ProceduralMeshBuilder& meshBuilder, const RoofStyle& roofStyle, f32 zPos, VisualLog* visLog);
void meshRoomCeilings(ContainerMeshBuilders& meshBuilders, const RoofStyle& roofStyle);
void meshRoomUndercarriage(ContainerMeshBuilders& meshBuilders, const RoofStyle& roofStyle);

//class f32v2HashFunction {
//public:
//    size_t operator()(const f32v2& f) const
//    {
//        return (size_t)std::hash<f32>{}(f.x) ^ (size_t)std::hash<f32>{}(f.y * 127.0f);
//    }
//};

thread_local UnorderedFlatMap<f32v2, f32> sHeightMap;
thread_local std::vector<TriangulationPoint> sRoofFacePoints;
thread_local Polygon_2 sCgalPoly;

struct RoofSkeletonVertex;

ui32v2 AXIS_UV_LOOKUP_FROM_CARTESIAN[4] = {
    ui32v2(AXIS_X, AXIS_Y), // Cartesian::DOWN
    ui32v2(AXIS_Y, AXIS_X), // Cartesian::LEFT
    ui32v2(AXIS_Y, AXIS_X), // Cartesian::RIGHT
    ui32v2(AXIS_X, AXIS_Y), // Cartesian::UP
};
f32 AXIS_V_DIR_FROM_CARTESIAN[4] = {
    1.0f, // Cartesian::DOWN
    1.0f, // Cartesian::LEFT
    -1.0f, // Cartesian::RIGHT
    -1.0f, // Cartesian::UP
};

//http://wscg.zcu.cz/wscg2003/Papers_2003/G67.pdf
// Step 1: Construct the Straight Skeleton http://citeseerx.ist.psu.edu/viewdoc/download;jsessionid=0A7158816778A842AE8ABC0A752CD92D?doi=10.1.1.131.7175&rep=rep1&type=pdf
// Step 2: Determine the distance, d, each vertex is from its supporting edge.
// Step 3 : Perform a boundary walk, using the least interior angle, to determine the roof planes.
// Step 4 : Raise the vertices according to their distance from the supporting edge.

struct GableVertexInfo {
    f32v2 pos;
    ui32 borderCount; // If this is ever > 1, we ignore
    Cartesian gableDir = Cartesian::INVALID;

    bool isValidGable() const { return borderCount == 1; }
};

struct ContourVertexInfo {
    f32v3 extrudePos = f32v3(0.0f);
    f32v3 extrudeOffset = f32v3(0.0f); // TODO: UNUSED
    Cartesian gableDir = Cartesian::INVALID;
};

bool collideExtrudeWalls(const i32v2& start, const i32v2& end, ui32 axis, const TileSpatialGrid& spatialGrid, const ui32 floorIndex, f32 zPos, VisualLog* visLog) {
    const i32v2 dims = spatialGrid.getDims();
    // If we are out of the AABB, its a collide
    if (start[!axis] < 0 || start[!axis] >= dims[!axis]) {
        return true;
    }
    else {
        // Loop along our wall and check for collisions with tiles (or out of AABB)
        if (start[axis] < end[axis]) {
            for (int i = start[axis]; i <= end[axis]; ++i) {
                if (i < 0 || i >= dims[axis]) {
                    return true;
                }
                ui32 bitIndex;
                if (axis == 0) {
                    bitIndex = floorIndex + start.y * dims.x + i;
                    if (visLog) visLog->addWireQuad(f32v3(i, start.y, zPos), f32v2(1.0f), color4(1.0f, 1.0f, 0.0f, 1.0f));
                }
                else {
                    bitIndex = floorIndex + i * dims.x + start.x;
                    if (visLog) visLog->addWireQuad(f32v3(start.x, i, zPos), f32v2(1.0f), color4(1.0f, 1.0f, 0.0f, 1.0f));
                }
                /* if (building.getInteriorTilesInAABB().getBit(bitIndex)) {
                     return true;
                 }*/
            }
        }
        else {
            for (int i = start[axis]; i >= end[axis]; --i) {
                if (i < 0 || i >= dims[axis]) {
                    return true;
                }
                ui32 bitIndex;
                if (axis == 0) {
                    bitIndex = floorIndex + start.y * dims.x + i;
                    if (visLog) visLog->addWireQuad(f32v3(i, start.y, zPos), f32v2(1.0f), color4(1.0f, 1.0f, 0.0f, 1.0f));
                }
                else {
                    bitIndex = floorIndex + i * dims.x + start.x;
                    if (visLog) visLog->addWireQuad(f32v3(start.x, i, zPos), f32v2(1.0f), color4(1.0f, 1.0f, 0.0f, 1.0f));
                }
                /* if (building.getInteriorTilesInAABB().getBit(bitIndex)) {
                     return true;
                 }*/
            }
        }
    }
    return false;
}

void computeGablePointsAndExtrudePositions(const BitArray& floorOwnedTiles, const TileSpatialGrid& spatialGrid, ui32 floor, f32 zPos, UnorderedFlatMap<f32v2, GableVertexInfo>& gableVertexInfo, UnorderedFlatMap<f32v2, ContourVertexInfo>& contourVertexInfo, SsPtr iss, VisualLog* visLog) {
    gableVertexInfo.reserve(10);
    contourVertexInfo.reserve(30);

    // Store every tile with 3 or more adjacent walls, those will have their extrusion distance reduced in order to stop clipping + non simple polygons
    boost::container::flat_set<TileIndex> reduceExtrudeTiles;
    for (i32 y = 0; y < spatialGrid.getDims().y; ++y) {
        for (i32 x = 0; x < spatialGrid.getDims().x; ++x) {
            TileIndex tileIndex = spatialGrid.getBaseTileIndexFromXYOffset(x, y);
            // Exterior tiles only
            if (!floorOwnedTiles.getBit(tileIndex)) {
                int numWalls = TileSpatialGridHelpers::countAdjacentSetOwnershipBits(spatialGrid, i32v3(x, y, 0), floorOwnedTiles);
                if (numWalls >= 3) {
                    if (visLog) {
                        if (numWalls >= 4) {
                            visLog->addFilledQuad(f32v3(x, y, zPos), f32v2(1.0f), color::Red);
                        }
                        else {

                            visLog->addFilledQuad(f32v3(x, y, zPos), f32v2(1.0f), color::Orange);
                        }
                    }
                    reduceExtrudeTiles.insert(tileIndex);
                }
            }
        }
    }
    
    // Compute gable target points
    for (auto&& it = iss->faces_begin(); it != iss->faces_end(); ++it) {
        auto&& he = it->halfedge();

        // Visual log
        if (visLog) {
            const auto& thisPoint = he->vertex()->point();
            const auto& oppositePoint = he->opposite()->vertex()->point();
            const auto& nextPoint = he->next()->vertex()->point();
            f32v3 p1(thisPoint.x(), thisPoint.y(), zPos + he->vertex()->time() * ROOF_HEIGHT_MULT);
            {
                f32v3 p2(oppositePoint.x(), oppositePoint.y(), zPos + he->opposite()->vertex()->time() * ROOF_HEIGHT_MULT);
                visLog->addLineBetweenPoints(p1, p2, color4(1.0f, 0.0f, 0.0f, 0.75f));
            }
            {
                f32v3 p2(nextPoint.x(), nextPoint.y(), zPos + he->next()->vertex()->time() * ROOF_HEIGHT_MULT);
                visLog->addLineBetweenPoints(p1, p2, color4(1.0f, 0.0f, 1.0f, 0.75f));
            }
        }

        do {
            const bool isGablePoint = he->is_bisector() && !he->is_inner_bisector() && he->next()->is_bisector() && !he->next()->is_inner_bisector();
            if (isGablePoint) {
                f32v2 gableTarget;
                const auto& thisPoint = he->vertex()->point();
                const auto& prevPoint = he->prev()->vertex()->point();
                const auto& nextPoint = he->next()->vertex()->point();
                gableTarget.x = (f32)(prevPoint.x() + nextPoint.x()) / 2.0f;
                gableTarget.y = (f32)(prevPoint.y() + nextPoint.y()) / 2.0f;
                const f32v2 lookup = f32v2(he->vertex()->point().x(), he->vertex()->point().y());
                auto&& it = gableVertexInfo.find(lookup);

                Cartesian gableDir;
                if (prevPoint.y() < thisPoint.y() && nextPoint.y() < thisPoint.y()) {
                    gableDir = Cartesian::SOUTH;
                } else if (prevPoint.x() < thisPoint.x() && nextPoint.x() < thisPoint.x()) {
                    gableDir = Cartesian::WEST;
                }
                else if (prevPoint.x() > thisPoint.x() && nextPoint.x() > thisPoint.x()) {
                    gableDir = Cartesian::EAST;
                }
                else {
                    gableDir = Cartesian::NORTH;
                }

                bool isValidGable = true;
                if (it == gableVertexInfo.end()) {
                    gableVertexInfo[lookup] = GableVertexInfo{ gableTarget, 1, gableDir };
                }
                else {
                    // TODO: Does this invalidate below?
                    ++it->second.borderCount;
                    isValidGable = false;
                }
                if (isValidGable) {
                    // Mark our adjacent contour corners as gable children
                    ContourVertexInfo& prevVertexInfo = contourVertexInfo[f32v2(prevPoint.x(), prevPoint.y())];
                    ContourVertexInfo& nextVertexInfo = contourVertexInfo[f32v2(nextPoint.x(), nextPoint.y())];
                    prevVertexInfo.gableDir = gableDir;
                    nextVertexInfo.gableDir = gableDir;
                }
                // Visual log
                if (visLog) {
                    f32v3 p1(thisPoint.x(), thisPoint.y(), zPos + he->vertex()->time() * ROOF_HEIGHT_MULT);
                    visLog->addWireQuad(p1 - f32v3(0.2f, 0.2f, 0.0f), f32v2(0.4f), color4(0.0f, 1.0f, 0.0f, 0.75f));
                }
            }
            else if (he->vertex()->is_contour() && he->is_bisector()) {

                // Contour corners that we will extrude along the bisector
                // Mark for later extrusions
                const auto& oppositePoint = he->opposite()->vertex()->point();
                const auto& thisPoint = he->vertex()->point();
                const float h = (f32)he->vertex()->time() * ROOF_HEIGHT_MULT;
                const f32v3 extrudePosition(thisPoint.x() - oppositePoint.x(), thisPoint.y() - oppositePoint.y(), h - he->opposite()->vertex()->time() * ROOF_HEIGHT_MULT);
                const f32v3 extrudeOffset = normalize(extrudePosition) * ROOF_EXTRUDE_DISTANCE;
                ContourVertexInfo& vertexInfo = contourVertexInfo[f32v2(thisPoint.x(), thisPoint.y())];
                vertexInfo.extrudePos = extrudeOffset + f32v3(thisPoint.x(), thisPoint.y(), 0.0f);
                vertexInfo.extrudeOffset = extrudeOffset;
                TileIndex exteriorTileIndex = spatialGrid.tryGetBaseTileIndexFromXYOffsetf(vertexInfo.extrudePos);
                if (exteriorTileIndex != INVALID_TILE_INDEX) {
                    if (reduceExtrudeTiles.find(exteriorTileIndex) != reduceExtrudeTiles.end()) {
                        vertexInfo.extrudeOffset = ROOF_EXTRUDE_DISTANCE_CLIPPING_REDUCE_MULT * extrudeOffset;
                        vertexInfo.extrudePos -= vertexInfo.extrudeOffset;
                    }
                }
            }
            he = he->next();
        } while (he != it->halfedge());
    }

    // Fixup extrude positions that may be colliding with walls on above floors
    const ui32 floorIndex = floor * spatialGrid.getDims().y * spatialGrid.getDims().x;
    for (auto&& it = iss->faces_begin(); it != iss->faces_end(); ++it) {
        auto&& he = it->halfedge();
        do {
            const auto& thisPoint = he->vertex()->point();
            auto&& it1 = contourVertexInfo.find(f32v2(thisPoint.x(), thisPoint.y()));
            if (it1 != contourVertexInfo.end()) {
                ContourVertexInfo& thisVertexInfo = it1->second;
                const auto& oppositePoint = he->opposite()->vertex()->point();
                auto&& it2 = contourVertexInfo.find(f32v2(oppositePoint.x(), oppositePoint.y()));
                if (it2 != contourVertexInfo.end()) {
                    ContourVertexInfo& oppositeVertexInfo = it2->second;
                    // Loop along the edge and check for collisions
                    const i32v2 start((int)glm::floor(thisVertexInfo.extrudePos.x), (int)glm::floor(thisVertexInfo.extrudePos.y));
                    const i32v2 end((int)glm::floor(oppositeVertexInfo.extrudePos.x), (int)glm::floor(oppositeVertexInfo.extrudePos.y));
                    // TODO: I dont know what this case means... but it happens. Vislog it?
                    if (start == end) continue;

                    // Make sure we are straight walls
                    assert(start.x != end.x || start.y != end.y);
                    //assert(!(start.x != end.x && start.y != end.y));
                    if (!(start.x != end.x && start.y != end.y)) {
                        // wtf is this lolol
                        continue;
                    }

                    // Determine which direction edge we are
                    // TODO: This could be simplified into functions where we pass the iteration dimension
                    if (start.x == end.x) {
                        // Y wall
                        if (collideExtrudeWalls(start, end, 1, spatialGrid, floorIndex, zPos, visLog)) {
                            thisVertexInfo.extrudePos.x = it1->first.x;
                            oppositeVertexInfo.extrudePos.x = it2->first.x;
                            thisVertexInfo.extrudePos.z = 0.0f;
                            oppositeVertexInfo.extrudePos.z = 0.0f;
                        }
                    }
                    else {
                        // X Wall
                        if (collideExtrudeWalls(start, end, 0, spatialGrid, floorIndex, zPos, visLog)) {
                            thisVertexInfo.extrudePos.y = it1->first.y;
                            oppositeVertexInfo.extrudePos.y = it2->first.y;
                            thisVertexInfo.extrudePos.z = 0.0f;
                            oppositeVertexInfo.extrudePos.z = 0.0f;
                        }
                    }
                }
            }
            he = he->next();
        } while (he != it->halfedge());
    }
}


void BuildingMesher::buildMeshAndPhysicsAsync(const Building& building) const {
    buildMeshAndPhysicsAsyncInternal(*building.getTileContainer(), nullptr, false, 10000 /*reserveCount*/, nullptr);
}

void BuildingMesher::addCustomMeshData(ContainerMeshBuilders& meshBuilders, StaticPhysicsMeshBuilder& physicsBuilder, const void* userData) const {
    // Build roof
    PROFILE_FUNCTION();

    const TileSpatialGrid& spatialGrid = meshBuilders.tileData.spatialGrid;

    const i32AABB3& aabb = spatialGrid.getAABB();
    // TODO: This is a race condition

    // Debug log
    VisualLog* visLog = VisualLogger::tryGetNewVisualLog("building", VisualLogCategory::Building, true);
    if (visLog) {
        visLog->setRootPos(spatialGrid.getWorldPos());
        visLog->nextStep("AABB");
        visLog->addWireQuad(f32v3(0.0f), aabb.dims, color4(1.0f, 0.0f, 0.0f, 0.9f));
    }

    // Materials
    RoofStyle roofStyle = RoofStyle{
        .shinglesMaterial=MaterialRepository::get().getMaterialDesc(CStrToken("roof")),
        .primaryBoardMaterial=MaterialRepository::get().getMaterialDesc(CStrToken("big_beam_0"))
    };
    meshBuilders.addMaterial(roofStyle.shinglesMaterial.id);
    meshBuilders.addMaterial(roofStyle.primaryBoardMaterial.id);

    // TODO: Not right
    sHeightMap.reserve(100);
    sRoofFacePoints.reserve(100);

    // ========================== Straight Skeleton ===============================
    const ui32 floorCount = spatialGrid.getDims().z;
    const ui32 floorTileCount = aabb.dims.y * aabb.dims.x;
    BitArray roofedTiles;
    roofedTiles.resize(aabb.dims.x * aabb.dims.y);
    for (ui32 floor = 0; floor < floorCount; ++floor) {
        const f32 zPos = (floor + 1.0f) * spatialGrid.getFloorHeight();
        // TODO: Replace bitarray with bool array
        roofedTiles.zeroAllBits();
        for (i32 y = 0; y < aabb.dims.y; ++y) {
            for (i32 x = 0; x < aabb.dims.x; ++x) {
                const ui32 floorBitIndex = y * aabb.dims.x + x;
                const TileIndex tileIndex = floor * floorTileCount + floorBitIndex;
                // If we own this tile, and above us is clear, we are a roofed tile
                if (meshBuilders.tileData.tiles[tileIndex].hasFlag(TileFlags::ROOFED) &&
                    (floor == floorCount - 1 || !meshBuilders.tileData.tiles[tileIndex + floorTileCount].hasFlag(TileFlags::ROOFED))) {
                    roofedTiles.setBitTo(floorBitIndex, true);
                }
            }
        }

        // Generate a list of straight skeletons
        if (visLog) visLog->nextStep("Detect roof edges " + std::to_string(floor));
        std::vector<SsPtr> iss = buildRoofStraightSkeletons(roofedTiles, spatialGrid, zPos, visLog);
        std::vector<RoofContourEdgeInfo> contourEdges;
        contourEdges.reserve(20);

        // Mesh each individual straight skeleton
        ui32 n = 0;
        for (auto& ss : iss) {

            if (visLog) visLog->nextStep("Skeleton " + std::to_string(floor) + " " + std::to_string(n));
            buildMeshFromStraightSkeleton(roofedTiles, ss, spatialGrid, meshBuilders.staticBuilder, contourEdges, roofStyle, floor, zPos, visLog);

            // ========================== Contours and extruded side boards ===============================
            if (visLog) visLog->nextStep("Contour " + std::to_string(floor) + " " + std::to_string(n));
            meshRoofContourEdges(contourEdges, meshBuilders.staticBuilder, roofStyle, zPos, visLog);
            contourEdges.clear();
        }
    }

    // ========================== Room Ceilings ===============================
    meshRoomCeilings(meshBuilders, roofStyle);

    // ========================== Room supports ===============================
    meshRoomUndercarriage(meshBuilders, roofStyle);


    physicsBuilder.setRootPos(spatialGrid.getWorldPos());

    if (visLog) visLog->finish();
}

std::vector<SsPtr> buildRoofStraightSkeletons(const BitArray& floorRoofedTiles, const TileSpatialGrid& spatialGrid, f32 zPos, VisualLog* visLog) {
    // Detect Edges

    const i32v2 dims(spatialGrid.getDims());
    const ui32 totalTiles = dims.x * dims.y;
    std::vector<SsPtr> skeletons;

    BitArray checkedTiles;
    // Mark all unowned tiles as "checked"
    checkedTiles.setNOT(floorRoofedTiles);
    i32v2 cornerPos(0, 0);
   
    constexpr int MAX_ROOF_VERTICES = 8192;
    static thread_local f32v2 sRoofVertices[MAX_ROOF_VERTICES];

    // Get multiple straight skeletons
    while (true) {
        ui32 numRoofVertices = 0;

        // Find first unchecked corner to start iteration
        const i32 startIndex = checkedTiles.getIndexOfFirstUnsetBit(cornerPos.y * dims.x + cornerPos.x);
        if (startIndex >= totalTiles) {
            return skeletons;
        }

        checkedTiles.setBitTo(startIndex, true);

        const i32 startX = cornerPos.x = startIndex % dims.x;
        const i32 startY = cornerPos.y = startIndex / dims.x;
        if (startY > dims.y) {
            // TODO: Text render
            LOG_CRITICAL("startY > dims.y error in buildRoofStraightSkeletons");
            if (visLog) {
                const f32v3 rootPos = f32v3(cornerPos.x, cornerPos.y, zPos);
                visLog->addWireQuad(rootPos, f32v2(1.0f), color::Black);
            }
            return skeletons;
        }
        Cartesian edge = Cartesian::SOUTH; // We are guaranteed theres always a bottom edge at this corner
        // If we do not have a free tile below, it means we are an interior tile on an already skeletoned segment, so continue
        if (cornerPos.y > 0 && floorRoofedTiles.getBit((cornerPos.y - 1) * dims.x + cornerPos.x)) {
            continue;
        }

        sRoofVertices[numRoofVertices++] = cornerPos;
        // First edge always goes right
        ++cornerPos.x;
        do {
            if (cornerPos.x < dims.x && cornerPos.y < dims.y) {
                checkedTiles.setBitTo(cornerPos.y * dims.x + cornerPos.x, true);
            }

            // Visual log
            if (visLog) {
                const f32v3 rootPos = f32v3(cornerPos.x, cornerPos.y, zPos);
                visLog->addWireQuad(rootPos, f32v2(1.0f), color4(1.0f, 1.0f, 1.0f, 0.75f));
                switch (edge) {
                    case Cartesian::SOUTH:
                        visLog->addText("S", rootPos, 0.25f, f32v2(0.0f, 0.5f), COLOR_WHITE);
                        break;
                    case Cartesian::WEST:
                        visLog->addText("W", rootPos, 0.25f, f32v2(0.0f, 0.5f), COLOR_WHITE);
                        break;
                    case Cartesian::EAST:
                        visLog->addText("E", rootPos, 0.25f, f32v2(0.0f, 0.5f), COLOR_WHITE);
                        break;
                    case Cartesian::NORTH:
                        visLog->addText("N", rootPos, 0.25f, f32v2(0.0f, 0.5f), COLOR_WHITE);
                        break;
                    case Cartesian::NONE:
                        visLog->addText("X", rootPos, 0.25f, f32v2(0.0f, 0.5f), COLOR_WHITE);
                        break;
                    case Cartesian::INVALID:
                        visLog->addText("!", rootPos, 0.25f, f32v2(0.0f, 0.5f), COLOR_WHITE);
                        break;
                    default:
                        break;
                }
            }

            GridCell4x4 gridCell4x4;
            gridCell4x4.constructFrom2DBitArray(floorRoofedTiles, cornerPos.x, cornerPos.y, dims.x, dims.y);

            if (!gridCell4x4.data) {
                if (visLog) {
                    const ui32v2& xy = spatialGrid.getTileXYOffset(startIndex);
                    visLog->addFilledQuad(f32v3(xy.x, xy.y, zPos), f32v2(1.0f), color::Black);
                    LOG_CRITICAL("gridCell4x4 error in roof generation");
                    //DebugBreak();
                }
                return skeletons;
            }
            const Cartesian nextEdge = GridEdgeFinder::getNextCCWEdgeWalkDirFromGrid4x4(gridCell4x4, edge, nullptr);
            
            if (nextEdge < Cartesian::NONE) {
                assert(edge != nextEdge);
                edge = nextEdge;
                // New vertex and connect previous
                assert(numRoofVertices < MAX_ROOF_VERTICES);
                sRoofVertices[numRoofVertices] = cornerPos;
                ++numRoofVertices;
            }
            else if (nextEdge == Cartesian::INVALID) {
                // Visual log
                if (visLog) {
                    const ui32v2& xy = spatialGrid.getTileXYOffset(startIndex);
                    visLog->addFilledQuad(f32v3(xy.x, xy.y, zPos), f32v2(1.0f), COLOR_RED);
                }
                return skeletons;
            }
            cornerPos += CARTESIAN_TANGENTS_CCW[e_cast(edge)];

        } while ((cornerPos.x != startX || cornerPos.y != startY)/* && edge != Cartesian::SOUTH*/);

        sCgalPoly.resize(numRoofVertices);
        for (ui32 i = 0; i < numRoofVertices; ++i) {
            sCgalPoly[i] = CgalPoint(sRoofVertices[i].x, sRoofVertices[i].y);
        }

        // Get the straight skeleton
        SsPtr newSS = CGAL::create_interior_straight_skeleton_2(sCgalPoly.vertices_begin(), sCgalPoly.vertices_end());
        if (newSS) {
            skeletons.emplace_back(std::move(newSS));
        }
    }
    return skeletons;
}


void buildMeshFromStraightSkeleton(const BitArray& floorRoofedTiles, SsPtr iss, const TileSpatialGrid& spatialGrid, ProceduralMeshBuilder& meshBuilder, std::vector<RoofContourEdgeInfo>& contourEdges, const RoofStyle& roofStyle, ui32 floor, f32 zPos, VisualLog* visLog) {
    // For bisector board placement
    UnorderedFlatSet<std::pair<f32v3, f32v3>> bisectorBoardPositions;
    bisectorBoardPositions.reserve(20);

    // ========================== Gables and Extrudes ===============================
    // Map gable and contour vertex points so we can move all connected verts
    UnorderedFlatMap<f32v2, GableVertexInfo> gableVertexInfo;
    UnorderedFlatMap<f32v2, ContourVertexInfo> contourVertexInfo;
    computeGablePointsAndExtrudePositions(floorRoofedTiles, spatialGrid, floor, zPos, gableVertexInfo, contourVertexInfo, iss, visLog);


    if (visLog) {
        visLog->nextStep("Position verts + triangulate");
    }

    // Gather and reposition verts
    ui32 debugColorIndex = 0;
    for (auto&& it = iss->faces_begin(); it != iss->faces_end(); ++it) {
        auto&& he = it->halfedge();
        sHeightMap.clear();
        sRoofFacePoints.clear();
        f32v3 gablePosition = f32v3(0.0f);
        bool isGable = false;
        // Loop through every edge of the SS poly and mark gables + cache points
        do {
            // Detect gable points
            auto&& gableIt = gableVertexInfo.find(f32v2(he->vertex()->point().x(), he->vertex()->point().y()));
            const bool isGablePoint = gableIt != gableVertexInfo.end();
            const bool isContourEdge = he->vertex()->is_contour() && he->next()->vertex()->is_contour();

            f32 x, y, t, h;
            if (isGablePoint && gableIt->second.isValidGable()) {
                // We are a gable pivot! Get our new position
                x = gableIt->second.pos.x;
                y = gableIt->second.pos.y;
                t = (f32)he->vertex()->time();
                h = t * ROOF_HEIGHT_MULT;
                isGable = he->is_bisector() && !he->is_inner_bisector() && he->next()->is_bisector() && !he->next()->is_inner_bisector();
                // Extrude along the gable direction by the wall thickness
                f32v3 extrudeNormal(x - he->vertex()->point().x(), y - he->vertex()->point().y(), h * 3.0f); // 3.0f is trial and error
                extrudeNormal = normalize(extrudeNormal);
                x += extrudeNormal.x * WALL_HALF_THICKNESS;
                y += extrudeNormal.y * WALL_HALF_THICKNESS;
                gablePosition = f32v3(x, y, h);
            }
            else {
                x = (f32)he->vertex()->point().x();
                y = (f32)he->vertex()->point().y();
                t = (f32)he->vertex()->time();
                h = t * ROOF_HEIGHT_MULT;

                // Extrude contours
                if (he->vertex()->is_contour()) {
                    assert(t == 0.0f);
                    const f32 baseX = x;
                    const f32 baseY = y;
                    auto&& extrudeIt = contourVertexInfo.find(f32v2(baseX, baseY));
                    if (extrudeIt != contourVertexInfo.end()) {
                        // Create a column
                        // TODO: This column will intersect lower floors! Make it smarter
                        const f32v3 boardStart(x, y, -0.2f);
                        const f32v3 boardEnd(x, y, zPos);
                        meshBuilder.addBoardBetweenPoints(boardStart, boardEnd, f32v2(0.11f), roofStyle.primaryBoardMaterial, f32v2(1.0f));
                        // Visual log
                        if (visLog) {
                            visLog->addLineBetweenPoints(boardStart, boardEnd, color4(0.0f, 1.0f, 1.0f, 1.0f));
                        }

                        ContourVertexInfo& vertexInfo = extrudeIt->second;
                        f32v3& extrudePosition = vertexInfo.extrudePos;
                        // Gables should not extrude diagonally, they should stay in line with their wall
                        switch (vertexInfo.gableDir) {
                            case Cartesian::SOUTH:
                                extrudePosition.y = baseY - WALL_HALF_THICKNESS;
                                break;
                            case Cartesian::WEST:
                                extrudePosition.x = baseX - WALL_HALF_THICKNESS;
                                break;
                            case Cartesian::EAST:
                                extrudePosition.x = baseX + WALL_HALF_THICKNESS;
                                break;
                            case Cartesian::NORTH:
                                extrudePosition.y = baseY + WALL_HALF_THICKNESS;
                                break;
                        }

                        x = extrudePosition.x;
                        y = extrudePosition.y;
                        h = extrudePosition.z;
                    }
                }
            }

            // Skeleton boards (non edge)
            if (!isContourEdge) {
                const f32v3 boardStart(x, y, zPos + h + ROOF_THICKNESS);
                // Check if next point is extruded
                f32 nextX = (f32)he->next()->vertex()->point().x();
                f32 nextY = (f32)he->next()->vertex()->point().y();
                f32 nextH = (f32)he->next()->vertex()->time() * ROOF_HEIGHT_MULT;
                auto&& extrudeIt = contourVertexInfo.find(f32v2(nextX, nextY));
                if (extrudeIt != contourVertexInfo.end()) {
                    const f32v3& extrudePosition = extrudeIt->second.extrudePos;
                    nextX = extrudePosition.x;
                    nextY = extrudePosition.y;
                    nextH = extrudePosition.z;
                }

                const f32v3 boardEnd(nextX, nextY, zPos + nextH + ROOF_THICKNESS);

                // Make sure we dont double add
                if (bisectorBoardPositions.find(std::make_pair(boardStart, boardEnd)) == bisectorBoardPositions.end() &&
                    bisectorBoardPositions.find(std::make_pair(boardEnd, boardStart)) == bisectorBoardPositions.end()) {
                    bisectorBoardPositions.insert(std::make_pair(boardStart, boardEnd));
                    constexpr f32 BOARD_SIZE_VARIANCE = 0.03f;
                    // Random size offset
                    const f32v2 halfDims = f32v2(
                        0.1f + (randFromf32v3(boardStart - boardEnd, (ui64)&it /*hax*/) - 0.5f) * BOARD_SIZE_VARIANCE
                    );
                    meshBuilder.addBoardBetweenPoints(boardStart, boardEnd, halfDims, roofStyle.primaryBoardMaterial, f32v2(1.0f));
                    // Visual log
                    if (visLog) {
                        visLog->addLineBetweenPoints(boardStart, boardEnd, color4(0.0f, 1.0f, 1.0f, 1.0f));
                    }
                }
            }

            sRoofFacePoints.emplace_back(x, y);
            sHeightMap[f32v2(x, y)] = h;

            // Mark contour edges for thickening
            if (isContourEdge) {
                const auto& thisVert = he->vertex()->point();
                const auto& nextVert = he->next()->vertex()->point();
                auto&& extrudeIt = contourVertexInfo.find(f32v2(nextVert.x(), nextVert.y()));
                assert(extrudeIt != contourVertexInfo.end());
                // Figure out direction based on position offsets

                const ContourVertexInfo& vertexInfo = extrudeIt->second;
                contourEdges.emplace_back(RoofContourEdgeInfo{
                    f32v3(x, y, h),
                    f32v3(thisVert.x(), thisVert.y(), 0.0f),
                    f32v3(vertexInfo.extrudePos.x, vertexInfo.extrudePos.y, vertexInfo.extrudePos.z),
                    f32v3(nextVert.x(), nextVert.y(), 0.0f),
                    gablePosition,
                    Cartesian::INVALID,
                    isGable //  TODO: WRONG I THINK
                    });
            }

            he = he->next();

        } while (he != it->halfedge());

        if (!isGable) {

            triangulateRoofFacePolygons(meshBuilder, roofStyle, debugColorIndex, zPos, visLog);
        }

        ++debugColorIndex;
        if (debugColorIndex >= DEBUG_COLOR_ARRAY_SIZE) debugColorIndex = 0;
    }
}


void triangulateRoofFacePolygons(ProceduralMeshBuilder& meshBuilder, const RoofStyle& roofStyle, ui32 debugColorIndex, f32 zPos, VisualLog* visLog) {

    // Triangulation only works on convex polygons so we will partition the potentially concave poly into
    // separate convex polygons
    // https://stackoverflow.com/questions/1832430/c-cgal-2d-delauny-triangulation-concave-shapes
    // 
    // Partition 

    // Remove duplicate vertices
    boost::container::flat_set<TriangulationPoint> duplicateVertexCheck; // Should be impossible?
    CGAL::Partition_traits_2<K>::Polygon_2 concavePoly;
    for (auto&& pp : sRoofFacePoints) {
        if (duplicateVertexCheck.find(pp) == duplicateVertexCheck.end()) {
            concavePoly.push_back(pp);
            duplicateVertexCheck.insert(pp);
        }
        else {
            // TODO: Should be impossible?
            assert(false);
        }
    }
    std::list<CGAL::Partition_traits_2<K>::Polygon_2> convexPolygonList; // TODO: Add holes as well

    // Convex partition requires a "simple" polygon, no overlaps, and no 
    if (!concavePoly.is_simple()) {
        // TODO: This has happened recently, not sure why
        LOG_CRITICAL("Input polygon to CGAL::optimal_convex_partition_2 is not simple: ");
        DebugBreak();
        bool firstPoint = true;
        f32v2 prevPoint;
        // Vislog
        for (auto&& pp : concavePoly) {
            f32v2 p2(pp.x(), pp.y());
            if (firstPoint) {
                firstPoint = false;
            }
            else {
                if (visLog) { 
                    visLog->addLineBetweenPoints(f32v3(prevPoint.x, prevPoint.y, zPos), f32v3(p2.x, p2.y, zPos), COLOR_MAGENTA);
                }
            }
            prevPoint = p2;
            LOG_ERROR("  {}  {}", pp.x(), pp.y());
        }
        if (visLog) {
            visLog->addLineBetweenPoints(f32v3(prevPoint.x, prevPoint.y, zPos), f32v3(concavePoly.begin()->x(), concavePoly.begin()->y(), zPos), COLOR_MAGENTA);
        }
        return; // TODO: REMOVE (uhhh... why? wtf is tis)
        if (CGAL::orientation_2(concavePoly.vertices_begin(), concavePoly.vertices_end(), CGAL::Partition_traits_2<K>()) == CGAL::CLOCKWISE) {
            LOG_ERROR("Reversing vertices");
            std::reverse(concavePoly.vertices_begin(), concavePoly.vertices_end());
        }
    }

    // Partition poly into separate convex pieces
    CGAL::optimal_convex_partition_2(concavePoly.vertices_begin(), concavePoly.vertices_end(), std::back_inserter(convexPolygonList));

    const auto vislogRender = [](VisualLog* visLog, const f32v2 points2D[3], f32 zPos) {
        if (visLog) {
            f32v3 points3D[3];
            for (int j = 0; j < 3; ++j) {
                points3D[j].x = points2D[j].x;
                points3D[j].y = points2D[j].y;
                points3D[j].z = zPos;
            }
            visLog->addWireTriangle(points3D, COLOR_WHITE);
        }
    };
    for (auto&& convexPoly : convexPolygonList) {

        // Get triangle points
        f32v2 points[3];
        if (convexPoly.size() == 3) {
            // Simple triangles are just directly copied
            for (int i = 0; i < 3; ++i) {
                points[i].x = (f32)convexPoly.vertex(i).x();
                points[i].y = (f32)convexPoly.vertex(i).y();
            }
            vislogRender(visLog, points, zPos);
            addRoofTriangle(meshBuilder, points, roofStyle.shinglesMaterial, zPos);
        }
        else {
            // Triangulate higher order polys
            Triangulation triangulation;
            triangulation.insert(convexPoly.vertices_begin(), convexPoly.vertices_end());


            int q = 0;
            for (auto&& it = triangulation.finite_faces_begin(); it != triangulation.finite_faces_end(); ++it) {
                for (int i = 0; i < 3; ++i) {
                    points[i].x = (f32)it->vertex(i)->point().x();
                    points[i].y = (f32)it->vertex(i)->point().y();
                }
                // Add to mesh
                vislogRender(visLog, points, zPos);
                addRoofTriangle(meshBuilder, points, roofStyle.shinglesMaterial, zPos);
            }
        }
    }
}

void addRoofQuad(
    ProceduralMeshBuilder& meshBuilder,
    const f32v3 points[4],
    const MaterialDesc& materialData
) {

    StaticModelVertex verts[4];
    for (int i = 0; i < 4; ++i) {
        verts[i].pos = points[i];
        verts[i].color = color4(1.0f, 1.0f, 1.0f, 1.0f);
    }

    // Determine orientation
    const f32v3 o1 = verts[1].pos - verts[0].pos;
    const f32v3 o2 = verts[2].pos - verts[0].pos;
    f32v3 normal = glm::normalize(glm::cross(o1, o2));
    Cartesian dir = Cartesian::WEST;
    if (abs(normal.x) < abs(normal.y)) {
        if (normal.y > 0) {
            dir = Cartesian::NORTH;
        }
        else {
            dir = Cartesian::SOUTH;
        }
    }
    else if (normal.x > 0) {
        dir = Cartesian::EAST;
    }

    // Determine how we get UVs
    const ui32v2 uvAxis = AXIS_UV_LOOKUP_FROM_CARTESIAN[e_cast(dir)];

    const f32 UV_SCALE = 0.4f;
    ui32 compressedNormal = Pack_INT_2_10_10_10_REV(normal.x, normal.y, normal.z, 0.0f);
    const f32v3& tangent(CUBE_FACING_TANGENTSF[e_cast(dir)]);
    ui32 compressedTangent = Pack_INT_2_10_10_10_REV(tangent.x, tangent.y, tangent.z, 0.0f);
    if (normal.z > 0.3f) {
        for (int i = 0; i < 4; ++i) {
            verts[i].normalPacked = compressedNormal;
            verts[i].tangentPacked = compressedTangent;
            f32v2 uvs(verts[i].pos[uvAxis.x] * UV_SCALE, (verts[i].pos[uvAxis.y]) * UV_SCALE * AXIS_V_DIR_FROM_CARTESIAN[e_cast(dir)] * (1.0f - ROOF_HEIGHT_MULT * 0.5f));
            verts[i].uvsPacked = PackUVs(uvs);
        }
    }
    else {
        // Different texturing for mostly vertical polygons
        for (int i = 0; i < 4; ++i) {
            verts[i].normalPacked = compressedNormal;
            verts[i].tangentPacked = compressedTangent;
            f32v2 uvs((verts[i].pos[uvAxis.x]) * UV_SCALE, verts[i].pos.z * UV_SCALE * AXIS_V_DIR_FROM_CARTESIAN[e_cast(dir)]);
            verts[i].uvsPacked = PackUVs(uvs);
        }
    }
    meshBuilder.addQuad(verts, materialData, false);
}

void addRoofTriangle(
    ProceduralMeshBuilder& meshBuilder,
    const f32v3 points[3],
    const MaterialDesc& materialData
) {

    StaticModelVertex verts[3];
    for (int i = 0; i < 3; ++i) {
        verts[i].pos = points[i];
        verts[i].color = color4(1.0f, 1.0f, 1.0f, 1.0f);
    }

    // Determine orientation
    const f32v3 o1 = verts[1].pos - verts[0].pos;
    const f32v3 o2 = verts[2].pos - verts[0].pos;
    f32v3 normal = glm::normalize(glm::cross(o1, o2));
    // Invert normal if needed
    if (normal.z < 0.0f) normal = -normal;
    Cartesian dir = Cartesian::WEST;
    if (abs(normal.x) < abs(normal.y)) {
        if (normal.y > 0) {
            dir = Cartesian::NORTH;
        }
        else {
            dir = Cartesian::SOUTH;
        }
    }
    else if (normal.x > 0) {
        dir = Cartesian::EAST;
    }

    // Determine how we get UVs
    const ui32v2 uvAxis = AXIS_UV_LOOKUP_FROM_CARTESIAN[e_cast(dir)];

    const f32 UV_SCALE = 0.4f;
    ui32 compressedNormal = Pack_INT_2_10_10_10_REV(normal.x, normal.y, normal.z, 0.0f);
    const f32v3& tangent(CUBE_FACING_TANGENTSF[e_cast(dir)]);
    ui32 compressedTangent = Pack_INT_2_10_10_10_REV(tangent.x, tangent.y, tangent.z, 0.0f);
    if (normal.z > 0.3f) {
        for (int i = 0; i < 3; ++i) {
            verts[i].normalPacked = compressedNormal;
            verts[i].tangentPacked = compressedTangent;
            f32v2 uvs(verts[i].pos[uvAxis.x] * UV_SCALE, (verts[i].pos[uvAxis.y]) * UV_SCALE * AXIS_V_DIR_FROM_CARTESIAN[e_cast(dir)] * (1.0f - ROOF_HEIGHT_MULT * 0.5f));
            verts[i].uvsPacked = PackUVs(uvs);
        }
    }
    else {
        // Different texturing for mostly vertical polygons
        for (int i = 0; i < 3; ++i) {
            verts[i].normalPacked = compressedNormal;
            verts[i].tangentPacked = compressedTangent;
            f32v2 uvs((verts[i].pos[uvAxis.x]) * UV_SCALE, verts[i].pos.z * UV_SCALE * AXIS_V_DIR_FROM_CARTESIAN[e_cast(dir)]);
            verts[i].uvsPacked = PackUVs(uvs);
        }
    }
    meshBuilder.addTriangle(verts, materialData, false);
}

void addRoofTriangle(
    ProceduralMeshBuilder& meshBuilder,
    const f32v2 points[3],
    const MaterialDesc& materialData,
    f32 zPos
) {

    // Lookup z height
    f32v3 verts[3];
    for (int i = 0; i < 3; ++i) {
        const f32 x = points[i].x;
        const f32 y = points[i].y;

        auto&& it = sHeightMap.find(f32v2(x, y));

        const f32 deg1 = (it == sHeightMap.end()) ? 0.0f : it->second;
        verts[i] = f32v3(x, y, zPos + deg1 + ROOF_THICKNESS);
    }

    addRoofTriangle(meshBuilder, verts, materialData);
}

void meshGable(VisualLog* visLog, const RoofContourEdgeInfo& edge, f32 zPos, Cartesian dir, ProceduralMeshBuilder& meshBuilder, const RoofStyle& roofStyle) {
    // Debug render gable
    if (visLog) visLog->addLineBetweenPoints(edge.v2, edge.v1, color4(1.0f, 1.0f, 1.0f));

    constexpr f32 GABLE_EXTRUDE_DISTANCE = 1.0f;
    f32v3 outerPoints[3];
    f32v3 innerPoints[3];
    innerPoints[0] = edge.parent1;
    innerPoints[1] = edge.parent2;
    innerPoints[2] = edge.gablePos;
    outerPoints[0] = edge.v1;
    outerPoints[1] = edge.v2;
    outerPoints[2] = edge.gablePos;
    for (int i = 0; i < 3; ++i) {
        innerPoints[i].z += zPos + ROOF_THICKNESS;
        outerPoints[i].z += zPos + ROOF_THICKNESS;
    }

    const f32v3 baseV1 = outerPoints[0];
    const f32v3 baseV2 = outerPoints[1];

    // Extrude parent vertices along wall (Gable already did)
    switch (dir) {
        case Cartesian::SOUTH:
            innerPoints[0].y -= WALL_HALF_THICKNESS;
            innerPoints[1].y -= WALL_HALF_THICKNESS;
            for (int i = 0; i < 3; ++i) {
                outerPoints[i].y -= GABLE_EXTRUDE_DISTANCE;
            }
            break;
        case Cartesian::WEST:
            innerPoints[0].x -= WALL_HALF_THICKNESS;
            innerPoints[1].x -= WALL_HALF_THICKNESS;
            for (int i = 0; i < 3; ++i) {
                outerPoints[i].x -= GABLE_EXTRUDE_DISTANCE;
            }
            break;
        case Cartesian::EAST:
            innerPoints[0].x += WALL_HALF_THICKNESS;
            innerPoints[1].x += WALL_HALF_THICKNESS;
            for (int i = 0; i < 3; ++i) {
                outerPoints[i].x += GABLE_EXTRUDE_DISTANCE;
            }
            break;
        case Cartesian::NORTH:
            innerPoints[0].y += WALL_HALF_THICKNESS;
            innerPoints[1].y += WALL_HALF_THICKNESS;
            for (int i = 0; i < 3; ++i) {
                outerPoints[i].y += GABLE_EXTRUDE_DISTANCE;
            }
            break;
        default:
            break;
    }

    // Wall triangle
    addRoofTriangle(meshBuilder, innerPoints, roofStyle.primaryBoardMaterial);

    // Bottom trim board
    constexpr f32 BOARD_SIZE_VARIANCE = 0.03f;
    const f32v2 halfDims = f32v2(
        0.1f + (randFromf32v3(innerPoints[0] - innerPoints[1], (ui64)&edge.gablePos /*hax*/) - 0.5f) * BOARD_SIZE_VARIANCE
    );
    meshBuilder.addBoardBetweenPoints(innerPoints[0], innerPoints[1], halfDims, roofStyle.primaryBoardMaterial, f32v2(1.0));

    // Extrude upper board
    meshBuilder.addBoardBetweenPoints(innerPoints[2], outerPoints[2], halfDims, roofStyle.primaryBoardMaterial, f32v2(1.0));
    // Quad left top
    f32v3 leftQuad[4] = { baseV1, outerPoints[0], outerPoints[2], innerPoints[2] };
    addRoofQuad(meshBuilder, leftQuad, roofStyle.shinglesMaterial);
    // Quad right top
    f32v3 rightQuad[4] = { baseV2, innerPoints[2], outerPoints[2], outerPoints[1] };
    addRoofQuad(meshBuilder, rightQuad, roofStyle.shinglesMaterial);

    for (int i = 0; i < 4; ++i) {
        leftQuad[i].z -= ROOF_THICKNESS;
        rightQuad[i].z -= ROOF_THICKNESS;
    }
    // Swaps to fix winding
    std::swap(leftQuad[1], leftQuad[3]);
    std::swap(rightQuad[1], rightQuad[3]);
    // Left quad bottom
    addRoofQuad(meshBuilder, leftQuad, roofStyle.primaryBoardMaterial);
    // Right quad bottom
    addRoofQuad(meshBuilder, rightQuad, roofStyle.primaryBoardMaterial);

    const f32v2 trimBoardHalfDims(TRIM_BOARD_HALF_THICKNESS);
    // Left side trim
    f32v3 leftQuadSideEdge[4] = {leftQuad[0], leftQuad[3], outerPoints[0], baseV1};
    meshBuilder.addBoardBetweenPoints(
        (baseV1 + leftQuad[0]) * 0.5f ,
        (outerPoints[0] + leftQuad[3]) * 0.5f,
        trimBoardHalfDims,
        roofStyle.primaryBoardMaterial,
        f32v2(1.0f)
    );
    
    // Right side trim
    f32v3 rightQuadSideEdge[4] = { rightQuad[0], baseV2, outerPoints[1], rightQuad[1] };
    meshBuilder.addBoardBetweenPoints(
        (baseV2 + rightQuad[0]) * 0.5f,
        (outerPoints[1] + rightQuad[1]) * 0.5f,
        trimBoardHalfDims,
        roofStyle.primaryBoardMaterial,
        f32v2(1.0f)
    );

    const f32v3 outerBottom = outerPoints[2] - f32v3(0.0f, 0.0f, ROOF_THICKNESS);
    const f32v3 outerMiddle = outerPoints[2] - f32v3(0.0f, 0.0f, ROOF_THICKNESS * 0.5f);
    // Left quad end edge trim board
    const f32v3 leftSideTrimPoints[2] = { (leftQuadSideEdge[1] + leftQuadSideEdge[2]) * 0.5f, outerMiddle };
    meshBuilder.addBoardBetweenPoints(leftSideTrimPoints[0], leftSideTrimPoints[1], trimBoardHalfDims, roofStyle.primaryBoardMaterial, f32v2(1.0f));
    // Right quad end edge trim board
    const f32v3 rightSideTrimPoints[2] = { (rightQuadSideEdge[2] + rightQuadSideEdge[3]) * 0.5f, outerMiddle };
    meshBuilder.addBoardBetweenPoints(rightSideTrimPoints[0], rightSideTrimPoints[1], trimBoardHalfDims, roofStyle.primaryBoardMaterial, f32v2(1.0f));

    // Placeholder support thingies (Replace with model)
    const f32v3 offsetDown(0.0f, 0.0f, 0.5f);
    meshBuilder.addBoardBetweenPoints(innerPoints[0], innerPoints[0] - offsetDown, halfDims * 1.5f, roofStyle.primaryBoardMaterial, f32v2(1.0));
    meshBuilder.addBoardBetweenPoints(innerPoints[1], innerPoints[1] - offsetDown, halfDims * 1.5f, roofStyle.primaryBoardMaterial, f32v2(1.0));

    // Outer diagonal support boards
    const f32v2 supportBoardHalfDims(TRIM_BOARD_HALF_THICKNESS);
    const f32v3 ol1 = leftQuad[2] - leftQuad[0];
    const f32v3 ol2 = leftQuad[1] - leftQuad[0];
    const f32v3 normalLeft = glm::normalize(glm::cross(ol1, ol2));
    const f32v3 offsetLeft = normalLeft * supportBoardHalfDims.x;
    const f32v3 or1 = rightQuad[2] - rightQuad[0];
    const f32v3 or2 = rightQuad[1] - rightQuad[0];
    const f32v3 normalRight = glm::normalize(glm::cross(or1, or2));
    const f32v3 offsetRight = normalRight * supportBoardHalfDims.x;
    meshBuilder.addBoardBetweenPoints(innerPoints[0] - offsetLeft - f32v3(0.0f, 0.0f, ROOF_THICKNESS), leftQuadSideEdge[1] - offsetLeft, supportBoardHalfDims, roofStyle.primaryBoardMaterial, f32v2(1.0), normalLeft);
    meshBuilder.addBoardBetweenPoints(innerPoints[1] - offsetRight - f32v3(0.0f, 0.0f, ROOF_THICKNESS), rightQuadSideEdge[3] - offsetRight, supportBoardHalfDims, roofStyle.primaryBoardMaterial, f32v2(1.0), normalRight);

    { // Outer straight support boards
        constexpr f32 SPACING = 0.34f;
        const f32v3 innerStepLeft = (innerPoints[2] - innerPoints[0]) * SPACING;
        const f32v3 outerStepLeft = (leftSideTrimPoints[1] - leftSideTrimPoints[0]) * SPACING;
        const f32v3 innerStepRight = (innerPoints[2] - innerPoints[1]) * SPACING;
        const f32v3 outerStepRight = (rightSideTrimPoints[1] - rightSideTrimPoints[0]) * SPACING;
        for (int i = 1; i < 3; ++i) {
            const f32v3 innerPointLeft = innerPoints[0] + innerStepLeft * (f32)i;
            const f32v3 outerPointLeft = leftSideTrimPoints[0] + outerStepLeft * (f32)i;
            meshBuilder.addBoardBetweenPoints(innerPointLeft - offsetLeft, outerPointLeft - offsetLeft, supportBoardHalfDims, roofStyle.primaryBoardMaterial, f32v2(1.0), normalLeft);
            const f32v3 innerPointRight = innerPoints[1] + innerStepRight * (f32)i;
            const f32v3 outerPointRight = rightSideTrimPoints[0] + outerStepRight * (f32)i;
            meshBuilder.addBoardBetweenPoints(innerPointRight - offsetRight, outerPointRight - offsetRight, supportBoardHalfDims, roofStyle.primaryBoardMaterial, f32v2(1.0), normalRight);
        }
    }

    /*  if (visLog) {
          visLog->addLineBetweenPoints(rightQuadSideEdge[0], rightQuadSideEdge[1], color4(1.0f, 0.0f, 1.0f));
          visLog->addLineBetweenPoints(rightQuadSideEdge[1], rightQuadSideEdge[2], color4(1.0f, 0.0f, 1.0f));
          visLog->addLineBetweenPoints(rightQuadSideEdge[2], rightQuadSideEdge[3], color4(1.0f, 0.0f, 1.0f));
          visLog->addLineBetweenPoints(rightQuadSideEdge[3], rightQuadSideEdge[0], color4(1.0f, 0.0f, 1.0f));
      }*/
}

void meshRoofContourEdges(const std::vector<RoofContourEdgeInfo>& contourEdges, ProceduralMeshBuilder& meshBuilder, const RoofStyle& roofStyle, f32 zPos, VisualLog* visLog) {
    const f32v2 trimBoardHalfDims(TRIM_BOARD_HALF_THICKNESS);
    for (auto&& edge : contourEdges) {
        if (edge.v1 == edge.parent1 && edge.v2 == edge.parent2) {
            // Ignore cases where we meld into the wall due to collision
            return;
        }

        // Compute edge dir
        Cartesian dir;
        const f32 xDiff = abs(edge.v2.x - edge.v1.x);
        const f32 yDiff = abs(edge.v2.y - edge.v1.y);
        if (xDiff > yDiff) {
           if (edge.v2.x > edge.v1.x) {
               if (visLog) visLog->addLineBetweenPoints(edge.v2, edge.v1, color4(1.0f, 0.0f, 0.0f));
               dir = Cartesian::SOUTH;
            }
           else {
               if (visLog) visLog->addLineBetweenPoints(edge.v2, edge.v1, color4(0.0f, 1.0f, 0.0f));
               dir = Cartesian::NORTH;
           }
        }
        else {
            if (edge.v2.y > edge.v1.y) {
                if (visLog) visLog->addLineBetweenPoints(edge.v2, edge.v1, color4(0.0f, 1.0f, 1.0f));
                dir = Cartesian::EAST;
            }
            else {
                if (visLog) visLog->addLineBetweenPoints(edge.v2, edge.v1, color4(0.0f, 0.0f, 1.0f));
                dir = Cartesian::WEST;
            }
        }
        if (edge.isGable) {
            meshGable(visLog, edge, zPos, dir, meshBuilder, roofStyle);
        }
        else {
            const f32v3 first(edge.v1.x, edge.v1.y, edge.v1.z + zPos);
            const f32v3 second(edge.v2.x, edge.v2.y, edge.v2.z + zPos);

            f32v3 points[4];
            // Side trim
            points[0] = first;
            points[1] = second;
            points[2] = second + f32v3(0.0f, 0.0f, ROOF_THICKNESS);
            points[3] = first + f32v3(0.0f, 0.0f, ROOF_THICKNESS);
            meshBuilder.addBoardBetweenPoints((points[0] + points[3]) * 0.5f, (points[1] + points[2]) * 0.5f, trimBoardHalfDims, roofStyle.primaryBoardMaterial, f32v2(1.0f));
            // Bottom quad
            points[0] = second;
            points[1] = first;
            points[2] = f32v3(edge.parent1.x, edge.parent1.y, zPos - ROOF_THICKNESS);
            points[3] = f32v3(edge.parent2.x, edge.parent2.y, zPos - ROOF_THICKNESS);
            meshBuilder.addQuadBetweenPoints(points, roofStyle.primaryBoardMaterial, f32v2(1.0f), COLOR_WHITE, false);

            // Non gables have supporting boards
            // Step along the edge and add extruded board pieces
            // Random dims
            constexpr f32 BOARD_SIZE_VARIANCE = 0.04f;
            constexpr f32 BOARD_GAP_VARIANCE = 0.1f;
            constexpr f32 BOARD_ANGLE_VARIANCE = 0.0f;
            constexpr f32 BOARD_LENGTH_VARIANCE = 0.15f;
            constexpr f32 BOARDS_PER_METER = 2;
            constexpr f32 BOARD_LENGTH_BASE = ROOF_EXTRUDE_DISTANCE - 0.35f;
            constexpr f32 BOARD_START_DEPTH_MULT = 0.6f;
            const f32v3 diff = second - first;
            const f32 distance = glm::length(diff);
            const f32v3 iterNormal = diff / distance;
            const f32v3& edgeNormal = CARTESIAN_NORMALS_3D[e_cast(dir)];
            const int boardCount = (int)round(distance * BOARDS_PER_METER);
            const f32 boardGapSize = distance / (boardCount + 1);
            const f32v3 start = first - edgeNormal * ROOF_EXTRUDE_DISTANCE * BOARD_START_DEPTH_MULT;
            // TODO: check for intersections with other rooms
            for (int i = 1; i <= boardCount; ++i) {
                // Get dims
                const f32v2 boardHalfDims = f32v2(
                    0.03f + randFromf32v3(first, i << 3) * BOARD_SIZE_VARIANCE,
                    0.03f + randFromf32v3(second, i << 3) * BOARD_SIZE_VARIANCE
                );
                const f32v3 startWithBoardOffset = f32v3(start.x, start.y, start.z + 0.22f - boardHalfDims.y);
                // Extruded boards with random offset variance
                const f32v3 offset = iterNormal * (i * boardGapSize + (randFromf32v3(startWithBoardOffset, i << 4) - 0.5f) * BOARD_GAP_VARIANCE);
                const f32v3 p1 = startWithBoardOffset + offset;
                const f32 boardLength = BOARD_LENGTH_BASE + (randFromf32v3(offset, i << 2) - 0.5f) * BOARD_LENGTH_VARIANCE;
                const f32v3 p2 = p1 + edgeNormal * boardLength - f32v3(0.0f, 0.0f, ROOF_HEIGHT_MULT * (0.5f + (randFromf32v3(p1, i) - 0.5f) * BOARD_ANGLE_VARIANCE));
                meshBuilder.addBoardBetweenPoints(p1, p2, boardHalfDims, roofStyle.primaryBoardMaterial, f32v2(1.0f));
            }
        }
    }
}

void meshRoomCeilings(ContainerMeshBuilders& meshBuilders, const RoofStyle& roofStyle) {
    constexpr f32 CEILING_THICKNESS = 0.05f;
    const TileSpatialGrid& spatialGrid = meshBuilders.tileData.spatialGrid;
    const i32v3 dims = spatialGrid.getDims();
    TileIndex index = 0;
    const f32v4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);
    for (ui32 z = 0; z < dims.z; ++z) {
        for (ui32 y = 0; y < dims.y; ++y) {
            for (ui32 x = 0; x < dims.x; ++x, ++index) {
                if (meshBuilders.tileData.tiles[index].hasFlag(TileFlags::ROOFED)) {
                    // If were at the top or the tile above us is outside the interior, or its interior and a non air tile above us, mesh a ceiling
                    const TileIndex aboveIndex = index + dims.x * dims.y;
                    if (z == dims.z - 1 || // If were at the top
                        !meshBuilders.tileData.tiles[aboveIndex].hasFlag(TileFlags::ROOFED) // Or tile above us is an exterior tile
                        /*|| !tileContainer.getTileAt(aboveIndex).isEmpty()*/) { // Or its an interior tile and not empty
                        // Mesh ceiling
                        f32v3 startPos(x, y, spatialGrid.getFloorHeight() * (z + 1) - CEILING_THICKNESS);
                        meshBuilders.staticBuilder.addAxisAlignedQuad(startPos, f32v2(1.0f), CubeFacing::BOTTOM, roofStyle.primaryBoardMaterial, uvRect, COLOR_WHITE);
                        // TODO: Cull edges appropriately
                        meshBuilders.staticBuilder.addAxisAlignedQuad(startPos, f32v2(1.0f, CEILING_THICKNESS), CubeFacing::LEFT, roofStyle.primaryBoardMaterial, uvRect, COLOR_WHITE);
                        meshBuilders.staticBuilder.addAxisAlignedQuad(startPos, f32v2(1.0f, CEILING_THICKNESS), CubeFacing::RIGHT, roofStyle.primaryBoardMaterial, uvRect, COLOR_WHITE);
                        meshBuilders.staticBuilder.addAxisAlignedQuad(startPos, f32v2(1.0f, CEILING_THICKNESS), CubeFacing::FRONT, roofStyle.primaryBoardMaterial, uvRect, COLOR_WHITE);
                        meshBuilders.staticBuilder.addAxisAlignedQuad(startPos, f32v2(1.0f, CEILING_THICKNESS), CubeFacing::BACK, roofStyle.primaryBoardMaterial, uvRect, COLOR_WHITE);
                    }
                }
            }
        }
    }
}

void meshRoomUndercarriage(ContainerMeshBuilders& meshBuilders, const RoofStyle& roofStyle) {
    const TileSpatialGrid& spatialGrid = meshBuilders.tileData.spatialGrid;
    const i32v3 dims = spatialGrid.getDims();
    const ui32 floorStride = dims.x * dims.y;
    TileIndex index = 0;
    const f32v4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);
    for (i32 z = 0; z < dims.z; ++z) {
        for (i32 y = 0; y < dims.y; ++y) {
            for (i32 x = 0; x < dims.x; ++x, ++index) {
                // Check if we should start supports here, i.e. below us is outside the building
                if (meshBuilders.tileData.tiles[index].hasFlag(TileFlags::ROOFED) && (z == 0 || !meshBuilders.tileData.tiles[index - floorStride].hasFlag(TileFlags::ROOFED))) {
                    // Place floor tiles and increment X
                    const ui32 startX = x;
                    do {
                        // Disabled now since floor takes care of it
                       // const f32v3 floorPos(x, y, tileContainer.getFloorHeight() * z - 0.0001f);
                       // meshBuilder.addAxisAlignedQuad(floorPos, f32v2(1.0f), CubeFacing::BOTTOM, roofStyle.primaryBoardMaterial, uvRect, COLOR_WHITE);
                    } while (++x < dims.x && meshBuilders.tileData.tiles[++index].hasFlag(TileFlags::ROOFED));
                    // TODO: ADD BOARD
                    const f32 boardThickness = 0.1f;
                    f32v3 startPos(startX, y + 0.5f, spatialGrid.getFloorHeight() * z - boardThickness - 0.0001f);
                    f32v3 endPos = startPos + f32v3(x - startX, 0.0f, 0.0f);
                    meshBuilders.staticBuilder.addBoardBetweenPoints(startPos, endPos, f32v2(boardThickness), roofStyle.primaryBoardMaterial, f32v2(1.0f));
                }
            }
        }
    }
}
