#include "stdafx.h"
#include "BuildingMesher.h"

#include "city/Building.h"
#include "ResourceManager.h"

#include "DebugRenderer.h"

#include "util/IntersectionUtil.h"

#include "rendering/QuadMesh.h"
#include "rendering/TriangleMesh.h"

#include "options/DebugOptions.h"

//CGal
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Polygon_2.h>
#include <CGAL/create_straight_skeleton_2.h>
#include <boost/shared_ptr.hpp>
#include <CGAL/Triangulation_2.h>
#include <CGAL/partition_2.h>
#include <CGAL/Partition_traits_2.h>
//#include <CGAL/Delaunay_triangulation_2.h>
//#include <CGAL/Regular_triangulation_2.h>

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

typedef CGAL::Exact_predicates_inexact_constructions_kernel K;
typedef K::Point_2                   CgalPoint;
typedef CGAL::Polygon_2<K>           Polygon_2;
typedef CGAL::Straight_skeleton_2<K> StraightSkeleton;
typedef boost::shared_ptr<StraightSkeleton> SsPtr;
typedef CGAL::Triangulation_2<K>         Triangulation;
typedef Triangulation::Vertex_circulator Vertex_circulator;
typedef Triangulation::Point             TriangulationPoint;

class f32v2HashFunction {
public:
    size_t operator()(const f32v2& f) const
    {
        return (size_t)std::hash<f32>{}(f.x) ^ (size_t)std::hash<f32>{}(f.y * 127.0f);
    }
};

thread_local std::unordered_map<f32v2, f32, f32v2HashFunction> sHeightMap;
thread_local std::vector<TriangulationPoint> sPoints;
thread_local Polygon_2 sCgalPoly;


struct RoofSkeletonVertex;

enum Corners {
    CORNER_TOP_LEFT = 0,
    CORNER_TOP_RIGHT = 1,
    CORNER_BOTTOM_LEFT = 2,
    CORNER_BOTTOM_RIGHT = 3
};

BuildingMesher::BuildingMesher() {
    // Zero table
    for (ui32 i = 0; i < ROOF_VERTEX_CORNER_TABLE_SIZE; ++i) {
        mCornerNextEdgeLookupTable[i] = Cartesian::INVALID;
    }
    // Set up corners shapes, we move counter clockwise always
    // 0 1
    // 0 0
    mCornerNextEdgeLookupTable[0b0100] = Cartesian::DOWN;
    mCornerTypeLookupTable[0b0100] = CornerWinding::TOP_RIGHT;
    // 1 0
    // 1 1
    mCornerNextEdgeLookupTable[0b1011] = Cartesian::RIGHT;
    mCornerTypeLookupTable[0b1011] = CornerWinding::BOTTOM_LEFT;
    // 1 0
    // 0 0
    mCornerNextEdgeLookupTable[0b1000] = Cartesian::RIGHT;
    mCornerTypeLookupTable[0b1000] = CornerWinding::TOP_LEFT;
    // 0 1
    // 1 1
    mCornerNextEdgeLookupTable[0b0111] = Cartesian::UP;
    mCornerTypeLookupTable[0b0111] = CornerWinding::BOTTOM_RIGHT;
    // 0 0
    // 1 0
    mCornerNextEdgeLookupTable[0b0010] = Cartesian::UP;
    mCornerTypeLookupTable[0b0010] = CornerWinding::BOTTOM_LEFT;
    // 1 1
    // 0 1
    mCornerNextEdgeLookupTable[0b1101] = Cartesian::LEFT;
    mCornerTypeLookupTable[0b1101] = CornerWinding::TOP_RIGHT;
    // 0 0
    // 0 1
    mCornerNextEdgeLookupTable[0b0001] = Cartesian::LEFT;
    mCornerTypeLookupTable[0b0001] = CornerWinding::BOTTOM_RIGHT;
    // 1 1
    // 1 0
    mCornerNextEdgeLookupTable[0b1110] = Cartesian::DOWN;
    mCornerTypeLookupTable[0b1110] = CornerWinding::TOP_LEFT;
    // Diagonal edge cases
    // 1 0
    // 0 1
    mCornerNextEdgeLookupTable[0b1001] = Cartesian::NONE;
    mCornerNextEdgeLookupTable[0b0110] = Cartesian::NONE;
}

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

// TODO Move
class f32v2hash {
public:
    size_t operator()(const f32v2& v) const {
        size_t h;
        boost::hash_combine(h, v.x);
        boost::hash_combine(h, v.y);
        return h;
    }
};

//http://wscg.zcu.cz/wscg2003/Papers_2003/G67.pdf
// Step 1: Construct the Straight Skeleton http://citeseerx.ist.psu.edu/viewdoc/download;jsessionid=0A7158816778A842AE8ABC0A752CD92D?doi=10.1.1.131.7175&rep=rep1&type=pdf
// Step 2: Determine the distance, d, each vertex is from its supporting edge.
// Step 3 : Perform a boundary walk, using the least interior angle, to determine the roof planes.
// Step 4 : Raise the vertices according to their distance from the supporting edge.
void BuildingMesher::buildRoofMesh(const Building& building)
{
    const ui32AABB2& aabb = building.mAABB;
    const BitArray& ownedTiles = building.mOwnedTilesInAABB;
    constexpr f32 ROOF_HEIGHT_MULT = 0.3f;
    BuildingRenderData& renderData = building.mRenderData;
    // TODO: Not right

    sHeightMap.reserve(100);
    sPoints.reserve(100);

    renderData.mMeshDirty = false;
    if (!renderData.mMesh) {
        renderData.mMesh = std::make_unique<BuildingMesh>();
    }

    // Detect Edges
    ui32 numRoofVertices = 0;
    
    // Find first corner
    ui32 index = 0;
    // TODO: This could be checked byte by byte for nonzero then extract most significant bit
    while (!ownedTiles.getBit(index)) {
        ++index;
    }
    ui32 startX = index % aabb.dims.x;
    ui32 startY = index / aabb.dims.y;
    i32v2 cornerPos(startX, startY);
    Cartesian edge = Cartesian::DOWN; // We are guarenteed theres always a bottom edge at this corner

    // Debug output
    //std::cout << "GENERATING ROOF\n";
    //ownedTiles.debugPrint(aabb.dims.x, aabb.dims.y);

    sRoofVertices[numRoofVertices++] = cornerPos;
    // First edge always goes right
    ++cornerPos.x;
    do {
        index = cornerPos.y * aabb.dims.x + cornerPos.x;
        ui8 corners[4];
        corners[CORNER_TOP_LEFT] = (cornerPos.x == 0 || cornerPos.y == aabb.dims.y) ? 0 : ownedTiles.getBit(index - 1);
        corners[CORNER_TOP_RIGHT] = (cornerPos.x == aabb.dims.x || cornerPos.y == aabb.dims.y) ? 0 : ownedTiles.getBit(index);
        corners[CORNER_BOTTOM_LEFT] = (cornerPos.x == 0 || cornerPos.y == 0) ? 0 : ownedTiles.getBit(index - 1 - aabb.dims.x);
        corners[CORNER_BOTTOM_RIGHT] = (cornerPos.x == aabb.dims.x || cornerPos.y == 0) ? 0 : ownedTiles.getBit(index - aabb.dims.x);
        ui8 code = corners[0] << 3;
        code |= corners[1] << 2;
        code |= corners[2] << 1;
        code |= corners[3];

        // TODO: FIX THIS LOGIC
        if (!code) {
            assert(code); // Must be nonzero or we walked off the edge
            return;
        }
        Cartesian nextEdge = mCornerNextEdgeLookupTable[code];
        if (nextEdge == Cartesian::NONE) {
            std::cout << "Edge detection failed due to bad corner\n";
            assert(false); // NEED TO IMPLEMENT EDGE DETECT
        }
        else if (nextEdge != Cartesian::INVALID) {
            const CornerWinding winding = mCornerTypeLookupTable[code];
            assert(edge != nextEdge);
            edge = nextEdge;
            // New vertex and connect previous
            assert(numRoofVertices < MAX_ROOF_VERTICES);
            sRoofVertices[numRoofVertices] = cornerPos;
            ++numRoofVertices;
        }
        cornerPos += CARTESIAN_EDGE_DIRS_COUNTER_CLOCKWISE[e_cast(edge)];

    } while (cornerPos.x != startX || cornerPos.y != startY);
    
    sCgalPoly.resize(numRoofVertices);
    // TODO: Resize
    for (ui32 i = 0; i < numRoofVertices; ++i) {
        sCgalPoly[i] = CgalPoint(sRoofVertices[i].x, sRoofVertices[i].y);
    }
    //assert(sCgalPoly.is_counterclockwise_oriented());

    SsPtr iss = CGAL::create_interior_straight_skeleton_2(sCgalPoly.vertices_begin(), sCgalPoly.vertices_end());
    f32v2 start = building.mAABB.pos;

    const SpriteData& spriteData = Services::ResourceManager::ref().getSprite("roof");
    BuildingMesh& buildingMesh = *renderData.mMesh;

    // Maps points to new points, to move gable vertices
    std::unordered_map<f32v2, f32v2, f32v2hash> gableTargetPoints;
    gableTargetPoints.reserve(10);
    // Compute gable target points
    for (auto&& it = iss->faces_begin(); it != iss->faces_end(); ++it) {
        auto&& he = it->halfedge();
        do {
            const bool isGablePoint = he->is_bisector() && !he->is_inner_bisector() && he->next()->is_bisector() && !he->next()->is_inner_bisector();
            if (isGablePoint) {
                f32v2 gableTarget;
                gableTarget.x = (he->prev()->vertex()->point().x() + he->next()->vertex()->point().x()) / 2.0f;
                gableTarget.y = (he->prev()->vertex()->point().y() + he->next()->vertex()->point().y()) / 2.0f;
                gableTargetPoints[f32v2(he->vertex()->point().x(), he->vertex()->point().y())] = gableTarget;
            }
            he = he->next();
        } while (he != it->halfedge());
    }

    // Gather and reposition verts
    ui32 debugColorIndex = 0;
    for (auto&& it = iss->faces_begin(); it != iss->faces_end(); ++it) {
        auto&& he = it->halfedge();
        sHeightMap.clear();
        sPoints.clear();
        bool isGable = false;
        do {
            // Detect gable points
            auto&& gableIt = gableTargetPoints.find(f32v2(he->vertex()->point().x(), he->vertex()->point().y()));
            const bool isGablePoint = gableIt != gableTargetPoints.end();

            //he->is_border doesnt work but if v1 and v2 time are both 0, it is a contour edge
            f32 x, y, t, h;
            if (gableIt != gableTargetPoints.end()) {
                // We are a gable pivot! Get our new position

                x = gableIt->second.x;
                y = gableIt->second.y;
                t = he->vertex()->time();
                h = t * ROOF_HEIGHT_MULT;
                isGable = true;
            }
            else {
                x = he->vertex()->point().x();
                y = he->vertex()->point().y();
                t = he->vertex()->time();
                h = t * ROOF_HEIGHT_MULT;

                if (he->vertex()->is_contour() && sDebugOptions.mRoofDebug) {
                    // Extrude outward
                    DebugRenderer::drawWireQuad(f32v3(start.x + x, start.y + y, building.mZPosRoof + h) - f32v3(0.1f, 0.1f, 0.0f), f32v2(0.15f + debugColorIndex * 0.015f), DEBUG_COLOR_ARRAY[debugColorIndex], BUILDING_DEBUG_LIFETIME);
                }
            }
            sPoints.emplace_back(x, y);
            sHeightMap[f32v2(x, y)] = h;

            if (sDebugOptions.mRoofDebug && isGablePoint) {
                DebugRenderer::drawWireQuad(f32v3(start.x + x, start.y + y, building.mZPosRoof + h) - f32v3(0.1f, 0.1f, 0.0f), f32v2(0.15f + debugColorIndex * 0.015f), DEBUG_COLOR_ARRAY[debugColorIndex], BUILDING_DEBUG_LIFETIME);
            }
            he = he->next();

        } while (he != it->halfedge());

        // Triangulation only works on convex polygons so we will partition the potentially concave poly into
        // separate convex polygons
        // https://stackoverflow.com/questions/1832430/c-cgal-2d-delauny-triangulation-concave-shapes
  
        CGAL::Partition_traits_2<K>::Polygon_2 concavePoly;
        for (auto&& pp : sPoints) {
            concavePoly.push_back(pp);
        }
        std::list<CGAL::Partition_traits_2<K>::Polygon_2> polygonList;
        if (!isGable) {
            CGAL::optimal_convex_partition_2(concavePoly.vertices_begin(), concavePoly.vertices_end(), std::back_inserter(polygonList));
        }
        else {
            polygonList.push_back(concavePoly);
        }
        for (auto&& convexPoly : polygonList) {
            // Get triangle points
            f32v2 points[3];
            if (convexPoly.size() == 3) {
                for (int i = 0; i < 3; ++i) {
                    points[i].x = convexPoly.vertex(i).x();
                    points[i].y = convexPoly.vertex(i).y();
                }
                // Add to mesh
                addRoofTriangle(points, start, building, spriteData, debugColorIndex, buildingMesh);
            }
            else {
                // Triangulate higher order polys
                Triangulation triangulation;
                triangulation.insert(convexPoly.vertices_begin(), convexPoly.vertices_end());
                for (auto&& it = triangulation.all_faces_begin(); it != triangulation.all_faces_end(); ++it) {
                    for (int i = 0; i < 3; ++i) {
                        points[i].x = it->vertex(i)->point().x();
                        points[i].y = it->vertex(i)->point().y();
                    }
                    // Add to mesh
                    addRoofTriangle(points, start, building, spriteData, debugColorIndex, buildingMesh);
                }
            }
        }
        ++debugColorIndex;
        if (debugColorIndex >= DEBUG_COLOR_ARRAY_SIZE) debugColorIndex = 0;
    }

    // Base of roof
    for (ui32 y = 0; y < aabb.dims.y; ++y) {
        for (ui32 x = 0; x < aabb.dims.x; ++x) {
            const ui32 index = y * aabb.dims.x + x;
            if (ownedTiles.getBit(index)) {
                f32v3 startPos(aabb.pos.x + x, aabb.pos.y + y, building.mZPosRoof);
            //    buildingMesh.addAxisAlignedQuad(startPos, f32v2(1.000f), f32v2(0.0f), CubeFacing::BOTTOM, spriteData.atlasPage, spriteData.uvs, COLOR_WHITE, false);
            }
        }
    }
    buildingMesh.finishMesh(MeshDrawMode::STATIC);
}

void BuildingMesher::addRoofTriangle(
    const f32v2 points[3],
    f32v2& start,
    const Building& building,
    const SpriteData& spriteData,
    ui32 debugColorIndex,
    BuildingMesh& buildingMesh
) {
    const f32v2 buildingPos(building.mAABB.pos.x, building.mAABB.pos.y);
    const f32v2 buildingCenter = f32v2(building.mAABB.pos) + f32v2(building.mAABB.dims) * 0.5f;

    TriangleVertex verts[3];
    bool isInfiniteFace = false;
    for (int i = 0; i < 3; ++i) {
        f32 x = points[i].x;
        f32 y = points[i].y;
        if (isinf(x)) {
            // TODO: This means we are the convex edge, use it?
            isInfiniteFace = true;
            break;
        }
        f32 deg1 = sHeightMap[f32v2(x, y)];

        verts[i].pos = f32v3(start.x + x, start.y + y, building.mZPosRoof + deg1);

        // Extrude bottom verts
        if (deg1 == 0.0f) {
            f32v2 normal = glm::normalize(f32v2(verts[i].pos.x, verts[i].pos.y) - buildingCenter) * 0.3f;
            verts[i].pos.x += normal.x;
            verts[i].pos.y += normal.y;
        }

        /* if (sDebugOptions.mRoofDebug) {
             DebugRenderer::drawWireQuad(verts[i].pos - f32v3(0.1f, 0.1f, 0.0f), f32v2(0.15f + debugColorIndex * 0.015f), DEBUG_COLOR_ARRAY[debugColorIndex], BUILDING_DEBUG_LIFETIME);
         }*/

        verts[i].uvTiling = spriteData.uvs;
        verts[i].color = color4(1.0f, 1.0f, 1.0f, 1.0f);
        verts[i].atlasPage = spriteData.atlasPage;
    }

    // The infinite face is not needed for our representation
    if (!isInfiniteFace) {

        // Determine orientation
        const f32v3 o1 = verts[1].pos - verts[0].pos;
        const f32v3 o2 = verts[2].pos - verts[0].pos;
        const f32v3 normal = glm::normalize(glm::cross(o1, o2));
        Cartesian dir = Cartesian::LEFT;
        if (abs(normal.x) < 0.0001f) {
            if (normal.y > 0) {
                dir = Cartesian::UP;
            }
            else {
                dir = Cartesian::DOWN;
            }
        }
        else if (normal.x > 0) {
            dir = Cartesian::RIGHT;
        }

        // Determine how we get UVs
        const ui32v2 uvAxis = AXIS_UV_LOOKUP_FROM_CARTESIAN[e_cast(dir)];

        for (int i = 0; i < 3; ++i) {
            verts[i].normal = normal;
            verts[i].uvs.x = verts[i].pos[uvAxis.x] - buildingPos.x;
            verts[i].uvs.y = verts[i].pos[uvAxis.y] - buildingPos.y;
        }

        DebugRenderer::drawWireTriangle(verts[0].pos, verts[1].pos, verts[2].pos, DEBUG_COLOR_ARRAY[debugColorIndex], BUILDING_DEBUG_LIFETIME);
        buildingMesh.addTriangle(verts);
    }
}
