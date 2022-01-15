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

BuildingMesher::BuildingMesher(ResourceManager& resourceManager) :
    mResourceManager(resourceManager)
{
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
    renderData.mRoofMeshDirty = false;

    sHeightMap.reserve(100);
    sPoints.reserve(100);

    if (!renderData.mRoofMesh) {
        renderData.mRoofMesh = std::make_unique<QuadMesh>();
        renderData.mRoofTriangleMesh = std::make_unique<TriangleMesh>();
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

        if (!code) {
            //assert(code); // Must be nonzero or we walked off the edge
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
        cornerPos += CARTESIAN_EDGE_DIRS_COUNTER_CLOCKWISE[enum_cast(edge)];

    } while (cornerPos.x != startX || cornerPos.y != startY);
    
    sCgalPoly.resize(numRoofVertices);
    // TODO: Resize
    for (ui32 i = 0; i < numRoofVertices; ++i) {
        sCgalPoly[i] = CgalPoint(sRoofVertices[i].x, sRoofVertices[i].y);
    }
    //assert(sCgalPoly.is_counterclockwise_oriented());

    SsPtr iss = CGAL::create_interior_straight_skeleton_2(sCgalPoly.vertices_begin(), sCgalPoly.vertices_end());
    f32v2 start = building.mAABB.pos;

    const SpriteData& spriteData = mResourceManager.getSprite("roof");
    TriangleMesh& triangleMesh = *renderData.mRoofTriangleMesh;


    for (auto&& it = iss->faces_begin(); it != iss->faces_end(); ++it) {
        auto&& he = it->halfedge();
        int kk = 0;
        sHeightMap.clear();
        sPoints.clear();
        do {
            f32 x = he->vertex()->point().x();
            f32 y = he->vertex()->point().y();
            sPoints.push_back(he->vertex()->point());
            const f32 h1 = he->vertex()->time() * ROOF_HEIGHT_MULT;
            sHeightMap[f32v2(x, y)] = h1;
            x = he->opposite()->vertex()->point().x();
            y = he->opposite()->vertex()->point().y();
            const f32 h2 = he->opposite()->vertex()->time() * ROOF_HEIGHT_MULT;
            sHeightMap[f32v2(x, y)] = h2;
            if (sDebugOptions.mRoofDebug) {
                f32v3 p1(start.x + x, start.y + y, building.mZPosRoof + h1);
                f32v3 p2(start.x + x, start.y + y, building.mZPosRoof + h2);
                DebugRenderer::drawLineBetweenPoints(p1, p2, color4(1.0f, 1.0f, 1.0f, 1.0f), 5000);
            }
            he = he->next();
            ++kk;

        } while (he != it->halfedge());

        Triangulation triangulation;
        triangulation.insert(sPoints.begin(), sPoints.end());

        const f32v2 buildingPos(building.mAABB.pos.x, building.mAABB.pos.y);
        const f32v2 buildingCenter = f32v2(building.mAABB.pos) + f32v2(building.mAABB.dims) * 0.5f;
        for (auto&& it = triangulation.all_faces_begin(); it != triangulation.all_faces_end(); ++it) {
            TriangleVertex verts[3];
            bool skipTriangle = false;
            for (int i = 0; i < 3; ++i) {
                f32 x = it->vertex(i)->point().x();
                f32 y = it->vertex(i)->point().y();
                if (isinf(x)) {
                    skipTriangle = true;
                }
                f32 deg1 = sHeightMap[f32v2(x,y)];

                verts[i].pos = f32v3(start.x + x, start.y + y, building.mZPosRoof + deg1);

                // Extrude bottom verts
                if (deg1 == 0.0f) {
                    f32v2 normal = glm::normalize(f32v2(verts[i].pos.x, verts[i].pos.y) - buildingCenter) * 0.3f;
                    verts[i].pos.x += normal.x;
                    verts[i].pos.y += normal.y;
                }

                if (sDebugOptions.mRoofDebug) {
                    DebugRenderer::drawWireQuad(verts[i].pos = - f32v3(0.1f, 0.1f, 0.0f), f32v2(0.2f), color4(1.0f, 0.0f, 1.0f, 1.0f), 5000);
                }

                verts[i].uvTiling = spriteData.uvs;
                verts[i].color = color4(1.0f, 1.0f, 1.0f, 1.0f);
                verts[i].atlasPage = spriteData.atlasPage;
            }

            if (!skipTriangle) {

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
                const ui32v2 uvAxis = AXIS_UV_LOOKUP_FROM_CARTESIAN[enum_cast(dir)];

                for (int i = 0; i < 3; ++i) {
                    verts[i].normal = normal;
                    verts[i].uvs.x = verts[i].pos[uvAxis.x] - buildingPos.x;
                    verts[i].uvs.y = verts[i].pos[uvAxis.y] - buildingPos.y;
                }

                triangleMesh.addTriangle(verts);
            }
        }
        
    }

    // Base of roof
    QuadMesh& mesh = *renderData.mRoofMesh;
    for (ui32 y = 0; y < aabb.dims.y; ++y) {
        for (ui32 x = 0; x < aabb.dims.x; ++x) {
            const ui32 index = y * aabb.dims.x + x;
            if (ownedTiles.getBit(index)) {
                f32v3 startPos(aabb.pos.x + x, aabb.pos.y + y, 3.0051f);
                mesh.addAxisAlignedQuad(startPos, f32v2(1.000f), f32v2(0.0f), CubeFacing::BOTTOM, spriteData.atlasPage, spriteData.uvs, COLOR_WHITE, false);
            }
        }
    }
    mesh.finishMesh(MeshDrawMode::STATIC);
    triangleMesh.finishMesh(MeshDrawMode::STATIC);
}
