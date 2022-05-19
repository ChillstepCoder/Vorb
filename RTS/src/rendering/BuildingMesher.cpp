#include "stdafx.h"
#include "BuildingMesher.h"

#include "city/Building.h"
#include "ResourceManager.h"

#include "DebugRenderer.h"
#include "debugging/VisualLogger.h"

#include "util/IntersectionUtil.h"

#include "rendering/mesh/Mesh.h"
#include "rendering/mesh/MeshBuilder.h"

#include "options/DebugOptions.h"

#include "Random.h"

#include "tile/TileHandle.h"
#include "resources/TileRepository.h"
#include "rendering/mesh/TileMeshBuilderMethods.h"

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

// TODO Move
class f32v2hash {
public:
    size_t operator()(const f32v2& v) const {
        size_t seed = 0;
        boost::hash_combine(seed, v.x);
        boost::hash_combine(seed, v.y);
        return seed;
    }
};

class f32v3hash {
public:
    size_t operator()(const f32v3& v) const {
        size_t seed = 0;
        boost::hash_combine(seed, v.x);
        boost::hash_combine(seed, v.y);
        boost::hash_combine(seed, v.z);
        return seed;
    }
};

class f32v3pairhash {
public:
    size_t operator()(const std::pair<f32v3, f32v3>& v) const {
        size_t seed = 0;
        boost::hash_combine(seed, v.first.x);
        boost::hash_combine(seed, v.first.y);
        boost::hash_combine(seed, v.first.z);
        boost::hash_combine(seed, v.second.x);
        boost::hash_combine(seed, v.second.y);
        boost::hash_combine(seed, v.second.z);
        return seed;
    }
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
f32 AXIS_V_DIR_FROM_CARTESIAN[4] = {
    -1.0f, // Cartesian::DOWN
    -1.0f, // Cartesian::LEFT
    1.0f, // Cartesian::RIGHT
    1.0f, // Cartesian::UP
};

//http://wscg.zcu.cz/wscg2003/Papers_2003/G67.pdf
// Step 1: Construct the Straight Skeleton http://citeseerx.ist.psu.edu/viewdoc/download;jsessionid=0A7158816778A842AE8ABC0A752CD92D?doi=10.1.1.131.7175&rep=rep1&type=pdf
// Step 2: Determine the distance, d, each vertex is from its supporting edge.
// Step 3 : Perform a boundary walk, using the least interior angle, to determine the roof planes.
// Step 4 : Raise the vertices according to their distance from the supporting edge.

void computeGablePointsAndExtrudePositions(std::unordered_map<f32v2, f32v2, f32v2hash>& gableTargetPoints, std::unordered_map<f32v2, f32v3, f32v2hash>& contourExtrudePositions, SsPtr iss) {
    gableTargetPoints.reserve(10);
    contourExtrudePositions.reserve(30);
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
            else if (he->vertex()->is_contour() && he->is_bisector()) {

                // Contour corners that we will extrude along the bisector
                // Mark for later extrusions
                const auto& oppositePoint = he->opposite()->vertex()->point();
                const auto& thisPoint = he->vertex()->point();
                const float h = he->vertex()->time() * ROOF_HEIGHT_MULT;
                f32v3 extrudePosition(thisPoint.x() - oppositePoint.x(), thisPoint.y() - oppositePoint.y(), h - he->opposite()->vertex()->time() * ROOF_HEIGHT_MULT);
                extrudePosition = normalize(extrudePosition) * ROOF_EXTRUDE_DISTANCE + f32v3(thisPoint.x(), thisPoint.y(), 0.0f);
                contourExtrudePositions[f32v2(thisPoint.x(), thisPoint.y())] = extrudePosition;
            }
            he = he->next();
        } while (he != it->halfedge());
    }
}

f32 randFromf32v3(const f32v3& x, ui64 additional) {
    return Random::getThreadSafef((ui64)f32v3hash()(x) + additional);
}

void BuildingMesher::buildMesh(const Building& building) {
    PreciseTimer timer;

    // Debug log
    VisualLog* visLog = VisualLogger::tryGetNewVisualLog("building");

    // TODO: ASYNC
    MeshBuilder meshBuilder(false);
    constexpr ui32 RESERVE_VERT_COUNT = 10000; // Average size
    meshBuilder.reserveVertexCount(RESERVE_VERT_COUNT);
    meshBuilder.reserveIndexCount(RESERVE_VERT_COUNT * 1.5f); // 1.5 is approx

    const ui32AABB2& aabb = building.mAABB;
    const BitArray& ownedTiles = building.mInteriorTilesInAABB;
    BuildingRenderData& renderData = building.mRenderData;
    const TileContainer& tileContainer = building.mTileContainer;

    // Textures
    const SubTexture& shinglesTexture = Services::ResourceManager::ref().getTexture("roof");
    const SubTexture& rawWoodTexture = Services::ResourceManager::ref().getTexture("raw_wood_dark");

    // TODO: Not right
    sHeightMap.reserve(100);
    sRoofFacePoints.reserve(100);

    // ========================== Mesh Tiles ===============================
    meshTiles(building, meshBuilder);

    renderData.mMeshDirty = false;
    if (!renderData.mMesh) {
        renderData.mMesh = std::make_unique<Mesh>();
    }

    // ========================== Straight Skeleton ===============================
    if (visLog) visLog->nextStep("Straight skeleton");

    const ui32 floorCount = tileContainer.getDims().z;
    const ui32 floorTileCount = building.mAABB.dims.y * building.mAABB.dims.x;
    for (ui32 floor = 0; floor < floorCount; ++floor) {
        const f32 zPos = building.mZPosFloor + (floor + 1.0f) * tileContainer.getFloorHeight();
        // TODO: Replace bitarray with bool array
        BitArray roofedTiles;
        roofedTiles.resizeAndZero(building.mAABB.dims.x * building.mAABB.dims.y);
        for (ui32 y = 0; y < building.mAABB.dims.y; ++y) {
            for (ui32 x = 0; x < building.mAABB.dims.x; ++x) {
                const ui32 floorBitIndex = y * building.mAABB.dims.x + x;
                const ui32 buildingBitIndex = floor * floorTileCount + floorBitIndex;
                // If we own this tile, and above us is clear, we are a roofed tile
                if (building.mInteriorTilesInAABB.getBit(buildingBitIndex) &&
                    (floor == floorCount - 1 || !building.mInteriorTilesInAABB.getBit(buildingBitIndex + floorTileCount))) {
                    roofedTiles.setBitTo(floorBitIndex, true);
                }
            }
        }

        // Generate a list of straight skeletons
        std::vector<SsPtr> iss = buildRoofStraightSkeletons(roofedTiles, building, mCornerNextEdgeLookupTable, mCornerTypeLookupTable, zPos, visLog);
        std::vector<RoofContourEdgeInfo> contourEdges;
        contourEdges.reserve(20);

        // Mesh each individual straight skeleton
        for (auto& ss : iss) {

            buildMeshFromStraightSkeleton(ss, building, meshBuilder, contourEdges, rawWoodTexture, shinglesTexture, zPos);

            // ========================== Contours and extruded side boards ===============================
            meshRoofContourEdges(contourEdges, building, meshBuilder, shinglesTexture, rawWoodTexture, zPos);
            contourEdges.clear();
        }
    }

    // ========================== Flat base ===============================
    for (ui32 y = 0; y < aabb.dims.y; ++y) {
        for (ui32 x = 0; x < aabb.dims.x; ++x) {
            const ui32 index = y * aabb.dims.x + x;
            if (ownedTiles.getBit(index)) {
                f32v3 startPos(x, y, building.mZPosFloor + tileContainer.getFloorHeight());
                meshBuilder.addAxisAlignedQuad(startPos, f32v2(1.000f), CubeFacing::BOTTOM, rawWoodTexture, rawWoodTexture.mUvRect, COLOR_WHITE);
            }
        }
    }

    meshBuilder.finishMesh(*renderData.mMesh, MeshDrawMode::STATIC);

    if (visLog) visLog->finish();

    std::cout << "BUILT ROOF MESH IN " << timer.stop() << " ms\n";
}

void BuildingMesher::meshTiles(const Building& building, MeshBuilder& meshBuilder) {
    const ui32v3& tileDims = building.mTileContainer.getDims();
    for (ui32 z = 0; z < tileDims.z; ++z) {
        for (ui32 y = 0; y < tileDims.y; ++y) {
            for (ui32 x = 0; x < tileDims.x; ++x) {
                TileIndex index = building.mTileContainer.getTileIndexFromXYZOffset(x, y, z);
                const Tile& tile = building.mTileContainer.getTileAt(index);
                const f32 baseZPosition = tile.getBaseZPositionUncompressedThreadSafe();
                for (int layerIndex = 0; layerIndex < TILE_LAYER_COUNT; ++layerIndex) {
                    TileID layerTile = tile.getLayersThreadSafe()[layerIndex];
                    if (layerTile == TILE_ID_NONE) {
                        continue;
                    }

                    const TileData& tileData = TileRepository::getTileData(layerTile);
                    const SubTexture& texture = tileData.texture;

                    // Tile mesh
                    // Flora mesh ONLY
                    if (tileData.shape == TileShape::THIN) {
                        assert(false); // Unsupported
                    }
                    else if (tileData.shape == TileShape::BLOCK) {
                        TileMeshBuilderMethods::addBlock(meshBuilder, building.mZPosFloor + z * building.mTileContainer.getFloorHeight(), f32v2(x, y), TileHandle(&building.mTileContainer, index), tileData);
                    }
                    else if (tileData.shape == TileShape::FLOOR) {

                        //TileMeshBuilderMethods::addFloor(*quadMeshBuilder, (TileFloor)floor, f32v2(x, y), heightData, tileData, index, chunk, floor == TILE_FLOOR_GROUND);
                    }
                }
            }
        }
    }
}

std::vector<SsPtr> BuildingMesher::buildRoofStraightSkeletons(const BitArray& ownedTiles, const Building& building, Cartesian* mCornerNextEdgeLookupTable, CornerWinding* mCornerTypeLookupTable, f32 zPos, VisualLog* visLog) {
    // Detect Edges
    const ui32AABB2& aabb = building.mAABB;
    std::vector<SsPtr> skeletons;

    BitArray checkedTiles;
    checkedTiles.resizeAndZero(ownedTiles.getNumBits());

    // Find first corner
    ui32 index = 0;
    // Get multiple straight skeletons
    while (true) {
        ui32 numRoofVertices = 0;

        // TODO: This could be checked byte by byte for nonzero then extract most significant bit?
        while (checkedTiles.getBit(index) || !ownedTiles.getBit(index)) {
            ++index;
            if (index >= ownedTiles.getNumBits()) {
                // There are no roof tiles
                return skeletons;
            }
        }

        checkedTiles.setBitTo(index, true);

        ui32 startX = index % aabb.dims.x;
        ui32 startY = index / aabb.dims.y;
        i32v2 cornerPos(startX, startY);
        Cartesian edge = Cartesian::DOWN; // We are guaranteed theres always a bottom edge at this corner
        // If we do not have a free tile below, it means we are an interior tile on an already skeletoned segment, so continue
        if (cornerPos.y > 0 && ownedTiles.getBit((cornerPos.y - 1) * aabb.dims.x + cornerPos.x)) {
            continue;
        }

        // Debug output
        //std::cout << "GENERATING ROOF\n";
        //ownedTiles.debugPrint(aabb.dims.x, aabb.dims.y);

        sRoofVertices[numRoofVertices++] = cornerPos;
        // First edge always goes right
        ++cornerPos.x;
        do {
            index = cornerPos.y * aabb.dims.x + cornerPos.x;

            // Visual log
            if (visLog) {
                const ui32v2& xy = building.mTileContainer.getTileXYOffset(index);
                visLog->addWireQuad(f32v3(aabb.pos.x + xy.x, aabb.pos.y + xy.y, zPos), f32v2(1.0f), color4(1.0f, 1.0f, 1.0f, 0.75f));
                visLog->addFilledQuad(f32v3(aabb.pos.x + xy.x, aabb.pos.y + xy.y, zPos), f32v2(1.0f), color4(1.0f, 1.0f, 1.0f, 0.5f));
            }

            checkedTiles.setBitTo(index, true);

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
                return skeletons;
            }
            Cartesian nextEdge = mCornerNextEdgeLookupTable[code];
            if (nextEdge == Cartesian::NONE) {
                std::cout << "Edge detection failed due to bad corner\n";
                assert(false); // NEED TO IMPLEMENT DIAGONAL EDGE DETECT
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


void BuildingMesher::buildMeshFromStraightSkeleton(SsPtr iss, const Building& building, MeshBuilder& meshBuilder, std::vector<RoofContourEdgeInfo>& contourEdges, const SubTexture& rawWoodTexture, const SubTexture& shinglesTexture, f32 zPos) {
    // For bisector board placement
    std::unordered_set<std::pair<f32v3, f32v3>, f32v3pairhash> bisectorBoardPositions;
    bisectorBoardPositions.reserve(20);

    // ========================== Gables and Extrudes ===============================
    // Map gable and contour vertex points so we can move all connected verts
    std::unordered_map<f32v2, f32v2, f32v2hash> gableTargetPoints;
    std::unordered_map<f32v2, f32v3, f32v2hash> contourExtrudePositions;
    computeGablePointsAndExtrudePositions(gableTargetPoints, contourExtrudePositions, iss);

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
            if (gableIt != gableTargetPoints.end()) {
                // We are a gable pivot! Get our new position
                x = gableIt->second.x;
                y = gableIt->second.y;
                t = he->vertex()->time();
                h = t * ROOF_HEIGHT_MULT;
                isGable = he->is_bisector() && !he->is_inner_bisector() && he->next()->is_bisector() && !he->next()->is_inner_bisector();
                // Extrude along the gable direction
                f32v3 extrudeNormal(x - he->vertex()->point().x(), y - he->vertex()->point().y(), h * 3.0f); // 3.0f is trial and error
                extrudeNormal = normalize(extrudeNormal) * ROOF_EXTRUDE_DISTANCE;
                x += extrudeNormal.x;
                y += extrudeNormal.y;
            }
            else {
                x = he->vertex()->point().x();
                y = he->vertex()->point().y();
                t = he->vertex()->time();
                h = t * ROOF_HEIGHT_MULT;

                // Extrude contours
                if (he->vertex()->is_contour()) {
                    assert(t == 0.0f);
                    auto&& extrudeIt = contourExtrudePositions.find(f32v2(x, y));
                    if (extrudeIt != contourExtrudePositions.end()) {
                        // Create a column
                        // TODO: This column will intersect lower floors! Make it smarter
                        const f32v3 boardStart(x, y, building.mZPosFloor - 0.2f);
                        const f32v3 boardEnd(x, y, zPos);
                        meshBuilder.addBoardBetweenPoints(boardStart, boardEnd, f32v3(0.1f), rawWoodTexture, 1.0f);

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
                f32 nextX = he->next()->vertex()->point().x();
                f32 nextY = he->next()->vertex()->point().y();
                f32 nextH = he->next()->vertex()->time() * ROOF_HEIGHT_MULT;
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
                    meshBuilder.addBoardBetweenPoints(boardStart, boardEnd, halfDims, rawWoodTexture, 1.0f);
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

        triangulateRoofFacePolygons(isGable, meshBuilder, building, shinglesTexture, debugColorIndex, zPos);

        ++debugColorIndex;
        if (debugColorIndex >= DEBUG_COLOR_ARRAY_SIZE) debugColorIndex = 0;
    }
}

void BuildingMesher::triangulateRoofFacePolygons(bool isGable, MeshBuilder& meshBuilder, const Building& building, const SubTexture& shinglesTexture, ui32 debugColorIndex, f32 zPos) {
    // Triangulation only works on convex polygons so we will partition the potentially concave poly into
    // separate convex polygons
    // https://stackoverflow.com/questions/1832430/c-cgal-2d-delauny-triangulation-concave-shapes
    // 
    // Partition 
    CGAL::Partition_traits_2<K>::Polygon_2 concavePoly;
    for (auto&& pp : sRoofFacePoints) {
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
                points[i].x = convexPoly.vertex(i).x();
                points[i].y = convexPoly.vertex(i).y();
            }
            // Add to mesh
            addRoofTriangle(meshBuilder, points, building, shinglesTexture, debugColorIndex, zPos);
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
                addRoofTriangle(meshBuilder, points, building, shinglesTexture, debugColorIndex, zPos);
            }
        }
    }
}

void BuildingMesher::addRoofTriangle(
    MeshBuilder& meshBuilder,
    const f32v2 points[3],
    const Building& building,
    const SubTexture& texture,
    ui32 debugColorIndex,
    f32 zPos
) {
    const f32v2 buildingCenter = f32v2(building.mAABB.pos) + f32v2(building.mAABB.dims) * 0.5f;

    StandardVertex verts[3];
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

        verts[i].pos = f32v3(x, y, zPos + deg1 + ROOF_THICKNESS);

        /* if (sDebugOptions.mRoofDebug) {
             DebugRenderer::drawWireQuad(verts[i].pos - f32v3(0.1f, 0.1f, 0.0f), f32v2(0.15f + debugColorIndex * 0.015f), DEBUG_COLOR_ARRAY[debugColorIndex], BUILDING_DEBUG_LIFETIME);
         }*/

        verts[i].color = color4(1.0f, 1.0f, 1.0f, 1.0f);
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

        const f32 UV_SCALE = 0.4f;
        i8v3 compressedNormal = compressNormal(normal);
        const i8v2 tangent(CUBE_FACING_TANGENTS[e_cast(dir)]);
        // Different texturing for nearly vertical polygons
        if (normal.z > 0.3f) {
            for (int i = 0; i < 3; ++i) {
                verts[i].normal = compressedNormal;
                verts[i].tangent = tangent;
                verts[i].uvs.x = verts[i].pos[uvAxis.x] * UV_SCALE;
                verts[i].uvs.y = (verts[i].pos[uvAxis.y]) * UV_SCALE * AXIS_V_DIR_FROM_CARTESIAN[e_cast(dir)] * normal.z;
            }
        }
        else {
            for (int i = 0; i < 3; ++i) {
                verts[i].normal = compressedNormal;
                verts[i].tangent = tangent;
                verts[i].uvs.x = (verts[i].pos[uvAxis.x]) * UV_SCALE;
                verts[i].uvs.y = verts[i].pos.z * UV_SCALE * AXIS_V_DIR_FROM_CARTESIAN[e_cast(dir)];
            }
        }
       /* if (sDebugOptions.mRoofDebug) {
            DebugRenderer::drawWireTriangle(verts[0].pos, verts[1].pos, verts[2].pos, DEBUG_COLOR_ARRAY[debugColorIndex], BUILDING_DEBUG_LIFETIME);
        }*/
        meshBuilder.addTriangle(verts, texture, false);
    }
}


void BuildingMesher::meshRoofContourEdges(const std::vector<RoofContourEdgeInfo>& contourEdges, const Building& building, MeshBuilder& meshBuilder, const SubTexture& shinglesTexture, const SubTexture& rawWoodTexture, f32 zPos) {
    for (auto&& edge : contourEdges) {
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
        // Side
        meshBuilder.addCartesianQuad(first, f32v3(second.x - first.x, second.y - first.y, ROOF_THICKNESS), axis, shinglesTexture, shinglesTexture.mUvRect, COLOR_WHITE);
        // Bottom
        f32v3 points[4];
        points[0] = second;
        points[1] = first;
        points[2] = f32v3(edge.parent1.x, edge.parent1.y, zPos - ROOF_THICKNESS);
        points[3] = f32v3(edge.parent2.x, edge.parent2.y, zPos - ROOF_THICKNESS);
        meshBuilder.addQuadBetweenPoints(points, shinglesTexture, 1.0f, COLOR_WHITE);

        // Compute edge dir
        Cartesian dir;
        constexpr f32 DIR_EPSILON = 0.01f;
        if (edge.v2.x > edge.v1.x + DIR_EPSILON) {
            dir = Cartesian::DOWN;
        }
        else if (edge.v2.x < edge.v1.x - DIR_EPSILON) {
            dir = Cartesian::UP;
        }
        else if (edge.v2.y > edge.v1.y + DIR_EPSILON) {
            dir = Cartesian::RIGHT;
        }
        else {
            dir = Cartesian::LEFT;
        }

        // Step along the edge and add extruded board pieces
        // Random dims
        constexpr f32 BOARD_SIZE_VARIANCE = 0.04f;
        constexpr f32 BOARD_GAP_VARIANCE = 0.1f;
        constexpr f32 BOARD_ANGLE_VARIANCE = 0.15f;
        constexpr f32 BOARD_LENGTH_VARIANCE = 0.15f;
        constexpr f32 BOARDS_PER_METER = 2;
        constexpr f32 BOARD_DISTANCE = 0.25f + ROOF_EXTRUDE_DISTANCE;
        const f32v3 diff = second - first;
        const f32 distance = glm::length(diff);
        const f32v3 iterNormal = diff / distance;
        const f32v3& edgeNormal = CARTESIAN_NORMALS_3D[e_cast(dir)];
        const int boardCount = (int)round(distance * BOARDS_PER_METER);
        const f32 boardGapSize = distance / (boardCount + 1);
        const f32v3 start = first - edgeNormal * ROOF_EXTRUDE_DISTANCE;
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
            const f32 boardLength = BOARD_DISTANCE + (randFromf32v3(offset, i << 2) - 0.5f) * BOARD_LENGTH_VARIANCE;
            const f32v3 p2 = p1 + edgeNormal * boardLength - f32v3(0.0f, 0.0f, ROOF_HEIGHT_MULT * (0.8f + (randFromf32v3(p1, i) - 0.5f) * BOARD_ANGLE_VARIANCE));
            meshBuilder.addBoardBetweenPoints(p1, p2, boardHalfDims, rawWoodTexture, 1.0f);
        }
    }
}