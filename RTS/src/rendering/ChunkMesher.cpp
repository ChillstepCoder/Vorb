#include "stdafx.h"
#include "ChunkMesher.h"

#include "world/Chunk.h"
#include "resources/TileRepository.h"
#include "rendering/mesh/Mesh.h"
#include "rendering/mesh/MeshBuilder.h"
#include "rendering/mesh/BillboardMeshBuilder.h"
#include "rendering/mesh/TileMeshBuilderMethods.h"
#include "rendering/QuadMesh.h"
#include "Random.h"
#include "options/DebugOptions.h"
#include <Vorb/graphics/SamplerState.h>

#include "world/WorldGrid.h"

// For grass noise
#include "generation/WorldGeneration.h"

// TODO THIS IS TMP
#include "ResourceManager.h"
#include "resources/TextureRepository.h"


//ui8 getCornerIndex(ExposedNeighbor8Bits cornerBit) {
//    switch (cornerBit) {
//        case EN8_BOTTOM_LEFT:
//            return 0x1;
//        case EN8_BOTTOM_RIGHT:
//            return 0x2;
//        case EN8_TOP_LEFT:
//            return 0x3;
//        case EN8_TOP_RIGHT:
//            return 0x4;
//        default:
//            assert(false);
//    }
//    return 0;
//}
//
//bool checkLShape(int i, int lBits, ui32 lIndex, ExposedNeighbor8Bits cornerBit) {
//    if (areBitsSet(i, lBits)) {
//        sConnectedWallData[i].a = lIndex;
//        if (isBitSet(i, cornerBit)) {
//            sConnectedWallData[i].b = getCornerIndex(cornerBit);
//        }
//        return true;
//    }
//    return false;
//}

//inline bool checkIShape(int i, ExposedNeighbor8Bits iBit, ui32 iIndex, ExposedNeighbor8Bits cornerBit1, ExposedNeighbor8Bits cornerBit2, ExposedNeighbor8Bits oppositeBit, ui32 oppositeIndex) {
//    if (isBitSet(i, iBit)) {
//        sConnectedWallData[i].a = iIndex;
//        if (isBitSet(i, oppositeBit)) {
//            sConnectedWallData[i].b = oppositeIndex;
//        }
//        else {
//            if (isBitSet(i, cornerBit1)) {
//                sConnectedWallData[i].b = getCornerIndex(cornerBit1);
//                if (isBitSet(i, cornerBit2)) {
//                    sConnectedWallData[i].c = getCornerIndex(cornerBit2);
//                }
//            }
//            else if (isBitSet(i, cornerBit2)) {
//                sConnectedWallData[i].b = getCornerIndex(cornerBit2);
//            }
//        }
//        return true;
//    }
//    return false;
//}


// Hex map
//  0  1  2  3  4  5
//  6  7  8  9  a  b
//  c  d  e  f 10 11
// 12 13 14 15 16 17
// Cached after running once
//void initConnectedOffsets() {
//    // 1 bits represent exposed faces
//    for (int i = 0; i < 256; ++i) {
//        sConnectedWallData[i].data = 0;
//
//        // Standalone
//        if (areBitsSet(i, EN8_LEFT | EN8_TOP | EN8_RIGHT | EN8_BOTTOM)) {
//            // No connections
//            sConnectedWallData[i].a = 0x11;
//            continue;
//        }
//
//        // U shapes
//        if (areBitsSet(i, EN8_LEFT | EN8_TOP | EN8_RIGHT)) {
//            sConnectedWallData[i].a = 0x10;
//            continue;
//        }
//        if (areBitsSet(i, EN8_BOTTOM | EN8_TOP | EN8_RIGHT)) {
//            sConnectedWallData[i].a = 0xf;
//            continue;
//        }
//        if (areBitsSet(i, EN8_BOTTOM | EN8_TOP | EN8_LEFT)) {
//            sConnectedWallData[i].a = 0xe;
//            continue;
//        }
//        if (areBitsSet(i, EN8_LEFT | EN8_BOTTOM | EN8_RIGHT)) {
//            sConnectedWallData[i].a = 0xd;
//            continue;
//        }
//
//        // L shapes
//        if (checkLShape(i, EN8_TOP | EN8_RIGHT, 0xc, EN8_BOTTOM_LEFT)) {
//            continue;
//        }
//        if (checkLShape(i, EN8_TOP | EN8_LEFT, 0xb, EN8_BOTTOM_RIGHT)) {
//            continue;
//        }
//        if (checkLShape(i, EN8_BOTTOM | EN8_RIGHT, 0xa, EN8_TOP_LEFT)) {
//            continue;
//        }
//        if (checkLShape(i, EN8_BOTTOM | EN8_LEFT, 0x9, EN8_TOP_RIGHT)) {
//            continue;
//        }
//
//        // I shapes
//        if (checkIShape(i, EN8_TOP, 0x8, EN8_BOTTOM_LEFT, EN8_BOTTOM_RIGHT, EN8_BOTTOM, 0x5)) {
//            continue;
//        }
//        if (checkIShape(i, EN8_RIGHT, 0x7, EN8_BOTTOM_LEFT, EN8_TOP_LEFT, EN8_LEFT, 0x6)) {
//            continue;
//        }
//        if (checkIShape(i, EN8_LEFT, 0x6, EN8_BOTTOM_RIGHT, EN8_TOP_RIGHT, EN8_RIGHT, 0x7)) {
//            continue;
//        }
//        if (checkIShape(i, EN8_BOTTOM, 0x5, EN8_TOP_LEFT, EN8_TOP_RIGHT, EN8_TOP, 0x8)) {
//            continue;
//        }
//
//        // Finally, corners
//        int j = 0;
//        if (isBitSet(i, EN8_BOTTOM_LEFT)) {
//            sConnectedWallData[i].dataArray[j++] = getCornerIndex(EN8_BOTTOM_LEFT);
//        }
//        if (isBitSet(i, EN8_BOTTOM_RIGHT)) {
//            sConnectedWallData[i].dataArray[j++] = getCornerIndex(EN8_BOTTOM_RIGHT);
//        }
//        if (isBitSet(i, EN8_TOP_LEFT)) {
//            sConnectedWallData[i].dataArray[j++] = getCornerIndex(EN8_TOP_LEFT);
//        }
//        if (isBitSet(i, EN8_TOP_RIGHT)) {
//            sConnectedWallData[i].dataArray[j++] = getCornerIndex(EN8_TOP_RIGHT);
//        }
//    }
//}

ChunkMesher::ChunkMesher(const WorldGrid& worldGrid) :
    mWorldGrid(worldGrid)
{

}

ChunkMesher::~ChunkMesher()
{

}

void ChunkMesher::updateMesh(const Chunk& chunk, const f32v3& cameraPos) {
    UNUSED(cameraPos);
    if (chunk.mTileContainer->shouldBuildStaticMesh()) {
        createMeshAsync(chunk);
    }
}

bool ChunkMesher::createMeshAsync(const Chunk& chunk) {

    ChunkRenderData& chunkRenderData = chunk.mChunkRenderData;
    TileContainerRenderData& tileRenderData = chunk.getTileContainer()->getRenderData();
    assert(!tileRenderData.mIsBuildingStaticMesh);
    tileRenderData.mIsBuildingStaticMesh = true;

    // TODO: Move somewhere else?
    chunk.getTileContainer()->setDirtyStaticMesh(false);
    chunk.incReadLockAndRefCountNeighbors4AndSelf();

    // TODO: Do we need to do this?
    if (!chunkRenderData.mBillboardMesh) {
        chunkRenderData.mBillboardMesh = std::make_unique<Mesh>();
    }
    if (!tileRenderData.mStaticMesh) {
        tileRenderData.mStaticMesh = std::make_unique<Mesh>();
    }

    const HeightmapPatchData* heightData = mWorldGrid.getHeightDataAt(chunk.getHeightmapPatchID());
    
    // TODO: Different way than using two shared ptr? Does it matter?
    std::shared_ptr<MeshBuilder> quadMeshBuilder = std::make_shared<MeshBuilder>(true);
    std::shared_ptr<BillboardMeshBuilder> billboardMeshBuilder = std::make_shared<BillboardMeshBuilder>();

    Services::Threadpool::ref().addTask([this, &chunk, heightData, quadMeshBuilder, billboardMeshBuilder](ThreadPoolWorkerData*) {

        quadMeshBuilder->reserveVertexCount(CHUNK_SIZE * 4); // Most chunks will have less than 1 quad per tile
        billboardMeshBuilder->reserveBillboardCount(CHUNK_SIZE / 2); // Most chunks will have less than 0.5 billboards per tile

        for (int y = 0; y < CHUNK_WIDTH; ++y) {
            for (int x = 0; x < CHUNK_WIDTH; ++x) {
                //  TODO: Multiple world layers
                TileIndex index = chunk.getTileContainer()->getTileIndexFromXYZOffset(x, y, 0);
                const Tile& tile = chunk.getTileContainer()->getTileAt(index);
                const f32 groundZPosition = tile.getGroundZPositionUncompressedThreadSafe();
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
                        // Billboards
                        if (tileData.textureMethod == TileTextureMethod::FLORA) {
                            /*f32v3 tilePosition(x + 0.5f, y + 0.5f, tile.groundZPosition);
                            Tile rightTile = chunk.getRightTileHandle(index).tile;
                            Tile topTile = chunk.getTopTileHandle(index).tile;
                            addTileFloraBillboard(billboardMesh, chunk, index, layerIndex, tileData, spriteData, rightTile, topTile);*/
                            continue;
                        }
                        else {
                            f32 zPosition = glm::max(groundZPosition, mWorldGrid.computeCenterHeightAtTile(chunk.getChunkID().getWorldPosInt() + ui32v2(x, y)));
                            f32v3 tilePosition(x + 0.5f, y + 0.5f, zPosition);
                            billboardMeshBuilder->addBillboard(tilePosition, tileData.dims, texture);
                        }
                    }
                    else if (tileData.shape == TileShape::BLOCK) {
                        TileMeshBuilderMethods::addBlock(*quadMeshBuilder, f32v3(x, y, 0.0f /*TODO REAL FLOOR HEIGHT*/), TileHandle(chunk.getTileContainer(), index), tileData, nullptr);
                    }
                    else if (tileData.shape == TileShape::FLOOR) {
                            
                        //TileMeshBuilderMethods::addFloor(*quadMeshBuilder, (TileFloor)floor, f32v2(x, y), heightData, tileData, index, chunk, floor == TILE_FLOOR_GROUND);
                    }
                }
            }
        }

        // No longer need read access
        chunk.decReadLockNeighbors4();
        chunk.decReadLock();
        chunk.decRefNeighbors4();
    }, [this, &chunk, quadMeshBuilder, billboardMeshBuilder]() {

        ChunkRenderData& chunkRenderData = chunk.mChunkRenderData;
        TileContainerRenderData& tileRenderData = chunk.mTileContainer->getRenderData();

        // Upload mesh buffers
        quadMeshBuilder->finishMesh(*tileRenderData.mStaticMesh, MeshDrawMode::STATIC);
        billboardMeshBuilder->finishMesh(*chunkRenderData.mBillboardMesh, MeshDrawMode::STATIC);

        // Flag as free
        tileRenderData.mIsBuildingStaticMesh = false;

        // No longer need to exist
        chunk.decRef();
    });
    return true;
}
