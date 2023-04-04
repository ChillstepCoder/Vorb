#include "stdafx.h"
#include "ProceduralMeshHelpers.h"

void ProceduralMeshHelpers::addTileWallMesh(Cartesian dir, const f32v3& tilePos, f32 wallHeight, ProceduralMeshBuilder& meshBuilder, StaticPhysicsMeshBuilder& physMesh, bool adjNegative, bool adjPositive)
{
    //f32v3 wallPoints[4];
    //f32v3 encapPointsStart[4];
    //f32v3 encapPointsEnd[4];
    //bool flipUv = false;
    //bool endcapStart = false;
    //bool endcapEnd = false;
    //float flipV = 1.0f;
    //switch (dir) {
    //    case Cartesian::SOUTH: {
    //        wallPoints[0] = tilePos;
    //        wallPoints[1] = tilePos + f32v3(1.0f, 0.0f);
    //        wallPoints[2] = tilePos + f32v3(1.0f, wallHeight);
    //        wallPoints[3] = tilePos + f32v3(0.0f, wallHeight);
    //        // Endcaps 
    //        // TODO: Endcaps im pretty sure can use lookup array. Maybe walldirs too..
    //        if (wall.isPrimary) {
    //            if (wall.cornerTypeStart != WallCornerType::NONE) {
    //                encapPointsStart[0] = wallPoints[0];
    //                encapPointsStart[1] = wallPoints[3];
    //                encapPointsStart[2] = wallPoints[3];
    //                encapPointsStart[2].y += WALL_THICKNESS_PLUS_EPSILON;
    //                encapPointsStart[3] = wallPoints[0];
    //                encapPointsStart[3].y += WALL_THICKNESS_PLUS_EPSILON;
    //                endcapStart = true;
    //            }
    //            if (wall.cornerTypeEnd != WallCornerType::NONE) {
    //                encapPointsEnd[0] = wallPoints[2];
    //                encapPointsEnd[1] = wallPoints[1];
    //                encapPointsEnd[2] = wallPoints[1];
    //                encapPointsEnd[2].y += WALL_THICKNESS_PLUS_EPSILON;
    //                encapPointsEnd[3] = wallPoints[2];
    //                encapPointsEnd[3].y += WALL_THICKNESS_PLUS_EPSILON;
    //                endcapEnd = true;
    //            }
    //        }
    //        break;
    //    }
    //    case Cartesian::WEST: {
    //        wallPoints[0] = tilePos + f32v3(wall.vertWoobleEndBottom.x, wall.length + wall.vertWoobleEndBottom.y, 0.0f);
    //        wallPoints[1] = tilePos + f32v3(wall.vertWoobleStartBottom.x, wall.vertWoobleStartBottom.y, 0.0f);
    //        wallPoints[2] = tilePos + f32v3(wall.vertWoobleStartTop.x, wall.vertWoobleStartTop.y, wallHeight);
    //        wallPoints[3] = tilePos + f32v3(wall.vertWoobleEndTop.x, wall.length + wall.vertWoobleEndTop.y, wallHeight);
    //        if (wall.isPrimary) {
    //            if (wall.cornerTypeStart != WallCornerType::NONE) {
    //                encapPointsStart[0] = wallPoints[0];
    //                encapPointsStart[1] = wallPoints[3];
    //                encapPointsStart[2] = wallPoints[3];
    //                encapPointsStart[2].x += WALL_THICKNESS_PLUS_EPSILON;
    //                encapPointsStart[3] = wallPoints[0];
    //                encapPointsStart[3].x += WALL_THICKNESS_PLUS_EPSILON;
    //                endcapStart = true;
    //            }
    //            if (wall.cornerTypeEnd != WallCornerType::NONE) {
    //                encapPointsEnd[0] = wallPoints[2];
    //                encapPointsEnd[1] = wallPoints[1];
    //                encapPointsEnd[2] = wallPoints[1];
    //                encapPointsEnd[2].x += WALL_THICKNESS_PLUS_EPSILON;
    //                encapPointsEnd[3] = wallPoints[2];
    //                encapPointsEnd[3].x += WALL_THICKNESS_PLUS_EPSILON;
    //                endcapEnd = true;
    //            }
    //        }
    //        break;
    //    }
    //    case Cartesian::EAST: {
    //        flipUv = true;
    //        flipV = -1.0f;
    //        wallPoints[0] = tilePos + f32v3(wall.vertWoobleStartTop.x, wall.vertWoobleStartTop.y, wallHeight);
    //        wallPoints[1] = tilePos + f32v3(wall.vertWoobleStartBottom.x, wall.vertWoobleStartBottom.y, 0.0f);
    //        wallPoints[2] = tilePos + f32v3(wall.vertWoobleEndBottom.x, wall.length + wall.vertWoobleEndBottom.y, 0.0f);
    //        wallPoints[3] = tilePos + f32v3(wall.vertWoobleEndTop.x, wall.length + wall.vertWoobleEndTop.y, wallHeight);
    //        if (wall.isPrimary) {
    //            if (wall.cornerTypeStart != WallCornerType::NONE) {
    //                encapPointsStart[0] = wallPoints[1];
    //                encapPointsStart[1] = wallPoints[0];
    //                encapPointsStart[2] = wallPoints[0];
    //                encapPointsStart[2].x -= WALL_THICKNESS_PLUS_EPSILON;
    //                encapPointsStart[3] = wallPoints[1];
    //                encapPointsStart[3].x -= WALL_THICKNESS_PLUS_EPSILON;
    //                endcapStart = true;
    //            }
    //            if (wall.cornerTypeEnd != WallCornerType::NONE) {
    //                encapPointsEnd[0] = wallPoints[3];
    //                encapPointsEnd[1] = wallPoints[2];
    //                encapPointsEnd[2] = wallPoints[2];
    //                encapPointsEnd[2].x -= WALL_THICKNESS_PLUS_EPSILON;
    //                encapPointsEnd[3] = wallPoints[3];
    //                encapPointsEnd[3].x -= WALL_THICKNESS_PLUS_EPSILON;
    //                endcapEnd = true;
    //            }
    //        }
    //        break;
    //    }
    //    case Cartesian::NORTH: {
    //        flipUv = true;
    //        flipV = -1.0f;
    //        wallPoints[0] = tilePos + f32v3(wall.length + wall.vertWoobleEndTop.x, wall.vertWoobleEndTop.y, wallHeight);
    //        wallPoints[1] = tilePos + f32v3(wall.length + wall.vertWoobleEndBottom.x, wall.vertWoobleEndBottom.y, 0.0f);
    //        wallPoints[2] = tilePos + f32v3(wall.vertWoobleStartBottom.x, wall.vertWoobleStartBottom.y, 0.0f);
    //        wallPoints[3] = tilePos + f32v3(wall.vertWoobleStartTop.x, wall.vertWoobleStartTop.y, wallHeight);
    //        if (wall.isPrimary) {
    //            if (wall.cornerTypeStart != WallCornerType::NONE) {
    //                encapPointsStart[0] = wallPoints[3];
    //                encapPointsStart[1] = wallPoints[2];
    //                encapPointsStart[2] = wallPoints[2];
    //                encapPointsStart[2].y -= WALL_THICKNESS_PLUS_EPSILON;
    //                encapPointsStart[3] = wallPoints[3];
    //                encapPointsStart[3].y -= WALL_THICKNESS_PLUS_EPSILON;
    //                endcapStart = true;
    //            }
    //            if (wall.cornerTypeEnd != WallCornerType::NONE) {
    //                encapPointsEnd[0] = wallPoints[1];
    //                encapPointsEnd[1] = wallPoints[0];
    //                encapPointsEnd[2] = wallPoints[0];
    //                encapPointsEnd[2].y -= WALL_THICKNESS_PLUS_EPSILON;
    //                encapPointsEnd[3] = wallPoints[1];
    //                encapPointsEnd[3].y -= WALL_THICKNESS_PLUS_EPSILON;
    //                endcapEnd = true;
    //            }
    //        }
    //        break;
    //    }
    //    default:
    //        assert(false);
    //        break;

    //}
    //// Main faces
    //const TileData& tileData = TileRepository::getTileData(wall.tileId);
    //const MaterialData& materialData = tileData.materialData;
    //const f32 UVSCALE_Y = flipV / 3.0f;
    //meshBuilder.addQuadBetweenPoints(wallPoints, materialData, f32v2(1.0f, UVSCALE_Y), COLOR_WHITE, flipUv);
    //physMesh.addQuadBetweenPoints(wallPoints);
    //if (endcapStart) {
    //    meshBuilder.addQuadBetweenPoints(encapPointsStart, materialData, f32v2(1.0f, UVSCALE_Y), COLOR_WHITE, flipUv);
    //    // The collision is thin enough here we just dont really need it
    //    /*if (physMesh) {
    //        physMesh.addQuadBetweenPoints(encapPointsStart);
    //    }*/
    //}
    //if (endcapEnd) {
    //    meshBuilder.addQuadBetweenPoints(encapPointsEnd, materialData, f32v2(1.0f, UVSCALE_Y), COLOR_WHITE, flipUv);
    //    // The collision is thin enough here we just dont really need it
    //    /*if (physMesh) {
    //        physMesh.addQuadBetweenPoints(encapPointsStart);
    //    }*/
    //}
}
