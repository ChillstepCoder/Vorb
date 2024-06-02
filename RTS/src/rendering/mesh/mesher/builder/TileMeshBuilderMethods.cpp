#include "stdafx.h"
#include "TileMeshBuilderMethods.h"

#include "rendering/model/InstancedStaticModelGatherer.h"
#include "rendering/mesh/mesher/builder/ContainerMeshBuilders.h"

#include "tile/TileHandle.h"
#include "tile/Stairs.h"
#include "math/Random.h"
#include "world/Chunk.h"

#include "options/DebugOptions.h"

#include "physics/StaticPhysicsMeshBuilder.h"
#include "resources/TileRepository.h"

#include "rendering/mesh/mesher/builder/ProceduralMeshHelpers.h"
#include "world/IHeightmapGrid.h"

constexpr int TILE_TEX_METHOD_CONNECTED_WALL_WIDTH = 6;
constexpr int TILE_TEX_METHOD_CONNECTED_WALL_HEIGHT = 5;
constexpr int TILE_TEX_METHOD_VERTICAL_WALL_HEIGHT = 3;
constexpr int TILE_TEX_METHOD_VERTICAL_WALL_WIDTH = 1;

constexpr f32 DOOR_THICKNESS = 0.1f;

enum class WallCornerType : ui8{
    FLAT, // No change to edge length
    INWARD, // Shrink edg
    OUTWARD,
    NONE, // Render no flat edge
};

// Track what quads are at a tile
struct WallChainData {
    f32v3 startPos;
    f32 length;
    f32 groundPos;
    f32v2 vertWoobleStartBottom = {};
    f32v2 vertWoobleEndBottom = {};
    f32v2 vertWoobleStartTop = {};
    f32v2 vertWoobleEndTop = {};
    TileID tileId;
    WallCornerType cornerTypeStart;
    WallCornerType cornerTypeEnd;
    Cartesian dir;
    bool isPrimary;

    bool hasWooble() {
        return (vertWoobleStartBottom != f32v2(0.0f) || vertWoobleEndBottom != f32v2(0.0f) || vertWoobleStartTop != f32v2(0.0f) || vertWoobleEndTop != f32v2(0.0f));
    }
};

// TODO: Method(s) file?
struct ConnectedWallData {
    union {
        struct {
            ui8 a;
            ui8 b;
            ui8 c;
            ui8 d;
        };
        ui8 dataArray[4];
        ui32 data;
    };
};

ConnectedWallData sConnectedWallData[256];
const ui16 sExposedWallLookup[4] = { 0x13, 0x12, 0x14, 0x15 }; // These are texture indexes corresponding to the walls

enum ExposedNeighbor8Bits {
    EN8_BOTTOM_LEFT = 1 << 0,
    EN8_BOTTOM = 1 << 1,
    EN8_BOTTOM_RIGHT = 1 << 2,
    EN8_LEFT = 1 << 3,
    EN8_RIGHT = 1 << 4,
    EN8_TOP_LEFT = 1 << 5,
    EN8_TOP = 1 << 6,
    EN8_TOP_RIGHT = 1 << 7
};
enum ExposedNeighbor4Bits {
    EN4_BOTTOM = 1 << 0,
    EN4_LEFT = 1 << 1,
    EN4_RIGHT = 1 << 2,
    EN4_TOP = 1 << 3,
};

const f32v2 CONNECTED_WALL_DIMS = f32v2(6.0f, 5.0f);
const f32v2 VERTICAL_WALL_DIMS = f32v2(1.0f, 3.0f);

inline bool isBitSet(int v, ExposedNeighbor8Bits bit) {
    return v & bit;
}

inline bool isBitSet(int v, ExposedNeighbor4Bits bit) {
    return v & bit;
}

inline bool areAnyBitsSet(int v, int bits) {
    return v & bits;
}

inline bool areBitsSet(int v, int bits) {
    return (v & bits) == bits;
}

const ExposedNeighbor8Bits EXPOSED_NEIGHBOR_8_CARDINAL[4] = {
    EN8_BOTTOM,
    EN8_LEFT,
    EN8_RIGHT,
    EN8_TOP
};

const ExposedNeighbor8Bits EXPOSED_NEIGHBOR_8_ADJACENTS[4][2] = {
    { EN8_LEFT, EN8_RIGHT }, // EN8_BOTTOM
    { EN8_BOTTOM, EN8_TOP }, // EN8_LEFT
    { EN8_BOTTOM, EN8_TOP }, // EN8_RIGHT
    { EN8_LEFT, EN8_RIGHT }, // EN8_TOP
};
const ExposedNeighbor4Bits EXPOSED_NEIGHBOR_4_ADJACENTS[4][2] = {
    { EN4_LEFT, EN4_RIGHT }, // EN4_BOTTOM
    { EN4_BOTTOM, EN4_TOP }, // EN4_LEFT
    { EN4_BOTTOM, EN4_TOP }, // EN4_RIGHT
    { EN4_LEFT, EN4_RIGHT }, // EN4_TOP
};

const NeighborIndex8 EXPOSED_NEIGHBOR_8_ADJACENT_INDICES[4][2] = {
    { NeighborIndex8::LEFT, NeighborIndex8::RIGHT }, // EN8_BOTTOM
    { NeighborIndex8::BOTTOM, NeighborIndex8::TOP }, // EN8_LEFT
    { NeighborIndex8::BOTTOM, NeighborIndex8::TOP }, // EN8_RIGHT
    { NeighborIndex8::LEFT, NeighborIndex8::RIGHT }, // EN8_TOP
};

const NeighborIndex4 EXPOSED_NEIGHBOR_4_ADJACENT_INDICES[4][2] = {
    { NeighborIndex4::LEFT, NeighborIndex4::RIGHT }, // EN4_BOTTOM
    { NeighborIndex4::BOTTOM, NeighborIndex4::TOP }, // EN4_LEFT
    { NeighborIndex4::BOTTOM, NeighborIndex4::TOP }, // EN4_RIGHT
    { NeighborIndex4::LEFT, NeighborIndex4::RIGHT }, // EN4_TOP
};

const CubeFacing EXPOSED_NEIGHBOR_QUAD_FACINGS[4] = {
    CubeFacing::FRONT,
    CubeFacing::LEFT,
    CubeFacing::RIGHT,
    CubeFacing::BACK
};

f32v2 getUvsOffsetsFromConnectedWallIndex(int index) {
    f32v2 rv;
    rv.x = float(index % (int)CONNECTED_WALL_DIMS.x);
    rv.y = float(index / (int)CONNECTED_WALL_DIMS.x);
    return rv;
}

f32v2 getUvsOffsetsFromVerticalWallIndex(int index) {
    f32v2 rv;
    rv.x = 0.0f;
    rv.y = (2 - index) / 3.0f;
    return rv;
}

void meshWallsDefault(const TileSpatialGrid& spatialGrid, const TileWallContainer& tileWalls, const std::vector<Tile>& tiles, i32v3 tileDims, f32 floorHeight, ProceduralMeshBuilder& meshBuilder, std::unordered_set<MaterialID>& materialDependencies, StaticPhysicsMeshBuilder& physMesh) {
    PROFILE_FUNCTION();

    if (tileWalls.isEmpty()) {
        return;
    }

    TileIndex tileIndex = 0;
    i32v3 xyz;
    TileRepository& tileRepo = TileRepository::get();
    for (xyz.z = 0; xyz.z < tileDims.z; ++xyz.z) {
        for (xyz.y = 0; xyz.y < tileDims.y; ++xyz.y) {
            for (xyz.x = 0; xyz.x < tileDims.x; ++xyz.x, ++tileIndex) {
                const f32 groundZOffset = tiles[tileIndex].getGroundZOffset();
                TileWall southAndWestWalls[2];
                tileWalls.getSouthAndWestWallsAtTile(southAndWestWalls, tileIndex);

                for (int i = 0; i < 2; ++i) {
                    if (southAndWestWalls[i].isValid()) {
                        const TileDef& tileData = tileRepo.getLoadedOrUnloadedAsset(southAndWestWalls[i].wallID);
                        ProceduralMeshHelpers::addTileWallMesh(tileData, spatialGrid, tileWalls, tiles, tileIndex, (Cartesian)i, xyz, floorHeight, meshBuilder, materialDependencies, physMesh);
                    }
                }
            }
        }
    }
}

// TODO: Dual grid meshing? https://www.youtube.com/watch?v=buKQjkad2I0
void TileMeshBuilderMethods::meshTileContainer(ContainerMeshBuilders& builders, StaticPhysicsMeshBuilder& physics, OPT const CompressedHeight* heightData) {
    PROFILE_FUNCTION();

    TileRepository& tileRepo = TileRepository::get();
    const ContainerMeshDataCopy& tiles = builders.tileData;
    // TODO: Do we need to handle container resize? Or is resize destroy and remake?
    const TileSpatialGrid& spatialGrid = builders.tileData.spatialGrid;
    const i32v3& tileDims = spatialGrid.getDims();
    const f32v3 tileContainerWorldPos = spatialGrid.getWorldPos();
    const f32 floorHeight = spatialGrid.getFloorHeight();

    //LOG_DEBUG("Meshing Container {}", tileContainer.getId());

    // =============== Mesh tiles ===============
    TileIndex index = 0;
    i32v3 xyz;
    for (xyz.z = 0; xyz.z < tileDims.z; ++xyz.z) {
        for (xyz.y = 0; xyz.y < tileDims.y; ++xyz.y) {
            for (xyz.x = 0; xyz.x < tileDims.x; ++xyz.x, ++index) {
                const Tile& tile = tiles.tiles[index];
                for (int layerIndex = 0; layerIndex < TILE_LAYER_COUNT; ++layerIndex) {
                    TileID layerTile = tile.getLayers()[layerIndex];  // TODO: Thread safe when async
                    // Blocked or invalid tiles have no render (Unowned tiles should all be NONE)
                    if (isTileNone(layerTile)) {
                        continue;
                    }
                    const TileDef& tileData = tileRepo.getLoadedOrUnloadedAsset(layerTile);

                    // Tile mesh
                    // Flora mesh ONLY
                    if (tileData.shape == TileShape::THIN) {
                        // Billboards
                        const f32v3 tilePosition = spatialGrid.getTileCenterWorldPos3D(index, tiles.tiles[index].getGroundZOffset());
                        builders.addMaterial(tileData.materialData[0].id);
                        builders.billboardBuilder.addBillboard(tilePosition, tileData.dims, tileData.materialData[0].id, true);
                    }
                    else if (tileData.shape == TileShape::BLOCK) {
                        // TODO: Handle other materials?
                        builders.addMaterial(tileData.materialData[0].id);
                        TileMeshBuilderMethods::addBlock(builders.staticBuilder, f32v3(xyz.x, xyz.y, xyz.z * floorHeight), tiles.tiles[index], tileData, physics);
                    }
                    else if (tileData.shape == TileShape::FLOOR) {
                        builders.addMaterial(tileData.materialData[0].id);
                        // Adjacent shapes are for culling (ONLY WORKS ON STRUCTURE WITH UNIFORM FLOOR POSITIONS)
                        TileShape adjacentShapes[4] = { TileShape::NONE, TileShape::NONE, TileShape::NONE, TileShape::NONE };
                        if (xyz.y > 0) {
                            TileID south = tiles.tiles[index - tileDims.x].getLayers()[layerIndex];
                            if (!isTileNone(south)) {
                                adjacentShapes[e_cast(Cartesian::SOUTH)] = tileRepo.getLoadedOrUnloadedAsset(south).shape;
                            }
                        }
                        if (xyz.x > 0) {
                            TileID west = tiles.tiles[index - 1].getLayers()[layerIndex];
                            if (!isTileNone(west)) {
                                adjacentShapes[e_cast(Cartesian::WEST)] = tileRepo.getLoadedOrUnloadedAsset(west).shape;
                            }
                        }
                        if (xyz.x < tileDims.x - 1) {
                            TileID east = tiles.tiles[index + 1].getLayers()[layerIndex];
                            if (!isTileNone(east)) {
                                adjacentShapes[e_cast(Cartesian::EAST)] = tileRepo.getLoadedOrUnloadedAsset(east).shape;
                            }
                        }
                        if (xyz.y < tileDims.y - 1) {
                            TileID north = tiles.tiles[index + tileDims.x].getLayers()[layerIndex];
                            if (!isTileNone(north)) {
                                adjacentShapes[e_cast(Cartesian::NORTH)] = tileRepo.getLoadedOrUnloadedAsset(north).shape;
                            }
                        }

                        TileMeshBuilderMethods::addFloor(builders.staticBuilder, adjacentShapes, floorHeight, xyz, tileData.materialData[0], physics);
                    }
                    else if (tileData.shape == TileShape::STAIRS) {
                        builders.addMaterial(tileData.materialData[0].id);
                        TileMeshBuilderMethods::addStairs(builders.staticBuilder, floorHeight, xyz, tile.getGroundZOffset(), tile.getOrientation((TileLayer)layerIndex), tileData, physics);
                    }
                    else if (tileData.shape == TileShape::MODEL) {
                        f32v3 worldPos = spatialGrid.getTileCenterWorldPos3D(index, tiles.tiles[index].getGroundZOffset());
                        ui8 variantIndex = 0;
                        if (layerIndex == TILE_LAYER_MAIN) [[likely]] {
                            variantIndex = tileData.modelVariants[tiles.tiles[index].getMainLayerVariant()];
                        }
                        else {
                            // Should be impossible?
                            panic("Ground tile {} - {} has a model shape on a non-main layer", index, tiles.tiles[index].getGroundID());
                        }
                        if (heightData) {
                            //sHeightmapGrid->getHeightDataAt(chunk.getHeightmapPatchID())->data;
                            builders.modelGatherer.addInstance(tileData.modelId, index, worldPos, f32v3(0.0f, 0.0f, 1.0f), Random::getCachedRandomfSpecific((ui32)(worldPos.x + worldPos.y * 1000.0f)) * M_2_PI, variantIndex);
                        }
                        else {
                            builders.modelGatherer.addInstance(tileData.modelId, index, worldPos, getModelRotationAtPosition(worldPos), variantIndex);
                        }
                        if (tileData.collisionShapeID != INVALID_COLLISION_SHAPE_ID) {
                            physics.addTrackedStaticRigidBody(index, worldPos, tileData.collisionShapeID);
                        }
                    }
                }
            }
        }
    }
    meshWallsDefault(spatialGrid, tiles.walls, tiles.tiles, tileDims, floorHeight, builders.staticBuilder, builders.materialDependencies, physics);

    // Dynamics
    //for (auto&& dynamicTile : tileContainer.getDynamicTiles()) {
    //    TileIndex tileIndex = dynamicTile.mTileIndex;
    //    // If this is a wall
    //    if (dynamicTile.mType <= DynamicTileType::WALL_TERM) {
    //        Cartesian dir = Cartesian(dynamicTile.mType);
    //        static_assert(e_cast(DynamicTileType::WALL_SOUTH) == 0 && e_cast(DynamicTileType::WALL_TERM) == 3);
    //        const TileWall& wall = tileWalls.getWallAtTile(tileIndex).walls[e_cast(dir)];
    //        assert(wall.wallID != TILE_ID_NONE);
    //        // Check if is door
    //        const TileData& tileData = TileRepository::getTileData(wall.wallID);
    //        if (tileData.shape == TileShape::DOOR) {
    //            f32v2 dims;
    //            f32v3 p1 = tileContainer.getTileXYZOffset(tileIndex);
    //            p1.z *= floorHeight;
    //            switch (dir) {
    //                case Cartesian::SOUTH:
    //                    dims = f32v2(DOOR_THICKNESS, 0.5f);
    //                    p1.x += 0.5f;
    //                    break;
    //                case Cartesian::WEST:
    //                    dims = f32v2(0.5f, DOOR_THICKNESS);
    //                    p1.y += 0.5f;
    //                    break;
    //                case Cartesian::EAST:
    //                    dims = f32v2(0.5f, DOOR_THICKNESS);
    //                    p1.x += 1.0f;
    //                    p1.y += 0.5f;
    //                    break;
    //                case Cartesian::NORTH:
    //                    dims = f32v2(DOOR_THICKNESS, 0.5f);
    //                    p1.x += 0.5f;
    //                    p1.y += 1.0f;
    //                    break;
    //                default:
    //                    assert(false);
    //                    break;

    //            }
    //            f32v3 p2 = p1;
    //            p2.z += floorHeight;
    //            builders.staticBuilder.addBoardBetweenPoints(p1, p2, dims, tileData.materialData, f32v2(1.0f, 1.0f / 3.0f));
    //        }
    //        else {
    //            assert(false);
    //        }
    //    }
    //    else {
    //        assert(false); // Implement other types
    //    }
    //}
}


void TileMeshBuilderMethods::addBlock(ProceduralMeshBuilder& meshBuilder, const f32v3& tilePos, const Tile& tile, const TileDef& tileData, StaticPhysicsMeshBuilder& physMesh) {
    switch (tileData.textureMethod) {
        case TileTextureMethod::SIMPLE: {
            meshBuilder.addAxisAlignedQuad(
                tilePos + CUBE_FACING_GEOMETRY_OFFSETS[e_cast(CubeFacing::BOTTOM)],
                f32v2(1.0f) /*dims*/,
                CubeFacing::TOP,
                tileData.materialData[0],
                f32v4(0.0f, 0.0f, 1.0f, 1.0f),
                COLOR_WHITE
            );
            break;
        }
        case TileTextureMethod::CONNECTED_WALL: {
            // todo: FIX (check history for old code)
            break;
        }
        case TileTextureMethod::VERTICAL: {
            addBlockVertical(meshBuilder, tilePos, tile, tileData, physMesh);
            break;
        }
        case TileTextureMethod::WORLD_TILING: {
            // todo: FIX
            //int xOff = (ui32)tilePos.x % 8;
            //int yOff = 7 - (ui32)tilePos.y % 8;
            addBlockWorldTiling(meshBuilder, tilePos, tile, tileData, physMesh);
            break;
        }
        case TileTextureMethod::FLORA: {
            // We do not mesh flora in this pass
            break;
        }
    }
    static_assert((int)TileTextureMethod::COUNT == 6, "Implement geo generation for new method");
}

void TileMeshBuilderMethods::addBlockVertical(ProceduralMeshBuilder& meshBuilder, const f32v3& tilePos, const Tile& tile, const TileDef& tileData, StaticPhysicsMeshBuilder& physMesh) {

    const MaterialDesc& materialData = tileData.materialData[0];

    const f32 topZPosition = tile.getGroundZOffset();
    const f32v3 topPos(tilePos.x, tilePos.y, topZPosition);

    /*TileHandle neighbors[4];
    chunk.getTileNeighbors4(tileHandle.index, neighbors);*/

    // Get height offsets to adjacent tiles
    //const f32 zPosition = tile.groundZPosition; // Dont check terrain here, assume above // TODO: make sure this is right
    const f32 quadHeight = topZPosition - tilePos.z;
    f32 heightDiffs[4];
    // f32 occluderHeight
    // TODO: Fix this
    for (int i = 0; i < 4; ++i) {
        heightDiffs[i] = quadHeight;
    }
    // Old culling
    /*heightDiffs[(int)NeighborIndex4::BOTTOM] = groundZPosition - getTileHeight(floor, neighbors[(int)NeighborIndex4::BOTTOM]);
    heightDiffs[(int)NeighborIndex4::LEFT] = groundZPosition - getTileHeight(floor, neighbors[(int)NeighborIndex4::LEFT]);
    heightDiffs[(int)NeighborIndex4::RIGHT] = groundZPosition - getTileHeight(floor, neighbors[(int)NeighborIndex4::RIGHT]);
    heightDiffs[(int)NeighborIndex4::TOP] = groundZPosition - getTileHeight(floor, neighbors[(int)NeighborIndex4::TOP]);*/

    // Render top
    meshBuilder.addAxisAlignedQuad(
        topPos,
        f32v2(1.0f), // Dimensions
        CubeFacing::TOP,
        materialData,
        f32v4(0.0f, 2.0f / 3.0f, 1.0f, 1.0f / 3.0f),
        COLOR_WHITE
    );
    physMesh.addTileQuad(topPos, f32v2(1.0f), CubeFacing::TOP);

    for (int c = 0; c < 4; ++c) {
        if (heightDiffs[c] > 0.0f) {
            CubeFacing quadFacing = EXPOSED_NEIGHBOR_QUAD_FACINGS[c];
            const f32v3 quadPos = tilePos + CUBE_FACING_GEOMETRY_OFFSETS[e_cast(quadFacing)];
            physMesh.addTileQuad(f32v3(quadPos.x, quadPos.y, quadPos.z), f32v2(1.0f, heightDiffs[c]), quadFacing);
        }
    }

    // Render sides
    const f32v3 topBase = topPos - f32v3(0.0f, 0.0f, 1.0f);
    for (int c = 0; c < 4; ++c) {
        // Render exposed cardinal wall if needed
        if (heightDiffs[c] > 0.0f) {
            CubeFacing quadFacing = EXPOSED_NEIGHBOR_QUAD_FACINGS[c];
            const f32v3 quadPos = topBase + CUBE_FACING_GEOMETRY_OFFSETS[e_cast(quadFacing)];
            const f32v2 offsets = getUvsOffsetsFromVerticalWallIndex(2);
            f32v4 uvs(0.0f, 0.0f, 1.0f, 1.0f);
            uvs.y += offsets.y * uvs.w;
            uvs.w /= 3.0f;

            meshBuilder.addAxisAlignedQuad(
                quadPos,
                f32v2(1.0f), // Dimensions
                quadFacing,
                materialData,
                uvs,
                COLOR_WHITE
            );

            // See if we need to add additional "tower" quads if we are exposed deeper on the bottom
            for (int i = 1; i < heightDiffs[c]; ++i) {
                unsigned sideCheck = 0;
                // New exposure check for left and right on towers
                ui16 val = 1;
                // Only bottom uses bottom texture
                if (i + 1.5f >= heightDiffs[c]) {
                    val = 0;
                }

                const f32v2 offsets = getUvsOffsetsFromVerticalWallIndex(val);
                f32v4 uvs(0.0f, 0.0f, 1.0f, 1.0f);
                // + 1 for the tall wall variants
                uvs.y += offsets.y * uvs.w;
                uvs.w /= 3.0f;
                // TODO: this resize here...

                // TODO: Stretched quads?
                meshBuilder.addAxisAlignedQuad(
                    f32v3(quadPos.x, quadPos.y, quadPos.z - i),
                    f32v2(1.0f), // Dimensions
                    quadFacing,
                    materialData,
                    uvs,
                    COLOR_WHITE
                );
            }
        }
    }
}

void TileMeshBuilderMethods::addBlockWorldTiling(ProceduralMeshBuilder& meshBuilder, const f32v3& tilePos, const Tile& tile, const TileDef& tileData, StaticPhysicsMeshBuilder& physMesh) {
    const f32 topHeight = tilePos.z + tile.getGroundZOffset();

    const f32v3 botSW(tilePos);
    const f32v3 botSE(tilePos.x + 1.0f, tilePos.y, tilePos.z);
    const f32v3 botNW(tilePos.x, tilePos.y + 1.0f, tilePos.z);
    const f32v3 botNE(tilePos.x + 1.0f, tilePos.y + 1.0f, tilePos.z);
    const f32v3 topSW(tilePos.x, tilePos.y, topHeight);
    const f32v3 topSE(tilePos.x + 1.0f, tilePos.y, topHeight);
    const f32v3 topNW(tilePos.x, tilePos.y + 1.0f, topHeight);
    const f32v3 topNE(tilePos.x + 1.0f, tilePos.y + 1.0f, topHeight);
    meshBuilder.addQuadBetweenPointsWorldUV(topSW, topSE, topNE, topNW, tileData.materialData[0], f32v2(1.0f), COLOR_WHITE, AXIS_Z, tilePos, false);
    meshBuilder.addQuadBetweenPointsWorldUV(botSW, topSW, topNW, botNW, tileData.materialData[0], f32v2(1.0f), COLOR_WHITE, AXIS_X, tilePos, false);
    meshBuilder.addQuadBetweenPointsWorldUV(botNE, topNE, topSE, botSE, tileData.materialData[0], f32v2(1.0f), COLOR_WHITE, AXIS_X, tilePos, false);
    meshBuilder.addQuadBetweenPointsWorldUV(botSW, botSE, topSE, topSW, tileData.materialData[0], f32v2(1.0f), COLOR_WHITE, AXIS_Y, tilePos, false);
    meshBuilder.addQuadBetweenPointsWorldUV(botNW, topNW, topNE, botNE, tileData.materialData[0], f32v2(1.0f), COLOR_WHITE, AXIS_Y, tilePos, false);
    physMesh.addQuadBetweenPoints(topSW, topSE, topNE, topNW);
    physMesh.addQuadBetweenPoints(botSW, topSW, topNW, botNW);
    physMesh.addQuadBetweenPoints(botNE, topNE, topSE, botSE);
    physMesh.addQuadBetweenPoints(botSW, botSE, topSE, topSW);
    physMesh.addQuadBetweenPoints(botNW, topNW, topNE, botNE);
}

void TileMeshBuilderMethods::addFloor(ProceduralMeshBuilder& meshBuilder, TileShape adjacentShapes[4], f32 floorHeight, const ui32v3& tileXYZ, const MaterialDesc& materialData, StaticPhysicsMeshBuilder& physMesh) {

    constexpr f32 FLOOR_THICKNESS = 0.05f;
    // Epsilon to prevent Z fighting
    const f32v3 tilePos(tileXYZ.x, tileXYZ.y, tileXYZ.z * floorHeight + 0.005f);

    f32v3 positions[4];
    const f32v2 wooble0 = getStructureWoobleAtPoint(tileXYZ);
    positions[0] = f32v3(tilePos.x + wooble0.x, tilePos.y + wooble0.y, tilePos.z);
    const f32v2 wooble1 = getStructureWoobleAtPoint(tileXYZ + ui32v3(1, 0, 0));
    positions[1] = f32v3(tilePos.x + 1.0f + wooble1.x, tilePos.y + wooble1.y, tilePos.z);
    const f32v2 wooble2 = getStructureWoobleAtPoint(tileXYZ + ui32v3(1, 1, 0));
    positions[2] = f32v3(tilePos.x + 1.0f + wooble2.x, tilePos.y + 1.0f + wooble2.y, tilePos.z);
    const f32v2 wooble3 = getStructureWoobleAtPoint(tileXYZ + ui32v3(0, 1, 0));
    positions[3] = f32v3(tilePos.x + wooble3.x, tilePos.y + 1.0f + wooble3.y, tilePos.z);

    // Top
    // 3 2
    // 0 1
    meshBuilder.addQuadBetweenPointsWorldUV(positions, materialData, f32v2(1.0f), COLOR_WHITE, AXIS_Z, f32v3(0.0f));

    // Collision ONLY on top. Floors are thin so the extra physics geo is not needed
    physMesh.addQuadBetweenPoints(positions);

    // No need to mesh bottoms if we are at base level
    if (tileXYZ.z == 0) {
        return;
    }
    f32v3 bottomPositions[4] = {
        positions[1], positions[0], positions[3], positions[2]
    };
    for (int i = 0; i < 4; ++i) {
        bottomPositions[i].z -= FLOOR_THICKNESS;
    }
    // Bottom
    // 2 3
    // 1 0
    meshBuilder.addQuadBetweenPointsWorldUV(bottomPositions, materialData, f32v2(1.0f), COLOR_WHITE, AXIS_Z, f32v3(0.0f));

    // TODO: Smarter culling
    // South
    if (adjacentShapes[e_cast(Cartesian::SOUTH)] != TileShape::FLOOR) {
        meshBuilder.addQuadBetweenPointsWorldUV(bottomPositions[1], bottomPositions[0], positions[1], positions[0], materialData, f32v2(1.0f), COLOR_WHITE, AXIS_Y, f32v3(0.0f));
    }
    // West
    if (adjacentShapes[e_cast(Cartesian::WEST)] != TileShape::FLOOR) {
        meshBuilder.addQuadBetweenPointsWorldUV(bottomPositions[2], bottomPositions[1], positions[0], positions[3], materialData, f32v2(1.0f), COLOR_WHITE, AXIS_X, f32v3(0.0f));
    }
    // East
    if (adjacentShapes[e_cast(Cartesian::EAST)] != TileShape::FLOOR) {
        meshBuilder.addQuadBetweenPointsWorldUV(bottomPositions[0], bottomPositions[3], positions[2], positions[1], materialData, f32v2(1.0f), COLOR_WHITE, AXIS_X, f32v3(0.0f));
    }
    // North
    if (adjacentShapes[e_cast(Cartesian::NORTH)] != TileShape::FLOOR) {
        meshBuilder.addQuadBetweenPointsWorldUV(bottomPositions[3], bottomPositions[2], positions[3], positions[2], materialData, f32v2(1.0f), COLOR_WHITE, AXIS_Y, f32v3(0.0f));
    }
    
    // OLD terrain implementation
    /*f32 corners[4];

    mWorldGrid.computeTileCorners(heightData->data, TilePosition(chunk.getChunkID(), tileIndex), corners);

    if (isOnTerrain) {
        quadMeshBuilder.addTerrainAlignedQuad(
            tilePosition + CUBE_FACING_GEOMETRY_OFFSETS[e_cast(CubeFacing::TOP)],
            corners,
            texture,
            COLOR_WHITE,
            mWorldGrid.areTrianglesFlippedAtTile(tileIndex)
        );
    }
    else {
        assert(false);
    }*/
}

void TileMeshBuilderMethods::addCeiling(ProceduralMeshBuilder& meshBuilder, f32 floorHeight, const ui32v3& tileXYZ, const MaterialDesc& materialData, StaticPhysicsMeshBuilder& physMesh) {
    const f32v3 tilePos(tileXYZ.x, tileXYZ.y, tileXYZ.z * floorHeight + 0.0001f);

    f32v3 positions[4];
    const f32v2 wooble0 = getStructureWoobleAtPoint(tileXYZ);
    positions[0] = f32v3(tilePos.x + wooble0.x, tilePos.y + wooble0.y, tilePos.z);
    const f32v2 wooble1 = getStructureWoobleAtPoint(tileXYZ + ui32v3(1, 0, 0));
    positions[1] = f32v3(tilePos.x + 1.0f + wooble1.x, tilePos.y + wooble1.y, tilePos.z);
    const f32v2 wooble2 = getStructureWoobleAtPoint(tileXYZ + ui32v3(1, 1, 0));
    positions[2] = f32v3(tilePos.x + 1.0f + wooble2.x, tilePos.y + 1.0f + wooble2.y, tilePos.z);
    const f32v2 wooble3 = getStructureWoobleAtPoint(tileXYZ + ui32v3(0, 1, 0));
    positions[3] = f32v3(tilePos.x + wooble3.x, tilePos.y + 1.0f + wooble3.y, tilePos.z);

    meshBuilder.addQuadBetweenPointsWorldUV(positions, materialData, f32v2(1.0f), COLOR_WHITE, AXIS_Z, f32v3(0.0f));
    physMesh.addQuadBetweenPoints(positions);
}
//
//void TileMeshBuilderMethods::addFloorTerrainAligned(ProceduralMeshBuilder& meshBuilder, f32 floorBaseHeight, const f32v2& tileXY, const HeightmapPatch* heightData, const TileHandle& tileHandle, const TileDef& tileData) {
//    //const SubTexture& texture = tileData.texture;
//
//    //f32 corners[4];
//    //mWorldGrid.computeTileCorners(heightData->data, TilePosition(chunk.getChunkID(), tileIndex), corners);
//
//    //if (isOnTerrain) {
//    //    quadMeshBuilder.addTerrainAlignedQuad(
//    //        tilePosition + CUBE_FACING_GEOMETRY_OFFSETS[e_cast(CubeFacing::TOP)],
//    //        corners,
//    //        texture,
//    //        COLOR_WHITE,
//    //        mWorldGrid.areTrianglesFlippedAtTile(tileIndex)
//    //    );
//    //}
//    //else {
//    //    assert(false);
//    //}
//}

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

void TileMeshBuilderMethods::addStairs(ProceduralMeshBuilder& meshBuilder, f32 floorHeight, const ui32v3& tileXYZ, float tileGroundZOffset, Cartesian tileOrientation, const TileDef& tileData, StaticPhysicsMeshBuilder& physMesh)
{
    const f32v3 tilePos(tileXYZ.x, tileXYZ.y, tileXYZ.z * floorHeight);
    // Place stair steps
    // TODO: ThreadSafe
    const f32 heightAdd = tileGroundZOffset;
    const Cartesian dir = tileOrientation;//  tile.getOrientation((TileLayer)tileData.layer);
    const f32v2 stepDir = CARTESIAN_NORMALS_2D[e_cast(dir)];
    constexpr f32 stepWidth = 1.0f / STEPS_PER_TILE;
    const f32 stairPieceBaseHeight = tilePos.z + heightAdd;
    const f32 stairPieceTopHeight = stairPieceBaseHeight + STEPS_PER_TILE * stepHeight;
    AXIS_3D sideUvOrient; // For UV mapping
    AXIS_3D frontUvOrient; // For UV mapping

    //if (stairPiece.isFlatPart) {
    //    // Flat parts are just a single quad on top
    //    const f32v3 pointsTop[4] = {
    //        f32v3(tilePos.x, tilePos.y, stairPieceBaseHeight),
    //        f32v3(tilePos.x + 1.0f, tilePos.y, stairPieceBaseHeight),
    //        f32v3(tilePos.x + 1.0f, tilePos.y + 1.0f, stairPieceBaseHeight),
    //        f32v3(tilePos.x, tilePos.y + 1.0f, stairPieceBaseHeight),
    //    };
    //    meshBuilder.addQuadBetweenPointsWorldUV(pointsTop, rawWoodTexture, f32v2(1.0f), COLOR_WHITE, AXIS_Z, f32v3(0.0f));
    //    building.mPhysicsMesh.addQuadBetweenPoints(pointsTop);
    //    switch (dir) {
    //        case Cartesian::SOUTH:
    //            sideUvOrient = AXIS_X;
    //            break;
    //        case Cartesian::WEST:
    //            sideUvOrient = AXIS_Y;
    //            break;
    //        case Cartesian::EAST:
    //            sideUvOrient = AXIS_Y;
    //            break;
    //        case Cartesian::NORTH:
    //            sideUvOrient = AXIS_X;
    //            break;
    //        default:
    //            assert(false);
    //            break;
    //    }
    //}
    //else {
        // Mesh each step and its sides
    for (i32 step = 0; step < STEPS_PER_TILE; ++step) {
        const f32 height = stairPieceBaseHeight + (step + 1) * stepHeight + 0.0001f/*epsilon*/;
        // Top bit
        const f32v2 stepStride = stepDir * stepWidth;
        const f32v2 stepOffset = (f32)step * stepStride;
        f32v2 cornerPos2D(tilePos.x + stepOffset.x, tilePos.y + stepOffset.y);
        cornerPos2D += f32v2(STAIR_DIR_OFFSETS[e_cast(dir)]) - f32v2(STAIR_DIR_OFFSETS[e_cast(dir)]) * 0.25f;

        const f32v2& stairDims = STAIR_DIR_DIMS[e_cast(dir)];
        const f32v3 pointsTop[4] = {
            f32v3(cornerPos2D.x, cornerPos2D.y, height),
            f32v3(cornerPos2D.x + stairDims.x, cornerPos2D.y, height),
            f32v3(cornerPos2D.x + stairDims.x, cornerPos2D.y + stairDims.y, height),
            f32v3(cornerPos2D.x, cornerPos2D.y + stairDims.y, height),
        };
        // Side bit and ground bit using switch cause I dont feel like figuring out a clever branchless way
        f32v3 pointsFront[4] = {
            f32v3(cornerPos2D.x, cornerPos2D.y, height),
            f32v3(cornerPos2D.x, cornerPos2D.y, height),
            f32v3(cornerPos2D.x, cornerPos2D.y, height),
            f32v3(cornerPos2D.x, cornerPos2D.y, height),
        };
        // 8 Points since we have two sides
        f32v3 pointsSide[8] = {}; // TODO: UNZERO
        //  We have to reduce the base of each next step 
        switch (dir) {
            case Cartesian::SOUTH:
                sideUvOrient = AXIS_X;
                frontUvOrient = AXIS_Y;
                pointsFront[0].y += stepWidth;
                pointsFront[1].y += stepWidth;
                pointsFront[2].y += stepWidth;
                pointsFront[3].y += stepWidth;

                pointsFront[2].x += 1.0f;
                pointsFront[3].x += 1.0f;

                pointsFront[0].z -= stepHeight;
                pointsFront[3].z -= stepHeight;

                pointsSide[0] = f32v3(tilePos.x, tilePos.y, stairPieceBaseHeight);
                pointsSide[1] = pointsTop[0];
                pointsSide[2] = pointsTop[3];
                pointsSide[3] = pointsFront[0];

                pointsSide[4] = f32v3(tilePos.x + 1.0f, tilePos.y, stairPieceBaseHeight);
                pointsSide[5] = pointsFront[3];
                pointsSide[6] = pointsTop[2];
                pointsSide[7] = pointsTop[1];
                break;
            case Cartesian::WEST:
                sideUvOrient = AXIS_Y;
                frontUvOrient = AXIS_X;
                pointsFront[0].x += stepWidth;
                pointsFront[1].x += stepWidth;
                pointsFront[2].x += stepWidth;
                pointsFront[3].x += stepWidth;

                pointsFront[2].y += 1.0f;
                pointsFront[3].y += 1.0f;

                pointsFront[1].z -= stepHeight;
                pointsFront[2].z -= stepHeight;

                pointsSide[0] = f32v3(tilePos.x, tilePos.y, stairPieceBaseHeight);
                pointsSide[1] = pointsFront[1];
                pointsSide[2] = pointsTop[1];
                pointsSide[3] = pointsTop[0];

                pointsSide[4] = f32v3(tilePos.x, tilePos.y + 1.0f, stairPieceBaseHeight);
                pointsSide[5] = pointsTop[3];
                pointsSide[6] = pointsTop[2];
                pointsSide[7] = pointsFront[2];
                break;
            case Cartesian::EAST:
                sideUvOrient = AXIS_Y;
                frontUvOrient = AXIS_X;
                pointsFront[2].y += 1.0f;
                pointsFront[3].y += 1.0f;

                pointsFront[0].z -= stepHeight;
                pointsFront[3].z -= stepHeight;

                pointsSide[0] = f32v3(tilePos.x + 1.0f, tilePos.y, stairPieceBaseHeight);
                pointsSide[1] = pointsTop[1];
                pointsSide[2] = pointsTop[0];
                pointsSide[3] = pointsFront[0];

                pointsSide[4] = f32v3(tilePos.x + 1.0f, tilePos.y + 1.0f, stairPieceBaseHeight);
                pointsSide[5] = pointsFront[3];
                pointsSide[6] = pointsTop[3];
                pointsSide[7] = pointsTop[2];
                break;
            case Cartesian::NORTH:
                sideUvOrient = AXIS_X;
                frontUvOrient = AXIS_Y;
                pointsFront[2].x += 1.0f;
                pointsFront[3].x += 1.0f;

                pointsFront[1].z -= stepHeight;
                pointsFront[2].z -= stepHeight;

                pointsSide[0] = f32v3(tilePos.x, tilePos.y + 1.0f, stairPieceBaseHeight);
                pointsSide[1] = pointsFront[1];
                pointsSide[2] = pointsTop[0];
                pointsSide[3] = pointsTop[3];

                pointsSide[4] = f32v3(tilePos.x + 1.0f, tilePos.y + 1.0f, stairPieceBaseHeight);
                pointsSide[5] = pointsTop[2];
                pointsSide[6] = pointsTop[1];
                pointsSide[7] = pointsFront[2];
                break;
            default:
                assert(false);
                break;
        }
        const f32v2 uvScale = f32v2(1.0f);
        meshBuilder.addQuadBetweenPointsWorldUV(pointsTop, tileData.materialData[0], uvScale, COLOR_WHITE, AXIS_Z, f32v3(0.0f));
        // The very first step in the entire chain shouldn't have base pieces
        meshBuilder.addQuadBetweenPointsWorldUV(pointsFront, tileData.materialData[0], uvScale, COLOR_WHITE, frontUvOrient, f32v3(0.0f));
        meshBuilder.addQuadBetweenPointsWorldUV(pointsSide, tileData.materialData[0], uvScale, COLOR_WHITE, sideUvOrient, f32v3(0.0f));
        meshBuilder.addQuadBetweenPointsWorldUV(&(pointsSide[4]), tileData.materialData[0], uvScale, COLOR_WHITE, sideUvOrient, f32v3(0.0f));

    }
    // Collision for the side and top of a stair
    f32v3 collisionPointsLeft[3];
    f32v3 collisionPointsRight[3];
    f32v3 collisionPointsRamp[4];
    const f32v3 rampBasePos(tilePos.x, tilePos.y, stairPieceBaseHeight);
    const f32v3 rampTopPos(tilePos.x, tilePos.y, stairPieceTopHeight);
    // Winding doesnt matter
    switch (dir) {
        case Cartesian::SOUTH:
            collisionPointsLeft[0] = rampBasePos;
            collisionPointsLeft[1] = rampTopPos;
            collisionPointsLeft[2] = rampBasePos + f32v3(0.0f, 1.0f, 0.0f);
            collisionPointsRight[0] = collisionPointsLeft[0] + f32v3(1.0f, 0.0f, 0.0f);
            collisionPointsRight[1] = collisionPointsLeft[1] + f32v3(1.0f, 0.0f, 0.0f);
            collisionPointsRight[2] = collisionPointsLeft[2] + f32v3(1.0f, 0.0f, 0.0f);
            collisionPointsRamp[0] = collisionPointsLeft[1];
            collisionPointsRamp[1] = collisionPointsLeft[2];
            collisionPointsRamp[2] = collisionPointsRight[2];
            collisionPointsRamp[3] = collisionPointsRight[1];
            break;
        case Cartesian::WEST:
            collisionPointsLeft[0] = rampBasePos;
            collisionPointsLeft[1] = rampTopPos;
            collisionPointsLeft[2] = rampBasePos + f32v3(1.0f, 0.0f, 0.0f);
            collisionPointsRight[0] = collisionPointsLeft[0] + f32v3(0.0f, 1.0f, 0.0f);
            collisionPointsRight[1] = collisionPointsLeft[1] + f32v3(0.0f, 1.0f, 0.0f);
            collisionPointsRight[2] = collisionPointsLeft[2] + f32v3(0.0f, 1.0f, 0.0f);
            collisionPointsRamp[0] = collisionPointsLeft[1];
            collisionPointsRamp[1] = collisionPointsLeft[2];
            collisionPointsRamp[2] = collisionPointsRight[2];
            collisionPointsRamp[3] = collisionPointsRight[1];
            break;
        case Cartesian::EAST:
            collisionPointsLeft[0] = rampBasePos;
            collisionPointsLeft[1] = rampTopPos + f32v3(1.0f, 0.0f, 0.0f);
            collisionPointsLeft[2] = rampBasePos + f32v3(1.0f, 0.0f, 0.0f);
            collisionPointsRight[0] = collisionPointsLeft[0] + f32v3(0.0f, 1.0f, 0.0f);
            collisionPointsRight[1] = collisionPointsLeft[1] + f32v3(0.0f, 1.0f, 0.0f);
            collisionPointsRight[2] = collisionPointsLeft[2] + f32v3(0.0f, 1.0f, 0.0f);
            collisionPointsRamp[0] = collisionPointsLeft[0];
            collisionPointsRamp[1] = collisionPointsLeft[1];
            collisionPointsRamp[2] = collisionPointsRight[1];
            collisionPointsRamp[3] = collisionPointsRight[0];
            break;
        case Cartesian::NORTH:
            collisionPointsLeft[0] = rampBasePos + f32v3(0.0f, 1.0f, 0.0f);
            collisionPointsLeft[1] = rampTopPos + f32v3(0.0f, 1.0f, 0.0f);
            collisionPointsLeft[2] = rampBasePos;
            collisionPointsRight[0] = collisionPointsLeft[0] + f32v3(1.0f, 0.0f, 0.0f);
            collisionPointsRight[1] = collisionPointsLeft[1] + f32v3(1.0f, 0.0f, 0.0f);
            collisionPointsRight[2] = collisionPointsLeft[2] + f32v3(1.0f, 0.0f, 0.0f);
            collisionPointsRamp[0] = collisionPointsLeft[1];
            collisionPointsRamp[1] = collisionPointsLeft[2];
            collisionPointsRamp[2] = collisionPointsRight[2];
            collisionPointsRamp[3] = collisionPointsRight[1];
            break;
        default:
            assert(false);
            break;
    }
    physMesh.addTriangleBetweenPoints(collisionPointsLeft);
    physMesh.addTriangleBetweenPoints(collisionPointsRight);
    physMesh.addQuadBetweenPoints(collisionPointsRamp);

    // Endcap quad
    // TODO: World space mesher util for cartesian quad?
    // // TODO: Cull this?
    //if (stairPiece.isLastPiece) {
    f32v3 pointsEndcap[4];
    AXIS_3D endcapUvOrient;
    switch (dir) {
        case Cartesian::NORTH:
            endcapUvOrient = AXIS_Y;
            pointsEndcap[0] = { tilePos.x, tilePos.y + 1.0f, tilePos.z };
            pointsEndcap[1] = { tilePos.x, tilePos.y + 1.0f, stairPieceTopHeight };
            pointsEndcap[2] = { tilePos.x + 1.0f, tilePos.y + 1.0f, stairPieceTopHeight };
            pointsEndcap[3] = { tilePos.x + 1.0f, tilePos.y + 1.0f, tilePos.z };
            break;
        case Cartesian::SOUTH:
            endcapUvOrient = AXIS_Y;
            pointsEndcap[0] = { tilePos.x, tilePos.y, tilePos.z };
            pointsEndcap[1] = { tilePos.x + 1.0f, tilePos.y, tilePos.z };
            pointsEndcap[2] = { tilePos.x + 1.0f, tilePos.y, stairPieceTopHeight };
            pointsEndcap[3] = { tilePos.x, tilePos.y, stairPieceTopHeight };
            break;
        case Cartesian::WEST:
            endcapUvOrient = AXIS_X;
            pointsEndcap[0] = { tilePos.x, tilePos.y, tilePos.z };
            pointsEndcap[1] = { tilePos.x, tilePos.y, stairPieceTopHeight };
            pointsEndcap[2] = { tilePos.x, tilePos.y + 1.0f, stairPieceTopHeight };
            pointsEndcap[3] = { tilePos.x, tilePos.y + 1.0f, tilePos.z };
            break;
        case Cartesian::EAST:
            endcapUvOrient = AXIS_X;
            pointsEndcap[0] = { tilePos.x + 1.0f, tilePos.y + 1.0f, tilePos.z };
            pointsEndcap[1] = { tilePos.x + 1.0f, tilePos.y + 1.0f, stairPieceTopHeight };
            pointsEndcap[2] = { tilePos.x + 1.0f, tilePos.y, stairPieceTopHeight };
            pointsEndcap[3] = { tilePos.x + 1.0f, tilePos.y, tilePos.z };
            break;
        default:
            break;

    }
    meshBuilder.addQuadBetweenPointsWorldUV(pointsEndcap, tileData.materialData[0], f32v2(1.0f), COLOR_WHITE, endcapUvOrient, f32v3(0.0f));
    physMesh.addQuadBetweenPoints(pointsEndcap);
       // }
    //}
    // Place square walls to the ground
    f32v3 pointsSide[8] = { tilePos, tilePos, tilePos, tilePos, tilePos, tilePos, tilePos, tilePos };
    switch (dir) {
        case Cartesian::NORTH:
        case Cartesian::SOUTH:
            pointsSide[1].z = stairPieceBaseHeight;
            pointsSide[2].z = stairPieceBaseHeight;
            pointsSide[2].y += 1.0f;
            pointsSide[3].y += 1.0f;

            pointsSide[4] += f32v3(1.0f, 1.0f, 0.0f);
            pointsSide[7].x += 1.0f;
            pointsSide[5] = f32v3(pointsSide[4].x, pointsSide[4].y, stairPieceBaseHeight);
            pointsSide[6] = f32v3(pointsSide[7].x, pointsSide[7].y, stairPieceBaseHeight);
            break;
        case Cartesian::WEST:
        case Cartesian::EAST:
            pointsSide[1].x += 1.0f;
            pointsSide[2].x += 1.0f;
            pointsSide[2].z = stairPieceBaseHeight;
            pointsSide[3].z = stairPieceBaseHeight;

            pointsSide[4] += f32v3(1.0f, 1.0f, 0.0f);
            pointsSide[5].y += 1.0f;
            pointsSide[6] = f32v3(pointsSide[5].x, pointsSide[5].y, stairPieceBaseHeight);
            pointsSide[7] = f32v3(pointsSide[4].x, pointsSide[4].y, stairPieceBaseHeight);
            break;
        default:
            break;

    }
    meshBuilder.addQuadBetweenPointsWorldUV(pointsSide, tileData.materialData[0], f32v2(1.0f), COLOR_WHITE, sideUvOrient, f32v3(0.0f));
    physMesh.addQuadBetweenPoints(pointsSide);
    meshBuilder.addQuadBetweenPointsWorldUV(&(pointsSide[4]), tileData.materialData[0], f32v2(1.0f), COLOR_WHITE, sideUvOrient, f32v3(0.0f));
    physMesh.addQuadBetweenPoints(&(pointsSide[4]));
}

f32 TileMeshBuilderMethods::getModelRotationAtPosition(const f32v3& worldPos) {
    return Random::getCachedRandomfSpecific((ui32)(worldPos.x + worldPos.y * 1000.0f)) * M_2_PI;
}

f32v2 TileMeshBuilderMethods::getStructureWoobleAtPoint(const ui32v3& xyz) {
    return getStructureWoobleAtPoint(xyz.x, xyz.y, xyz.z);
}

f32v2 TileMeshBuilderMethods::getStructureWoobleAtPoint(ui32 x, ui32 y, ui32 z) {
    // No wooble on the bottom layer
    if (z != 0 && Random::getCachedRandomfSpecific(x | (y << 3) + z * 1523u) <= sDebugOptions.mWallWoobleChance) {
        f32v2 outWooble = f32v2(Random::getThreadSafef(x, y + z * 1200u), Random::getThreadSafef(y - z * 1200u, x));
        // Scale -1 to 1
        outWooble = outWooble * 2.0f - 1.0f;
        // Get rid of small woobles by scaling to [-1, -0.5) [0.5, 1]
        outWooble.x = (outWooble.x + 1.0f) * 0.5f - (f32)(outWooble.x < 0.0f);
        outWooble.y = (outWooble.y + 1.0f) * 0.5f - (f32)(outWooble.y < 0.0f);
        outWooble *= sDebugOptions.mWallWoobleIntensity;
        return outWooble;
    }
    return f32v2(0.0f);
}

//
//f32 ChunkMesher::getTileHeight(TileFloor floor, const Tile& neighbor, const f32* heightData, TilePosition tilePos) {
//    f32 height = 0.0f;
//    const TileID tileId = neighbor.getLayersThreadSafe(TILE_FLOOR_GROUND)[TILE_LAYER_GROUND];
//    if (tileId != TILE_ID_NONE) {
//        //const TileData& tileData = TileRepository::getTileData(tileId);
//        height = neighbor.getGroundZPositionUncompressedThreadSafe(floor);
//        // Transparent tiles do not count
//      /*  if (!(spriteData.flags & SPRITEDATA_FLAG_TRANSPARENT)) {
//            height = neighbor.getGroundZPositionUncompressedThreadSafe();
//        }*/
//    }
//    return glm::max(height, mWorldGrid.computeMinHeightAtTile(heightData, tilePos));
//}
//
//f32 ChunkMesher::getTileHeight(TileFloor floor, const TileHandle& neighbor) {
//    const Tile& tile = *neighbor.tile;
//    f32 height = 0.0f;
//    const TileID tileId = tile.getLayersThreadSafe(TILE_FLOOR_GROUND)[TILE_LAYER_GROUND];
//    if (tileId != TILE_ID_NONE) {
//        /* const TileData& tileData = TileRepository::getTileData(tileId);
//         const SpriteData& spriteData = tileData.spriteData;*/
//        height = tile.getGroundZPositionUncompressedThreadSafe(floor);
//        // Transparent tiles appear to be 1 tile lower
//        /*if (!(spriteData.flags & SPRITEDATA_FLAG_TRANSPARENT)) {
//            height = tile.getGroundZPositionUncompressedThreadSafe();
//        }*/
//    }
//    return glm::max(height, mWorldGrid.computeMinHeightAtTile(TilePosition(neighbor.chunk->getChunkID(), neighbor.index)));
//}
