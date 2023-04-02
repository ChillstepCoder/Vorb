#include "stdafx.h"
#include "BuildingMesher.h"

#include "city/Building.h"
#include "resources/ResourceManager.h"
#include "resources/MaterialRepository.h"

#include "debugging/DebugRenderer.h"
#include "debugging/VisualLogger.h"

#include "util/IntersectionUtil.h"

#include "world/IWorld.h"

#include "rendering/mesh/Mesh.h"
#include "rendering/mesh/mesher/builder/ContainerMeshBuilders.h"
#include "rendering/model/InstancedStaticModelGatherer.h"

#include "options/DebugOptions.h"

#include "math/Random.h"

#include "tile/TileHandle.h"
#include "resources/TileRepository.h"
#include "rendering/mesh/mesher/builder/TileMeshBuilderMethods.h"

#include "util/GridEdgeFinder.h"

#include "physics/PhysicsWorld.h"

#include "rendering/RenderThreadTasks.h"
#include "gamethread/GameThreadTasks.h"

constexpr f32 ROOF_THICKNESS = 0.04f;
constexpr f32 ROOF_EXTRUDE_DISTANCE = 0.45f;
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


//class f32v2HashFunction {
//public:
//    size_t operator()(const f32v2& f) const
//    {
//        return (size_t)std::hash<f32>{}(f.x) ^ (size_t)std::hash<f32>{}(f.y * 127.0f);
//    }
//};

thread_local std::unordered_map<f32v2, f32, f32v2hash> sHeightMap;
thread_local std::vector<TriangulationPoint> sRoofFacePoints;
thread_local Polygon_2 sCgalPoly;


struct RoofSkeletonVertex;

enum Corners {
    CORNER_TOP_LEFT = 0,
    CORNER_TOP_RIGHT = 1,
    CORNER_BOTTOM_LEFT = 2,
    CORNER_BOTTOM_RIGHT = 3
};


constexpr int MAX_ROOF_VERTICES = 8192;
thread_local f32v2 sRoofVertices[MAX_ROOF_VERTICES];


// TODO: MathUtil
f32v2 rotate90(f32v2 vec) {
    return f32v2(vec.y, -vec.x);
}
f32v2 rotate270(f32v2 vec) {
    return f32v2(-vec.y, vec.x);
}

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

struct GableTargetPointInfo {
    f32v2 pos;
    ui32 borderCount; // If this is ever > 1, we ignore

    bool isValidGable() const { return borderCount == 1; }
};

bool collideExtrudeWalls(const i32v2& start, const i32v2& end, ui32 axis, const i32AABB3& aabb, const ui32 floorIndex, f32 zPos, const Building& building, VisualLog* visLog) {
    // If we are out of the AABB, its a collide
    if (start[!axis] < 0 || start[!axis] >= aabb.dims[!axis]) {
        return true;
    }
    else {
        // Loop along our wall and check for collisions with tiles (or out of AABB)
        if (start[axis] < end[axis]) {
            for (int i = start[axis]; i <= end[axis]; ++i) {
                if (i < 0 || i >= aabb.dims[axis]) {
                    return true;
                }
                ui32 bitIndex;
                if (axis == 0) {
                    bitIndex = floorIndex + start.y * aabb.dims.x + i;
                    if (visLog) visLog->addWireQuad(f32v3(i, start.y, zPos), f32v2(1.0f), color4(1.0f, 1.0f, 0.0f, 1.0f));
                }
                else {
                    bitIndex = floorIndex + i * aabb.dims.x + start.x;
                    if (visLog) visLog->addWireQuad(f32v3(start.x, i, zPos), f32v2(1.0f), color4(1.0f, 1.0f, 0.0f, 1.0f));
                }
                if (building.getInteriorTilesInAABB().getBit(bitIndex)) {
                    return true;
                }
            }
        }
        else {
            for (int i = start[axis]; i >= end[axis]; --i) {
                if (i < 0 || i >= aabb.dims[axis]) {
                    return true;
                }
                ui32 bitIndex;
                if (axis == 0) {
                    bitIndex = floorIndex + start.y * aabb.dims.x + i;
                    if (visLog) visLog->addWireQuad(f32v3(i, start.y, zPos), f32v2(1.0f), color4(1.0f, 1.0f, 0.0f, 1.0f));
                }
                else {
                    bitIndex = floorIndex + i * aabb.dims.x + start.x;
                    if (visLog) visLog->addWireQuad(f32v3(start.x, i, zPos), f32v2(1.0f), color4(1.0f, 1.0f, 0.0f, 1.0f));
                }
                if (building.getInteriorTilesInAABB().getBit(bitIndex)) {
                    return true;
                }
            }
        }
    }
    return false;
}

void computeGablePointsAndExtrudePositions(const Building& building, ui32 floor, f32 zPos, std::unordered_map<f32v2, GableTargetPointInfo, f32v2hash>& gableTargetPoints, std::unordered_map<f32v2, f32v3, f32v2hash>& contourExtrudePositions, SsPtr iss, VisualLog* visLog) {
    gableTargetPoints.reserve(10);
    contourExtrudePositions.reserve(30);
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
                gableTarget.x = (f32)(he->prev()->vertex()->point().x() + he->next()->vertex()->point().x()) / 2.0f;
                gableTarget.y = (f32)(he->prev()->vertex()->point().y() + he->next()->vertex()->point().y()) / 2.0f;
                const f32v2 lookup = f32v2(he->vertex()->point().x(), he->vertex()->point().y());
                auto&& it = gableTargetPoints.find(lookup);
                if (it == gableTargetPoints.end()) {
                    gableTargetPoints[lookup] = GableTargetPointInfo{ gableTarget, 1 };
                }
                else {
                    ++it->second.borderCount;
                }
                // Visual log
                if (visLog) {
                    const auto& thisPoint = he->vertex()->point();
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
                f32v3 extrudePosition(thisPoint.x() - oppositePoint.x(), thisPoint.y() - oppositePoint.y(), h - he->opposite()->vertex()->time() * ROOF_HEIGHT_MULT);
                extrudePosition = normalize(extrudePosition) * ROOF_EXTRUDE_DISTANCE + f32v3(thisPoint.x(), thisPoint.y(), 0.0f);
                contourExtrudePositions[f32v2(thisPoint.x(), thisPoint.y())] = extrudePosition;
            }
            he = he->next();
        } while (he != it->halfedge());
    }

    // Fixup extrude positions that may be colliding with walls on above floors
    const ui32 floorIndex = floor * building.getAABB().dims.y * building.getAABB().dims.x;
    const i32AABB3& aabb = building.getAABB();
    for (auto&& it = iss->faces_begin(); it != iss->faces_end(); ++it) {
        auto&& he = it->halfedge();
        do {
            const auto& thisPoint = he->vertex()->point();
            auto&& it1 = contourExtrudePositions.find(f32v2(thisPoint.x(), thisPoint.y()));
            if (it1 != contourExtrudePositions.end()) {
                const auto& oppositePoint = he->opposite()->vertex()->point();
                auto&& it2 = contourExtrudePositions.find(f32v2(oppositePoint.x(), oppositePoint.y()));
                if (it2 != contourExtrudePositions.end()) {
                    // Loop along the edge and check for collisions
                    const i32v2 start((int)glm::floor(it1->second.x), (int)glm::floor(it1->second.y));
                    const i32v2 end((int)glm::floor(it2->second.x), (int)glm::floor(it2->second.y));
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
                        if (collideExtrudeWalls(start, end, 1, aabb, floorIndex, zPos, building, visLog)) {
                            it1->second.x = it1->first.x;
                            it2->second.x = it2->first.x;
                            it1->second.z = 0.0f;
                            it2->second.z = 0.0f;
                        }
                    }
                    else {
                        // X Wall
                        if (collideExtrudeWalls(start, end, 0, aabb, floorIndex, zPos, building, visLog)) {
                            it1->second.y = it1->first.y;
                            it2->second.y = it2->first.y;
                            it1->second.z = 0.0f;
                            it2->second.z = 0.0f;
                        }
                    }
                }
            }
            he = he->next();
        } while (he != it->halfedge());
    }
}

f32 randFromf32v3(const f32v3& x, ui64 additional) {
    return Random::getThreadSafef((ui64)f32v3hash()(x) + additional);
}


void BuildingMesher::buildMeshAndPhysicsAsync(const Building& building) const {
    initMeshAndPhysicsAsyncInternal(*building.getTileContainer(), nullptr, false, 10000 /*reserveCount*/, &building);
}

void BuildingMesher::addCustomMeshData(ContainerMeshBuilders& meshBuilders, StaticPhysicsMeshBuilder& physicsBuilder, const void* userData) const {
    PROFILE_FUNCTION();

    const Building& building = *static_cast<const Building*>(userData);

    const i32AABB3& aabb = building.mAABB;
    const TileContainer& tileContainer = *building.mTileContainer;
    const BitArray& ownedTiles = tileContainer.getOwnedTiles();

    PhysicsWorld& physWorld = sWorld->getPhysicsWorld();

    // Debug log
    VisualLog* visLog = VisualLogger::tryGetNewVisualLog("building");
    if (visLog) {
        visLog->setRootPos(tileContainer.getWorldPos3D());
        visLog->nextStep("AABB");
        visLog->addWireQuad(f32v3(0.0f), building.mAABB.dims, color4(1.0f, 0.0f, 0.0f, 0.9f));
    }

    // Materials
    const MaterialData& shinglesMaterial = Services::ResourceManager::ref().getMaterialRepository().getMaterialData("roof");
    const MaterialData& rawWoodMaterial = Services::ResourceManager::ref().getMaterialRepository().getMaterialData("raw_wood_dark");

    // TODO: Not right
    sHeightMap.reserve(100);
    sRoofFacePoints.reserve(100);

    // ========================== Straight Skeleton ===============================
    const ui32 floorCount = tileContainer.getDims().z;
    const ui32 floorTileCount = building.mAABB.dims.y * building.mAABB.dims.x;
    BitArray roofedTiles;
    roofedTiles.resize(building.mAABB.dims.x * building.mAABB.dims.y);
    for (ui32 floor = 0; floor < floorCount; ++floor) {
        const f32 zPos = (floor + 1.0f) * tileContainer.getFloorHeight();
        // TODO: Replace bitarray with bool array
        roofedTiles.zeroAllBits();
        for (i32 y = 0; y < building.mAABB.dims.y; ++y) {
            for (i32 x = 0; x < building.mAABB.dims.x; ++x) {
                const ui32 floorBitIndex = y * building.mAABB.dims.x + x;
                const ui32 buildingBitIndex = floor * floorTileCount + floorBitIndex;
                // If we own this tile, and above us is clear, we are a roofed tile
                if (ownedTiles.getBit(buildingBitIndex) &&
                    (floor == floorCount - 1 || !ownedTiles.getBit(buildingBitIndex + floorTileCount))) {
                    roofedTiles.setBitTo(floorBitIndex, true);
                }
            }
        }

        // Generate a list of straight skeletons
        if (visLog) visLog->nextStep("Detect roof edges " + std::to_string(floor));
        std::vector<SsPtr> iss = buildRoofStraightSkeletons(roofedTiles, building, zPos, visLog);
        std::vector<RoofContourEdgeInfo> contourEdges;
        contourEdges.reserve(20);

        // Mesh each individual straight skeleton
        ui32 n = 0;
        for (auto& ss : iss) {

            if (visLog) visLog->nextStep("Skeleton " + std::to_string(floor) + " " + std::to_string(n));
            buildMeshFromStraightSkeleton(ss, building, meshBuilders.staticBuilder, contourEdges, rawWoodMaterial, shinglesMaterial, floor, zPos, visLog);

            // ========================== Contours and extruded side boards ===============================
            meshRoofContourEdges(contourEdges, building, meshBuilders.staticBuilder, shinglesMaterial, rawWoodMaterial, zPos, visLog);
            contourEdges.clear();
        }
    }

    // ========================== Room Ceilings ===============================
    meshRoomCeilings(building, meshBuilders.staticBuilder, rawWoodMaterial);

    // ========================== Room supports ===============================
    meshRoomUndercarriage(building, meshBuilders.staticBuilder, rawWoodMaterial);


    physicsBuilder.setRootPos(f32v3(building.mAABB.pos));

    if (visLog) visLog->finish();
}

std::vector<SsPtr> BuildingMesher::buildRoofStraightSkeletons(const BitArray& floorOwnedTiles, const Building& building, f32 zPos, VisualLog* visLog) {
    // Detect Edges

    const i32v2 dims(building.mAABB.dims.x, building.mAABB.dims.y);
    const ui32 totalTiles = dims.x * dims.y;
    std::vector<SsPtr> skeletons;

    BitArray checkedTiles;
    // Mark all unowned tiles as "checked"
    checkedTiles.setNOT(floorOwnedTiles);
    i32v2 cornerPos(0, 0);
   
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
        const i32 startY = cornerPos.y = startIndex / dims.y;
        Cartesian edge = Cartesian::SOUTH; // We are guaranteed theres always a bottom edge at this corner
        // If we do not have a free tile below, it means we are an interior tile on an already skeletoned segment, so continue
        if (cornerPos.y > 0 && floorOwnedTiles.getBit((cornerPos.y - 1) * dims.x + cornerPos.x)) {
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
            gridCell4x4.constructFrom2DBitArray(floorOwnedTiles, cornerPos.x, cornerPos.y, dims.x, dims.y);

            if (!gridCell4x4.data) {
                assert(false && "Must be nonzero or we walked off the edge or something");
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
                    const ui32v2& xy = building.mTileContainer->getTileXYOffset(startIndex);
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


void BuildingMesher::buildMeshFromStraightSkeleton(SsPtr iss, const Building& building, ProceduralMeshBuilder& meshBuilder, std::vector<RoofContourEdgeInfo>& contourEdges, const MaterialData& rawWoodMaterial, const MaterialData& shinglesMaterial, ui32 floor, f32 zPos, VisualLog* visLog) {
    // For bisector board placement
    std::unordered_set<std::pair<f32v3, f32v3>, f32v3pairhash> bisectorBoardPositions;
    bisectorBoardPositions.reserve(20);

    // ========================== Gables and Extrudes ===============================
    // Map gable and contour vertex points so we can move all connected verts
    std::unordered_map<f32v2, GableTargetPointInfo, f32v2hash> gableTargetPoints;
    std::unordered_map<f32v2, f32v3, f32v2hash> contourExtrudePositions;
    computeGablePointsAndExtrudePositions(building, floor, zPos, gableTargetPoints, contourExtrudePositions, iss, visLog);

    // Gather and reposition verts
    ui32 debugColorIndex = 0;
    for (auto&& it = iss->faces_begin(); it != iss->faces_end(); ++it) {
        auto&& he = it->halfedge();
        sHeightMap.clear();
        sRoofFacePoints.clear();
        bool isGable = false;
        // Loop through every edge of the SS poly and mark gables + cache points
        do {
            // Detect gable points
            auto&& gableIt = gableTargetPoints.find(f32v2(he->vertex()->point().x(), he->vertex()->point().y()));
            const bool isGablePoint = gableIt != gableTargetPoints.end();
            const bool isContourEdge = he->vertex()->is_contour() && he->next()->vertex()->is_contour();

            f32 x, y, t, h;
            if (gableIt != gableTargetPoints.end() && gableIt->second.isValidGable()) {
                // We are a gable pivot! Get our new position
                x = gableIt->second.pos.x;
                y = gableIt->second.pos.y;
                t = (f32)he->vertex()->time();
                h = t * ROOF_HEIGHT_MULT;
                isGable = he->is_bisector() && !he->is_inner_bisector() && he->next()->is_bisector() && !he->next()->is_inner_bisector();
                // Extrude along the gable direction
                f32v3 extrudeNormal(x - he->vertex()->point().x(), y - he->vertex()->point().y(), h * 3.0f); // 3.0f is trial and error
                extrudeNormal = normalize(extrudeNormal) * ROOF_EXTRUDE_DISTANCE;
                x += extrudeNormal.x;
                y += extrudeNormal.y;
            }
            else {
                x = (f32)he->vertex()->point().x();
                y = (f32)he->vertex()->point().y();
                t = (f32)he->vertex()->time();
                h = t * ROOF_HEIGHT_MULT;

                // Extrude contours
                if (he->vertex()->is_contour()) {
                    assert(t == 0.0f);
                    auto&& extrudeIt = contourExtrudePositions.find(f32v2(x, y));
                    if (extrudeIt != contourExtrudePositions.end()) {
                        // Create a column
                        // TODO: This column will intersect lower floors! Make it smarter
                        const f32v3 boardStart(x, y, -0.2f);
                        const f32v3 boardEnd(x, y, zPos);
                        meshBuilder.addBoardBetweenPoints(boardStart, boardEnd, f32v2(0.11f), rawWoodMaterial, f32v2(1.0f));
                        // Visual log
                        if (visLog) {
                            visLog->addLineBetweenPoints(boardStart, boardEnd, color4(0.0f, 1.0f, 1.0f, 1.0f));
                        }

                        const f32v3& extrudePosition = extrudeIt->second;
                        // Extrude
                        x = extrudePosition.x;
                        y = extrudePosition.y;
                        h = extrudePosition.z;
                    }
                }
            }

            // Edge boards
            if (!isContourEdge) {
                const f32v3 boardStart(x, y, zPos + h + ROOF_THICKNESS);
                // Check if next point is extruded
                f32 nextX = (f32)he->next()->vertex()->point().x();
                f32 nextY = (f32)he->next()->vertex()->point().y();
                f32 nextH = (f32)he->next()->vertex()->time() * ROOF_HEIGHT_MULT;
                auto&& extrudeIt = contourExtrudePositions.find(f32v2(nextX, nextY));
                if (extrudeIt != contourExtrudePositions.end()) {
                    const f32v3& extrudePosition = extrudeIt->second;
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
                    meshBuilder.addBoardBetweenPoints(boardStart, boardEnd, halfDims, rawWoodMaterial, f32v2(1.0f));
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
                auto&& extrudeIt = contourExtrudePositions.find(f32v2(nextVert.x(), nextVert.y()));
                assert(extrudeIt != contourExtrudePositions.end());
                // Figure out direction based on position offsets

                contourEdges.emplace_back(RoofContourEdgeInfo{
                    f32v3(x, y, h),
                    f32v3(thisVert.x(), thisVert.y(), 0.0f),
                    f32v3(extrudeIt->second.x, extrudeIt->second.y, extrudeIt->second.z),
                    f32v3(nextVert.x(), nextVert.y(), 0.0f)
                    });
            }

            if (sDebugOptions.mRoofDebug && isGablePoint) {
                DebugRenderer::drawWireQuad(f32v3(building.mAABB.pos.x + x, building.mAABB.pos.y + y, zPos + h) - f32v3(0.1f, 0.1f, 0.0f), f32v2(0.15f + debugColorIndex * 0.015f), DEBUG_COLOR_ARRAY[debugColorIndex], BUILDING_DEBUG_LIFETIME);
            }
            he = he->next();

        } while (he != it->halfedge());

        triangulateRoofFacePolygons(isGable, meshBuilder, building, shinglesMaterial, debugColorIndex, zPos);

        ++debugColorIndex;
        if (debugColorIndex >= DEBUG_COLOR_ARRAY_SIZE) debugColorIndex = 0;
    }
}

void BuildingMesher::triangulateRoofFacePolygons(bool isGable, ProceduralMeshBuilder& meshBuilder, const Building& building, const MaterialData& shinglesMaterial, ui32 debugColorIndex, f32 zPos) {
    // Triangulation only works on convex polygons so we will partition the potentially concave poly into
    // separate convex polygons
    // https://stackoverflow.com/questions/1832430/c-cgal-2d-delauny-triangulation-concave-shapes
    // 
    // Partition 
    CGAL::Partition_traits_2<K>::Polygon_2 concavePoly;
    for (auto&& pp : sRoofFacePoints) {

        if (pp.x() > 20000.0f || pp.y() > 20000.0f) {
            LOG_DEBUG("ERROR FACE VAL {} {}", pp.x(), pp.y());
        }

        concavePoly.push_back(pp);
    }
    std::list<CGAL::Partition_traits_2<K>::Polygon_2> convexPolygonList;
    if (!isGable) {
        // Partition poly into seperate convex pieces
        CGAL::optimal_convex_partition_2(concavePoly.vertices_begin(), concavePoly.vertices_end(), std::back_inserter(convexPolygonList));
    }
    else {
        // Gables are never concave ( I THINK )
        convexPolygonList.push_back(concavePoly);
    }
    
    for (auto&& convexPoly : convexPolygonList) {

        // Get triangle points
        f32v2 points[3];
        if (convexPoly.size() == 3) {
            // Simple triangles are just directly copied
            for (int i = 0; i < 3; ++i) {
                points[i].x = (f32)convexPoly.vertex(i).x();
                points[i].y = (f32)convexPoly.vertex(i).y();
            }
            // Add to mesh
            addRoofTriangle(meshBuilder, points, building, shinglesMaterial, debugColorIndex, zPos);
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
                addRoofTriangle(meshBuilder, points, building, shinglesMaterial, debugColorIndex, zPos);
            }
        }
    }
}

void BuildingMesher::addRoofTriangle(
    ProceduralMeshBuilder& meshBuilder,
    const f32v2 points[3],
    const Building& building,
    const MaterialData& materialData,
    ui32 debugColorIndex,
    f32 zPos
) {
    const f32v2 buildingCenter = f32v2(building.mAABB.pos) + f32v2(building.mAABB.dims) * 0.5f;

    StaticModelVertex verts[3];
    for (int i = 0; i < 3; ++i) {
        const f32 x = points[i].x;
        const f32 y = points[i].y;

        const f32 deg1 = sHeightMap[f32v2(x, y)];
        verts[i].pos = f32v3(x, y, zPos + deg1 + ROOF_THICKNESS);

        /* if (sDebugOptions.mRoofDebug) {
             DebugRenderer::drawWireQuad(verts[i].pos - f32v3(0.1f, 0.1f, 0.0f), f32v2(0.15f + debugColorIndex * 0.015f), DEBUG_COLOR_ARRAY[debugColorIndex], BUILDING_DEBUG_LIFETIME);
         }*/

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
        // Different texturing for nearly vertical polygons
        for (int i = 0; i < 3; ++i) {
            verts[i].normalPacked = compressedNormal;
            verts[i].tangentPacked = compressedTangent;
            f32v2 uvs((verts[i].pos[uvAxis.x]) * UV_SCALE, verts[i].pos.z * UV_SCALE * AXIS_V_DIR_FROM_CARTESIAN[e_cast(dir)]);
            verts[i].uvsPacked = PackUVs(uvs);
        }
    }
    /* if (sDebugOptions.mRoofDebug) {
        DebugRenderer::drawWireTriangle(verts[0].pos, verts[1].pos, verts[2].pos, DEBUG_COLOR_ARRAY[debugColorIndex], BUILDING_DEBUG_LIFETIME);
    }*/
    meshBuilder.addTriangle(verts, materialData, false);
}

void BuildingMesher::meshRoofContourEdges(const std::vector<RoofContourEdgeInfo>& contourEdges, const Building& building, ProceduralMeshBuilder& meshBuilder, const MaterialData& shinglesMaterial, const MaterialData& rawWoodMaterial, f32 zPos, VisualLog* visLog) {
    for (auto&& edge : contourEdges) {
        if (edge.v1 == edge.parent1 && edge.v2 == edge.parent2) {
            // Ignore cases where we meld into the wall due to collision
            return;
        }

        f32v3 first(edge.v1.x, edge.v1.y, edge.v1.z + zPos);
        f32v3 second(edge.v2.x, edge.v2.y, edge.v2.z + zPos);
        CubeFacing axis;
        if (first.x < second.x) {
            axis = CubeFacing::FRONT;
        }
        else if (first.x > second.x) {
            axis = CubeFacing::BACK;
        }
        else if (first.y < second.y) {
            axis = CubeFacing::RIGHT;
        }
        else if (first.y > second.y) {
            axis = CubeFacing::LEFT;
        }
        f32v3 points[4];
        // Side
        // TODO: Z fighting here when we have no overhang due to collisions with AABB edge
        points[0] = first;
        points[1] = second;
        points[2] = second + f32v3(0.0f, 0.0f, ROOF_THICKNESS);
        points[3] = first + f32v3(0.0f, 0.0f, ROOF_THICKNESS);
        meshBuilder.addQuadBetweenPoints(points, shinglesMaterial, f32v2(1.0f), COLOR_WHITE, false);
        // Bottom
        points[0] = second;
        points[1] = first;
        points[2] = f32v3(edge.parent1.x, edge.parent1.y, zPos - ROOF_THICKNESS);
        points[3] = f32v3(edge.parent2.x, edge.parent2.y, zPos - ROOF_THICKNESS);
        meshBuilder.addQuadBetweenPoints(points, shinglesMaterial, f32v2(1.0f), COLOR_WHITE, false);

        // Compute edge dir
        Cartesian dir;
        f32 xDiff = abs(edge.v2.x - edge.v1.x);
        f32 yDiff = abs(edge.v2.y - edge.v1.y);
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

        // Step along the edge and add extruded board pieces
        // Random dims
        constexpr f32 BOARD_SIZE_VARIANCE = 0.04f;
        constexpr f32 BOARD_GAP_VARIANCE = 0.1f;
        constexpr f32 BOARD_ANGLE_VARIANCE = 0.15f;
        constexpr f32 BOARD_LENGTH_VARIANCE = 0.15f;
        constexpr f32 BOARDS_PER_METER = 2;
        constexpr f32 BOARD_LENGTH_BASE = 0.01f + ROOF_EXTRUDE_DISTANCE;
        const f32v3 diff = second - first;
        const f32 distance = glm::length(diff);
        const f32v3 iterNormal = diff / distance;
        const f32v3& edgeNormal = CARTESIAN_NORMALS_3D[e_cast(dir)];
        const int boardCount = (int)round(distance * BOARDS_PER_METER);
        const f32 boardGapSize = distance / (boardCount + 1);
        const f32v3 start = first - edgeNormal * ROOF_EXTRUDE_DISTANCE;
        // TODO: check for intsersections with other rooms
        for (int i = 1; i <= boardCount; ++i) {
            // Get dims
            const f32v2 boardHalfDims = f32v2(
                0.03f + randFromf32v3(first, i << 3) * BOARD_SIZE_VARIANCE,
                0.03f + randFromf32v3(second, i << 3) * BOARD_SIZE_VARIANCE
            );
            const f32v3 startWithBoardOffset = f32v3(start.x, start.y, start.z + 0.2 - boardHalfDims.y);
            // Extruded boards with random offset variance
            const f32v3 offset = iterNormal * (i * boardGapSize + (randFromf32v3(startWithBoardOffset, i << 4) - 0.5f) * BOARD_GAP_VARIANCE);
            const f32v3 p1 = startWithBoardOffset + offset;
            const f32 boardLength = BOARD_LENGTH_BASE + (randFromf32v3(offset, i << 2) - 0.5f) * BOARD_LENGTH_VARIANCE;
            const f32v3 p2 = p1 + edgeNormal * boardLength - f32v3(0.0f, 0.0f, ROOF_HEIGHT_MULT * (0.8f + (randFromf32v3(p1, i) - 0.5f) * BOARD_ANGLE_VARIANCE));
            meshBuilder.addBoardBetweenPoints(p1, p2, boardHalfDims, rawWoodMaterial, f32v2(1.0f));
        }
    }
}

void BuildingMesher::meshRoomCeilings(const Building& building, ProceduralMeshBuilder& meshBuilder, const MaterialData& rawWoodMaterial) {
    constexpr f32 CEILING_THICKNESS = 0.05f;
    const i32AABB3& aabb = building.mAABB;
    const TileContainer& tileContainer = *building.mTileContainer;
    const BitArray& ownedTiles = tileContainer.getOwnedTiles();
    TileIndex index = 0;
    const f32v4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);
    for (ui32 z = 0; z < building.mTileContainer->getDims().z; ++z) {
        for (ui32 y = 0; y < aabb.dims.y; ++y) {
            for (ui32 x = 0; x < aabb.dims.x; ++x, ++index) {
                if (ownedTiles.getBit(index)) {
                    // If were at the top or the tile above us is outside the interior, or its interior and a non air tile above us, mesh a ceiling
                    const TileIndex aboveIndex = index + aabb.dims.x * aabb.dims.y;
                    if (z == tileContainer.getDims().z - 1 || // If were at the top
                        !ownedTiles.getBit(aboveIndex) // Or tile above us is an exterior tile
                        /*|| !tileContainer.getTileAt(aboveIndex).isEmpty()*/) { // Or its an interior tile and not empty
                        // Mesh ceiling
                        f32v3 startPos(x, y, tileContainer.getFloorHeight() * (z + 1) - CEILING_THICKNESS);
                        meshBuilder.addAxisAlignedQuad(startPos, f32v2(1.0f), CubeFacing::BOTTOM, rawWoodMaterial, uvRect, COLOR_WHITE);
                        // TODO: Cull edges appropriately
                        meshBuilder.addAxisAlignedQuad(startPos, f32v2(1.0f, CEILING_THICKNESS), CubeFacing::LEFT, rawWoodMaterial, uvRect, COLOR_WHITE);
                        meshBuilder.addAxisAlignedQuad(startPos, f32v2(1.0f, CEILING_THICKNESS), CubeFacing::RIGHT, rawWoodMaterial, uvRect, COLOR_WHITE);
                        meshBuilder.addAxisAlignedQuad(startPos, f32v2(1.0f, CEILING_THICKNESS), CubeFacing::FRONT, rawWoodMaterial, uvRect, COLOR_WHITE);
                        meshBuilder.addAxisAlignedQuad(startPos, f32v2(1.0f, CEILING_THICKNESS), CubeFacing::BACK, rawWoodMaterial, uvRect, COLOR_WHITE);
                    }
                }
            }
        }
    }
}

void BuildingMesher::meshRoomUndercarriage(const Building& building, ProceduralMeshBuilder& meshBuilder, const MaterialData& rawWoodMaterial) {
    const i32AABB3& aabb = building.mAABB;
    const TileContainer& tileContainer = *building.mTileContainer;
    const ui32 floorStride = aabb.dims.x * aabb.dims.y;
    const BitArray& ownedTiles = tileContainer.getOwnedTiles();
    TileIndex index = 0;
    const f32v4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);
    for (i32 z = 0; z < building.mTileContainer->getDims().z; ++z) {
        for (i32 y = 0; y < aabb.dims.y; ++y) {
            for (i32 x = 0; x < aabb.dims.x; ++x, ++index) {
                // Check if we should start supports here, i.e. below us is outside the building
                if (ownedTiles.getBit(index) && (z == 0 || !ownedTiles.getBit(index - floorStride))) {
                    // Place floor tiles and increment X
                    const ui32 startX = x;
                    do {
                        // Disabled now since floor takes care of it
                       // const f32v3 floorPos(x, y, tileContainer.getFloorHeight() * z - 0.0001f);
                       // meshBuilder.addAxisAlignedQuad(floorPos, f32v2(1.0f), CubeFacing::BOTTOM, rawWoodMaterial, uvRect, COLOR_WHITE);
                    } while (++x < aabb.dims.x && ownedTiles.getBit(++index));
                    // TODO: ADD BOARD
                    const f32 boardThickness = 0.1f;
                    f32v3 startPos(startX, y + 0.5f, tileContainer.getFloorHeight() * z - boardThickness - 0.0001f);
                    f32v3 endPos = startPos + f32v3(x - startX, 0.0f, 0.0f);
                    meshBuilder.addBoardBetweenPoints(startPos, endPos, f32v2(boardThickness), rawWoodMaterial, f32v2(1.0f));
                }
            }
        }
    }
}

const i32v2 STAIR_DIR_OFFSETS[CARTESIAN_COUNT] = {
    i32v2(0, 1), // SOUTH
    i32v2(1, 0), // WEST
    i32v2(0, 0), // EAST
    i32v2(0, 0), // NORTH
};

const f32v2 STAIR_DIR_DIMS[CARTESIAN_COUNT] = {
    f32v2(1, 0.25), // SOUTH
    f32v2(0.25, 1), // WEST
    f32v2(0.25, 1), // EAST
    f32v2(1, 0.25), // NORTH
};
